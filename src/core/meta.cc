#define NRF_LOG_MODULE_NAME meta
#include "prelude.hh"
#include "meta.hh"
#include <string.h>
#include <inttypes.h>
NRF_LOG_MODULE_REGISTER();

#define DEVICE_NAME_FMT stringify(PRODUCT_PREFIX) "-%03lX-%03lX"
#define DEVICE_NAME_BUF_SIZE 32

static uint64_t m_device_uid = 0;
static char m_device_name[DEVICE_NAME_BUF_SIZE] = {};
static char m_device_uid_str[1 + (2 * sizeof(uint64_t))] = {};
static size_t m_device_name_len = 0;

extern const meta::build_id m_gnu_build_id;

void meta::init()
{
    static bool initialized = false;

    if (initialized) {
        return;
    }

    initialized = true;

    m_device_name_len = snprintf(
        m_device_name,
        DEVICE_NAME_BUF_SIZE,
        DEVICE_NAME_FMT,
        NRF_FICR->DEVICEID[0] & 0xfff,
        NRF_FICR->DEVICEID[1] & 0xfff
    );

    m_device_uid = NRF_FICR->DEVICEID[0];
    m_device_uid <<= 32;
    m_device_uid |= NRF_FICR->DEVICEID[1];

    snprintf(m_device_uid_str, sizeof(m_device_uid_str), "%08lX%08lX", NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1]);
}

size_t meta::device_name_len()
{
    return m_device_name_len;
}

char const *meta::device_name()
{
    return m_device_name;
}

size_t meta::mfg_name_len()
{
    return sizeof(MANUFACTURER_NAME) - 1;
}

char const *meta::mfg_name()
{
    return MANUFACTURER_NAME;
}

meta::rdm_id_t meta::device_rdm_uid()
{
    auto result = meta::rdm_id_t {};
    uint16_big_encode(RDM_MANUF_ID, result.bytes);
    uint32_encode(m_device_uid, &result.bytes[2]);
    return result;
}

char const *meta::device_uid_str()
{
    return m_device_uid_str;
}
