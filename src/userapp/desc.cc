#include "prelude.hh"
#include "userapp.hh"

using namespace userapp;

ret_code_t desc::full_desc(desc_tbl &tbl) const
{
    ret_code_t ret;
    uint32_t magic = 0, ver = 0, arch = 0;

    ret = magic_number(magic);
    tbl.magic = magic;
    VERIFY_SUCCESS(ret);

    ret = version(ver);
    tbl.version = ver;
    VERIFY_SUCCESS(ret);

    ret = architecture(arch);
    VERIFY_SUCCESS(ret);

    if ((ver >> 16) == 1) {
        tbl.init = (userapp::init_func_t)buffer[2];
        tbl.refresh = (userapp::refresh_func_t)buffer[3];
        tbl.app_name = (char const *)buffer[4];
        tbl.provider_name = (char const *)buffer[5];
        tbl.app_id = (char const *)buffer[6];
    } else {
        unreachable();
    }

    return ret;
}
