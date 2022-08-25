#include "prelude.hh"
#include "bootloader.hh"
#include "ble.hh"
#include "nrf_log.h"
#include "ble_dfu.h"
#include "nrf_sdh.h"
#include "nrf_pwr_mgmt.h"
#include "ble_advertising.h"
#include "ble_conn_state.h"
#include "nrf_bootloader_info.h"
#include "nrf_power.h"

static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event);

ret_code_t dfu::init_ble_service()
{
    ble_dfu_buttonless_init_t init = {
        .evt_handler = ble_dfu_evt_handler
    };

    return ble_dfu_buttonless_init(&init);
}

// YOUR_JOB: Update this code if you want to do anything given a DFU event (optional).
/**@brief Function for handling dfu events from the Buttonless Secure DFU service
 *
 * @param[in]   event   Event from the Buttonless Secure DFU service.
 */
static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
    switch (event)
    {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
        {
            NRF_LOG_INFO("Device is preparing to enter bootloader mode.");

            // Prevent device from advertising on disconnect.
            ble_adv_modes_config_t config = {};
            ble::get_advertising_config(config);
            config.ble_adv_on_disconnect_disabled = true;
            ble::set_advertising_config(config);

            // Disconnect all other bonded devices that currently are connected.
            // This is required to receive a service changed indication
            // on bootup after a successful (or aborted) Device Firmware Update.
            const auto disconnect = [](uint16_t conn_handle, void *context) {
                unused(context);
                ble::disconnect(conn_handle, ble::disconnect_reason::remote_user_terminated_connection);
            };

            uint32_t conn_count = ble_conn_state_for_each_connected(disconnect, NULL);
            NRF_LOG_INFO("Disconnected %d links.", conn_count);
            break;
        }

        case BLE_DFU_EVT_BOOTLOADER_ENTER:
            // YOUR_JOB: Write app-specific unwritten data to FLASH, control finalization of this
            //           by delaying reset by reporting false in app_shutdown_handler
            NRF_LOG_INFO("Device will enter bootloader mode.");
            break;

        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
            NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
            // YOUR_JOB: Take corrective measures to resolve the issue
            //           like calling APP_ERROR_CHECK to reset the device.
            break;

        case BLE_DFU_EVT_RESPONSE_SEND_ERROR:
            NRF_LOG_ERROR("Request to send a response to client failed.");
            // YOUR_JOB: Take corrective measures to resolve the issue
            //           like calling APP_ERROR_CHECK to reset the device.
            APP_ERROR_CHECK(false);
            break;

        default:
            NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
            break;
    }
}

static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void *p_context) {
    if (state == NRF_SDH_EVT_STATE_DISABLED) {
        // Softdevice was disabled before going into reset. Inform bootloader to skip CRC on next boot.
        nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);

        //Go to system off.
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

/* nrf_sdh state observer. */
NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) = {
        .handler = buttonless_dfu_sdh_state_observer,
};
