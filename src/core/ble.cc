#define NRF_LOG_MODULE_NAME ble
#include "prelude.hh"
#include "ble.hh"
#include "meta.hh"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_sdh.h"
#include "nrf_sdh_freertos.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_dis.h"
#include "ble_gap.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "bsp_btn_ble.h"
NRF_LOG_MODULE_REGISTER();

#define ATT_MTU 128
#define BLE_DATA_LEN 132

#define BLE_BASE_UUID {0x2a, 0x27, 0xdd, 0x37, 0x04, 0x0e, 0x47, 0x46, \
                       0xae, 0x13, 0x35, 0xe0, 0x06, 0xfd, 0x77, 0xa2}

#define APP_BLE_OBSERVER_PRIO 3
#define APP_BLE_CONN_CFG_TAG 1

#define APP_ADV_INTERVAL MSEC_TO_UNITS( 500, UNIT_0_625_MS) /** Advertising interval */
#define APP_ADV_DURATION MSEC_TO_UNITS(4000, UNIT_10_MS)    /** Advertising duration */

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(120, UNIT_1_25_MS)  /** Minimum acceptable connection interval */
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(240, UNIT_1_25_MS)  /** Maximum acceptable connection interval */
#define SLAVE_LATENCY 0                                     /** Slave latency. */
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS)    /** Connection supervisory time-out */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  5000                /** Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   30000               /** Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                   /** Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND              1                       /** Perform bonding. */
#define SEC_PARAM_MITM              0                       /** Man In The Middle protection not required. */
#if defined(USE_LESC) && USE_LESC
#define SEC_PARAM_LESC              1                       /** LE Secure Connections not enabled. */
#else
#define SEC_PARAM_LESC              0
#endif /* defined(USE_LESC) && USE_LESC */
#define SEC_PARAM_KEYPRESS          0                       /** Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES   BLE_GAP_IO_CAPS_NONE    /** No I/O capabilities. */
#define SEC_PARAM_OOB               0                       /** Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE      7                       /** Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE      16                      /** Maximum encryption key size. */

NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);
BLE_ADVERTISING_DEF(m_advertising);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static uint8_t m_manuf_adv_data[16] = {
    0x41, 0x98
};

// static constexpr size_t m_meta_adv_index = 0;
static ble_uuid_t m_adv_uuids[] = {
    // {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
    // {(uint16_t)ble::service_uuid::meta,   BLE_UUID_TYPE_UNKNOWN}
};

static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
static ret_code_t gap_params_init(void);
static ret_code_t gatt_init(void);
static void nrf_qwr_error_handler(uint32_t nrf_error);
static ret_code_t advertising_init(void);
static ret_code_t qwr_init(void);
static ret_code_t dis_init(void);
static void advertising_start(void * p_erase_bonds);
static ret_code_t conn_params_init(void);
static ret_code_t peer_manager_init(void);
static void pm_evt_handler(pm_evt_t const * p_evt);
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt);
static void conn_params_error_handler(uint32_t nrf_error);
static void set_advdata(void const *manuf, size_t manuf_len, ble_advdata_t *advdata, ble_advdata_manuf_data_t *manuf_data);

ret_code_t ble::init()
{
    static bool initialized = false;

    if (initialized) {
        return NRF_SUCCESS;
    }

    initialized = true;

    ret_code_t ret;

    ret = nrf_sdh_enable_request();
    VERIFY_SUCCESS(ret);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    ret = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    VERIFY_SUCCESS(ret);

    // Enable BLE stack.
    ret = nrf_sdh_ble_enable(&ram_start);
    VERIFY_SUCCESS(ret);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

    ret = gap_params_init();
    VERIFY_SUCCESS(ret);

    ret = gatt_init();
    VERIFY_SUCCESS(ret);

    ret = advertising_init();
    VERIFY_SUCCESS(ret);

    ret = qwr_init();
    VERIFY_SUCCESS(ret);

    ret = dis_init();
    VERIFY_SUCCESS(ret);

    ret = conn_params_init();
    VERIFY_SUCCESS(ret);

    ret = peer_manager_init();
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t ble::init_thread(bool erase_bonds)
{
    static bool do_erase_bonds;
    do_erase_bonds = erase_bonds;
    nrf_sdh_freertos_init(advertising_start, &do_erase_bonds);
    return NRF_SUCCESS;
}

void ble::erase_bonds()
{
    ret_code_t ret = pm_peers_delete();
    NRF_LOG_DEBUG("pm_peers_delete: %u", ret);
    APP_ERROR_CHECK(ret);
}

uint16_t ble::conn_handle()
{
    return m_conn_handle;
}

void ble::get_advertising_config(ble_adv_modes_config_t &config)
{
    config.ble_adv_fast_enabled  = true;
    config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    config.ble_adv_fast_timeout  = APP_ADV_DURATION;
}

void ble::set_advertising_config(ble_adv_modes_config_t &config)
{
    ble_advertising_modes_config_set(&m_advertising, &config);
}

ret_code_t ble::set_advertising_data(void const *data, size_t len)
{
    ble_advdata_t advdata = {};
    ble_advdata_manuf_data_t manuf_data = {};
    
    set_advdata(data, len, &advdata, &manuf_data);

    return ble_advertising_advdata_update(&m_advertising, &advdata, nullptr);
}

void ble::disconnect(uint16_t conn_handle, ble::disconnect_reason reason)
{
    ret_code_t err_code = sd_ble_gap_disconnect(conn_handle, (uint8_t)reason);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Disconnect failed (conn_handle=%d err_code=%d)", conn_handle, err_code);
    }
}

