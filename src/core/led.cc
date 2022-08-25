#define NRF_LOG_MODULE_NAME led
#include "prelude.hh"
#include "led.hh"
#include "ble/led.hh"
NRF_LOG_MODULE_REGISTER();

using namespace led;

void led::reset_all()
{
    service_0().reset();
#if MAX_LED_CHANNELS >= 2
    service_1().reset();
#endif
#if MAX_LED_CHANNELS >= 3
    service_2().reset();
#endif
#if MAX_LED_CHANNELS >= 4
    service_3().reset();
#endif
}
