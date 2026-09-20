/* 使用真实测量状态机和测距协议解析器，模拟串口与任务抢占。
 * 验证请求取消、发送边界姿态快照、发布一致性、目标聚合及计时回绕。 */
#include "app_measure.h"
#include "app_config.h"
#include "dev_ranger.h"
#include "bsp_uart.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick;
static unsigned critical_depth, suspended, delays, tx_count;
static uint8_t tx_cmd[256];
static uint32_t tx_tick[256];
static uint8_t rx[4096];
static size_t rx_head, rx_tail;
static bool att_valid = true;
static int32_t heading = 1234, pitch = -456;
static void (*read_hook)(void);
static void (*suspend_hook)(void);

void test_enter_critical(void) { critical_depth++; }
void test_exit_critical(void) { assert(critical_depth); critical_depth--; }
uint32_t xTaskGetTickCount(void) { return tick; }
void vTaskDelay(uint32_t ticks)
{
    assert(!critical_depth && !suspended);
    delays++;
    tick += ticks;
}
void vTaskSuspendAll(void)
{
    /* 模拟高优先级按键任务在暂停调度前抢占，请求代号复查必须阻止
     * 已取消轮次继续发送指令。 */
    if (suspend_hook)
    {
        void (*hook)(void) = suspend_hook;
        suspend_hook = NULL;
        hook();
    }
    suspended++;
}
int xTaskResumeAll(void) { assert(suspended); suspended--; return 0; }
bool attitude_valid(void) { return att_valid; }
int32_t attitude_heading_c01(void) { return heading; }
int32_t attitude_pitch_c01(void) { return pitch; }
void bsp_pwr_ranger(bool on) { (void)on; }
void bsp_uart_init(bsp_uart_t port, uint32_t baud)
{
    assert(port == BSP_UART_RANGER && baud == 115200U);
}
void bsp_uart_flush_rx(bsp_uart_t port)
{
    assert(port == BSP_UART_RANGER);
    rx_head = rx_tail = 0U;
}
void bsp_uart_write(bsp_uart_t port, const void* bytes, size_t n)
{
    const uint8_t* p = bytes;
    uint8_t sum = 0U;
    assert(port == BSP_UART_RANGER && !critical_depth);
    assert(n >= 6U && n <= 10U && p[0] == 0xEE && p[1] == 0x16);
    for (size_t i = 3U; i < n - 1U; i++) sum = (uint8_t)(sum + p[i]);
    assert(sum == p[n - 1U]);
    if (tx_count) assert((uint32_t)(tick - tx_tick[tx_count - 1U]) >= 20U);
    assert(tx_count < sizeof(tx_cmd));
    tx_cmd[tx_count] = p[4];
    tx_tick[tx_count++] = tick;
}
size_t bsp_uart_read(bsp_uart_t port, void* out, size_t n)
{
    assert(port == BSP_UART_RANGER && n == 1U && !critical_depth);
    if (read_hook)
    {
        void (*hook)(void) = read_hook;
        read_hook = NULL;
        hook();
    }
    if (rx_tail == rx_head) return 0U;
    *(uint8_t*)out = rx[rx_tail++];
    return 1U;
}
static void append_range(uint8_t status, uint16_t meters)
{
    uint8_t frame[] = {0xEE, 0x16, 6, 3, 2, status,
        (uint8_t)(meters >> 8), (uint8_t)meters, 0, 0};
    for (size_t i = 3U; i < sizeof(frame) - 1U; i++)
        frame[9] = (uint8_t)(frame[9] + frame[i]);
    assert(rx_head + sizeof(frame) < sizeof(rx));
    memcpy(rx + rx_head, frame, sizeof(frame));
    rx_head += sizeof(frame);
}
static void poll_at(uint32_t when)
{
    unsigned before = delays;
    tick = when;
    measure_poll();
    assert(delays == before && !critical_depth && !suspended);
}
static void reset(meas_mode_t mode, uint32_t when)
{
    measure_set_mode(mode);
    poll_at(when);
    ranger_init();
    delays = 0U;
    tx_count = 0U;
    tick = when;
    att_valid = true;
    heading = 1234;
    pitch = -456;
    assert(!measure_is_running() && !measure_round_active());
}
static uint32_t start(uint32_t when)
{
    unsigned before = tx_count;
    measure_trigger();
    assert(measure_is_running() && measure_round_active());
    assert(tx_count == before); /* 按键请求不直接操作串口。 */
    poll_at(when);
    assert(tx_count == before + 1U && tx_cmd[before] == 3U);
    poll_at(when + 19U);
    assert(tx_count == before + 1U);
    poll_at(when + 20U);
    assert(tx_count == before + 2U && tx_cmd[before + 1U] == 2U);
    return measure_round_id();
}
static measure_result_t finish(uint32_t when, uint16_t meters)
{
    measure_result_t result;
    append_range(0U, meters);
    poll_at(when);
    poll_at(when + APP_MEASURE_SILENCE_MS - 1U);
    assert(!measure_take_result(&result));
    poll_at(when + APP_MEASURE_SILENCE_MS);
    assert(measure_take_result(&result));
    assert(!measure_take_result(&result));
    assert(result.near_valid && result.near_mm == (uint32_t)meters * 1000U);
    return result;
}
static void test_gap_stop_and_mode(void)
{
    reset(MEAS_MODE_SINGLE, 0U);
    measure_trigger();
    poll_at(0U);
    poll_at(19U); /* tick 为零也必须遵守指令间隔。 */
    assert(tx_count == 1U);
    measure_stop();
    assert(measure_round_active()); /* 传感器轮询后才确认停止。 */
    poll_at(20U);
    assert(tx_count == 1U && !measure_round_active());
    poll_at(4000U);
    assert(!measure_take_published());

    measure_trigger();
    poll_at(5000U);
    measure_set_mode(MEAS_MODE_TEST);
    poll_at(5020U);
    assert(tx_count == 2U && !measure_is_running());
    measure_trigger();
    measure_trigger(); /* 传感器处理前两次快速按键仍等价于启停一次。 */
    poll_at(5040U);
    assert(tx_count == 2U && !measure_is_running());
}
static void test_attitude_and_restart(void)
{
    reset(MEAS_MODE_MULTI, 100U);
    uint32_t old_id = start(100U);
    measure_trigger(); /* 测量中重触发，不产生活动标志上升沿。 */
    heading = 5000;
    poll_at(140U);
    heading = 6789;
    pitch = -123;
    poll_at(160U);
    uint32_t new_id = measure_round_id();
    assert(new_id != old_id);
    heading = 20000;
    pitch = 4000;
    measure_result_t result = finish(170U, 100U);
    assert(result.round_id == new_id && result.start_tick == 160U);
    assert(result.mode == MEAS_MODE_MULTI && result.attitude_valid);
    assert(result.heading_c01 == 6789 && result.pitch_c01 == -123);
    assert(!result.far_valid && measure_result_is_current(&result));
    measure_set_mode(MEAS_MODE_SINGLE);
    assert(!measure_result_is_current(&result));
    poll_at(400U);
    measure_copy_result(&result);
    assert(!result.near_valid && !result.far_valid);

    reset(MEAS_MODE_MULTI, 500U);
    att_valid = false;
    start(500U);
    att_valid = true;
    result = finish(530U, 120U);
    assert(!result.attitude_valid); /* 禁止回退到稍后的有效姿态。 */
}
static void test_preemption_and_stale_input(void)
{
    reset(MEAS_MODE_SINGLE, 1000U);
    measure_trigger();
    poll_at(1000U);
    suspend_hook = measure_stop;
    poll_at(1020U);
    assert(tx_count == 1U); /* final check closes check->send race */
    poll_at(1030U);
    assert(!measure_round_active());

    start(1100U);
    append_range(0, 999U);
    read_hook = measure_stop; /* stop while ranger drain/aggregation runs */
    poll_at(1130U);
    poll_at(1400U);
    assert(!measure_take_published());

    /* Leave a partial old frame in parser, then restart. */
    append_range(0, 888U);
    rx_head -= 4U;
    ranger_poll();
    start(1500U);
    measure_result_t result = finish(1530U, 42U);
    assert(result.near_mm == 42000U);

    /* Buffered old complete frame in the setup gap is discarded. */
    measure_trigger();
    poll_at(1800U);
    append_range(0, 777U);
    poll_at(1820U);
    result = finish(1830U, 43U);
    assert(result.near_mm == 43000U);
}
static void test_aggregation_and_timeout(void)
{
    reset(MEAS_MODE_SINGLE, 2000U);
    start(2000U);
    for (unsigned i = 0U; i < 16U; i++) append_range((uint8_t)(i << 4), (uint16_t)(10U + i));
    poll_at(2030U);
    poll_at(2230U);
    measure_result_t result;
    assert(measure_take_result(&result));
    assert(result.near_mm == 10000U && result.far_valid && result.far_mm == 25000U);
    assert(!measure_is_running());

    start(2300U);
    append_range(0x03U, 20U);
    append_range(0x13U, 20U);
    poll_at(2330U);
    poll_at(2530U);
    assert(measure_take_result(&result) && !result.far_valid);

    start(2600U);
    append_range(4U, UINT16_MAX);
    poll_at(2630U);
    assert(measure_take_result(&result) && !result.near_valid && !result.far_valid);

    start(2700U);
    poll_at(2720U + APP_MEASURE_TIMEOUT_MS - 1U);
    assert(!measure_take_result(&result));
    poll_at(2720U + APP_MEASURE_TIMEOUT_MS);
    assert(measure_take_result(&result) && !result.near_valid);
}
static void test_periods_and_stop_retains_result(void)
{
    for (unsigned test = 0; test < 2U; test++)
    {
        meas_mode_t mode = test ? MEAS_MODE_TEST : MEAS_MODE_CONT;
        uint32_t period = test ? APP_MEASURE_TEST_PERIOD_MS : APP_MEASURE_CONT_PERIOD_MS;
        reset(mode, 10000U);
        uint32_t first_id = start(10000U);
        measure_result_t result = finish(10030U, 15U);
        assert(result.mode == mode && measure_is_running() && !measure_round_active());
        poll_at(10020U + period - 1U);
        assert(tx_count == 2U);
        poll_at(10020U + period);
        assert(tx_count == 3U && measure_round_active());
        poll_at(10040U + period);
        assert(measure_round_id() != first_id);
        result = finish(10050U + period, 25U);
        /* Cancel the following round while waiting for its SINGLE command. */
        poll_at(10040U + 2U * period);
        assert(measure_round_active());
        unsigned commands_before_stop = tx_count;
        measure_trigger(); /* session toggle stop retains last result */
        poll_at(10050U + 2U * period);
        assert(tx_count == commands_before_stop);
        measure_copy_result(&result);
        assert(!measure_is_running() && result.near_valid && result.near_mm == 25000U);
        measure_stop(); /* explicit stop clears last result */
        poll_at(10310U + period);
        measure_copy_result(&result);
        assert(!result.near_valid);
    }
}
static void test_tick_wrap(void)
{
    reset(MEAS_MODE_SINGLE, UINT32_MAX - 10U);
    start(UINT32_MAX - 10U);
    measure_result_t result = finish(20U, 33U);
    assert(result.near_mm == 33000U && result.start_tick == 9U);
}
int main(void)
{
    test_gap_stop_and_mode();
    test_attitude_and_restart();
    test_preemption_and_stale_input();
    test_aggregation_and_timeout();
    test_periods_and_stop_retains_result();
    test_tick_wrap();
    puts("measurement tests passed");
    return 0;
}
