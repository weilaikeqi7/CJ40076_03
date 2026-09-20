#include "app_key.h"
#include "dev_key.h"
#include <assert.h>
#include <stdio.h>

static bool power_down, mode_down;
void dev_key_init(void) {}
bool dev_key_raw_power_pressed(void) { return power_down; }
bool dev_key_raw_mode_pressed(void) { return mode_down; }
static unsigned scan(unsigned count)
{
    unsigned events = 0;
    while (count--) events |= app_key_scan().evt;
    return events;
}
int main(void)
{
    unsigned events;
    app_key_init();
    scan(10);
    app_key_set_calib_mode(true);
    power_down = mode_down = true;
    events = scan(130);
    assert(events == APP_KEY_EVT_BOTH_LONG);
    app_key_set_calib_mode(false); /* HEr/HIt save while still holding both */
    assert(scan(100) == APP_KEY_EVT_NONE);
    power_down = false;
    assert(scan(20) == APP_KEY_EVT_NONE);
    mode_down = false;
    assert(scan(100) == APP_KEY_EVT_NONE);
    power_down = true;
    scan(20);
    power_down = false;
    assert(scan(20) == APP_KEY_EVT_POWER_SHORT);
    app_key_set_calib_mode(true);
    power_down = mode_down = true;
    assert(scan(130) == APP_KEY_EVT_BOTH_LONG);
    mode_down = false; /* PIt->HIt staggered release must not adjust value */
    assert(scan(100) == APP_KEY_EVT_NONE);
    power_down = false;
    assert(scan(100) == APP_KEY_EVT_NONE);
    puts("PASS: chord consumed through both-key release, later short press preserved");
}
