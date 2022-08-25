#define NRF_LOG_MODULE_NAME cfg
#include "prelude.hh"
#include "cfg.hh"
#include "fds.h"
NRF_LOG_MODULE_REGISTER();

using namespace cfg;

ret_code_t cfg::init()
{
    ret_code_t ret;

    ret = cfg::init_flash_backend();
    VERIFY_SUCCESS(ret);

    return ret;
}