uint8_t ble::uuid_type()
{
    static uint8_t result = BLE_UUID_TYPE_UNKNOWN;

    ret_code_t ret;

    if (result == BLE_UUID_TYPE_UNKNOWN) {
        NRF_LOG_INFO("Registering vendor-specific UUID");
        ble_uuid128_t uuid = {BLE_BASE_UUID};
        ret = sd_ble_uuid_vs_add(&uuid, &result);
        APP_ERROR_CHECK(ret);
    }

    return result;
}

size_t ble::max_att_data_len()
{
    return ATT_MTU - 3;
}

static void set_advdata(void const *manuf, size_t manuf_len, ble_advdata_t *advdata, ble_advdata_manuf_data_t *manuf_data)
{
    advdata->name_type               = BLE_ADVDATA_FULL_NAME;
    advdata->include_appearance      = true;
    advdata->flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata->uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata->uuids_complete.p_uuids  = m_adv_uuids;
    advdata->p_manuf_specific_data   = manuf_data;

    manuf_data->company_identifier = BLE_MANUF_ID;
    if (manuf)
        memcpy(&m_manuf_adv_data[2], manuf, std::min(manuf_len, sizeof(m_manuf_adv_data) - 2));

    manuf_data->data = {
        .size = (uint16_t)(2 + manuf_len),
        .p_data = m_manuf_adv_data
    };
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t ret;

    pm_handler_secure_on_connection(p_ble_evt);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            ret = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(ret);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            ret = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(ret);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            NRF_LOG_DEBUG("PHY update request");
            ble_gap_phys_t const phys =
            {
                .tx_phys = BLE_GAP_PHY_AUTO,
                .rx_phys = BLE_GAP_PHY_AUTO,
            };
            ret = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(ret);
        }   break;

        case BLE_GAP_EVT_PHY_UPDATE: {
            auto params = &p_ble_evt->evt.gap_evt.params.phy_update;
            NRF_LOG_INFO("PHY update (status=%u) RX=%u TX=%u",
                (uint32_t)params->status,
                (uint32_t)params->rx_phy,
                (uint32_t)params->tx_phy);
        }   break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout");
            ret = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(ret);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout");
            ret = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(ret);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            NRF_LOG_INFO("BLE_GAP_EVT_SEC_PARAMS_REQUEST");
            break;

        default:
            // No implementation needed.
            break;
    }
}

static ret_code_t gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (uint8_t*)meta::device_name(),
                                          meta::device_name_len());
    VERIFY_SUCCESS(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
    VERIFY_SUCCESS(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    VERIFY_SUCCESS(err_code);

    return err_code;
}

static ret_code_t gatt_init(void)
{
    ret_code_t ret;
    
    ret = nrf_ble_gatt_init(&m_gatt, NULL);
    VERIFY_SUCCESS(ret);

    ret = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, ATT_MTU);
    VERIFY_SUCCESS(ret);

    ret = nrf_ble_gatt_data_length_set(&m_gatt, BLE_CONN_HANDLE_INVALID, BLE_DATA_LEN);
    VERIFY_SUCCESS(ret);

    return ret;
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static ret_code_t advertising_init()
{
    ret_code_t             err_code;
    ble_advertising_init_t init = {};
    ble_advdata_manuf_data_t manuf_data = {};

    memset(&init, 0, sizeof(init));

    set_advdata(nullptr, 0, &init.advdata, &manuf_data);

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    VERIFY_SUCCESS(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);

    return err_code;
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_DEBUG("Advertising");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}

static ret_code_t qwr_init(void)
{
    nrf_ble_qwr_init_t init = {
        .error_handler = nrf_qwr_error_handler
    };

    return nrf_ble_qwr_init(&m_qwr, &init);
}

static ret_code_t dis_init(void)
{
    ble_dis_init_t init = {
        .dis_char_rd_sec = SEC_OPEN,
    };

    ble_srv_ascii_to_utf8(&init.manufact_name_str, (char*)meta::mfg_name());
    ble_srv_ascii_to_utf8(&init.serial_num_str, (char*)meta::device_uid_str());

    return ble_dis_init(&init);
}

static void advertising_start(void * p_erase_bonds)
{
    bool erase_bonds = p_erase_bonds ? *(bool*)p_erase_bonds : false;

    if (erase_bonds)
    {
        ble::erase_bonds();
        *(bool*)p_erase_bonds = false;
    }
    else
    {
        ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for initializing the Connection Parameters module. */
static ret_code_t conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init = {};

    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = on_conn_params_evt;
    cp_init.error_handler = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    VERIFY_SUCCESS(err_code);

    return err_code;
}

static ret_code_t peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    VERIFY_SUCCESS(err_code);

    err_code = pm_register(pm_evt_handler);
    VERIFY_SUCCESS(err_code);

    return err_code;
}

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    bool delete_bonds = false;

    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            advertising_start(&delete_bonds);
            break;

        default:
            break;
    }
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in]   p_evt   Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
