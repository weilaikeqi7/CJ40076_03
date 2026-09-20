#include <assert.h>
#include <stdio.h>

#include "../../src/app/app.c"

int main(void)
{
    uint32_t bits;

    s_task_alive_bits = 0U;
    app_mark_key_alive();
    app_mark_sens_alive();
    app_mark_disp_alive();
    app_mark_pwr_alive();
    bits = app_take_alive_bits();
    assert(bits == TASK_ALIVE_ALL);
    assert(s_task_alive_bits == 0U);

    app_mark_key_alive();
    bits = app_take_alive_bits();
    assert(bits == TASK_ALIVE_BIT_KEY);
    assert(s_task_alive_bits == TASK_ALIVE_BIT_KEY);

    app_mark_sens_alive();
    app_mark_disp_alive();
    app_mark_pwr_alive();
    bits = app_take_alive_bits();
    assert(bits == TASK_ALIVE_ALL);
    assert(s_task_alive_bits == 0U);

    printf("runtime app heartbeat snapshot: PASS\n");
    return 0;
}
