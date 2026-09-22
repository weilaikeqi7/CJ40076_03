/* Link against src/device/dev_compass.c, never include or replace its logic. */
#include "dev_compass.h"
#include "bsp_uart.h"
#include "bsp_power.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr, "FAIL model=%d ticks=%u %s:%d: %s\n", COMPASS_MODEL, \
            (unsigned)configTICK_RATE_HZ, __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)

typedef struct { uint8_t bytes[12]; size_t len; unsigned wait_ms; } expected_t;
typedef struct { uint8_t bytes[16]; size_t len; TickType_t tick; } tx_t;
static tx_t tx[128];
static size_t tx_count;
static uint8_t rx[4096];
static size_t rx_size, rx_pos;
static TickType_t now;
static unsigned delay_count, critical_depth, scheduler_depth, power_calls, flush_calls, init_calls;
static uint32_t baud_rate;
static bool power_on;

void host_enter_critical(void) { ++critical_depth; }
void host_exit_critical(void) { CHECK(critical_depth > 0); --critical_depth; }
TickType_t xTaskGetTickCount(void) { return now; }
void vTaskDelay(TickType_t ticks)
{
    CHECK(critical_depth == 0);
    CHECK(scheduler_depth == 0);
    CHECK(ticks != 0);
    CHECK(++delay_count < 10000);
    now += ticks;
}
void vTaskSuspendAll(void) { CHECK(critical_depth == 0); ++scheduler_depth; }
int xTaskResumeAll(void) { CHECK(scheduler_depth > 0); --scheduler_depth; return 0; }
void bsp_uart_init(bsp_uart_t port, uint32_t baud)
{
    CHECK(port == BSP_UART_COMPASS);
    baud_rate = baud;
    ++init_calls;
}
void bsp_uart_write(bsp_uart_t port, const void *data, size_t len)
{
    CHECK(port == BSP_UART_COMPASS);
    CHECK(power_on);
    CHECK(tx_count < COUNT(tx));
    CHECK(len <= sizeof(tx[0].bytes));
    memcpy(tx[tx_count].bytes, data, len);
    tx[tx_count].len = len;
    tx[tx_count++].tick = now;
}
size_t bsp_uart_read(bsp_uart_t port, void *buf, size_t max_len)
{
    size_t len = rx_size - rx_pos;
    CHECK(port == BSP_UART_COMPASS);
    if (len > max_len) len = max_len;
    memcpy(buf, rx + rx_pos, len);
    rx_pos += len;
    return len;
}
void bsp_uart_flush_rx(bsp_uart_t port)
{
    CHECK(port == BSP_UART_COMPASS);
    rx_pos = rx_size = 0;
    ++flush_calls;
}
void bsp_pwr_compass(bool on) { power_on = on; ++power_calls; }

#if COMPASS_MODEL == 1
#define J(reg, lo, hi, wait) {{0xFF, 0xAA, reg, lo, hi}, 5, wait}
static const expected_t setup[] = {
    J(0x69, 0x88, 0xB5, 200), J(0x23, 1, 0, 200), J(0, 0, 0, 100),
    J(0x69, 0x88, 0xB5, 200), J(2, 8, 0, 200), J(0, 0, 0, 100),
    J(0x69, 0x88, 0xB5, 200), J(3, 5, 0, 200), J(0, 0, 0, 100)
};
static const expected_t reset[] = { J(0x69, 0x88, 0xB5, 200), J(0, 1, 0, 1000) };
static const expected_t accel[] = {
    J(0x69, 0x88, 0xB5, 200), J(1, 1, 0, 4000), J(1, 0, 0, 100), J(0, 0, 0, 100)
};
static const expected_t angle[] = {
    J(0x69, 0x88, 0xB5, 200), J(1, 8, 0, 3000), J(0, 0, 0, 100)
};
static const expected_t mag_start[] = { J(0x69, 0x88, 0xB5, 200), J(1, 7, 0, 0) };
static const expected_t mag_end[] = {
    J(0x69, 0x88, 0xB5, 200), J(1, 0, 0, 100), J(0, 0, 0, 100)
};
#elif COMPASS_MODEL == 2
static const expected_t setup[] = {
    {{0, 7, 6, 0x0A, 0x17, 0x6E, 0x90}, 7, 50},
    {{0, 5, 9, 0x6E, 0xDC}, 5, 50},
    {{0, 9, 3, 3, 5, 0x18, 0x19, 0xDF, 0xDE}, 9, 50},
    {{0, 5, 0x15, 0xBD, 0x61}, 5, 50}
};
static const expected_t reset[] = {
    {{0, 5, 0x1D, 0x3C, 0x69}, 5, 100}, {{0, 5, 0x24, 0x9B, 0x13}, 5, 100}
};
static const expected_t mag_start[] = { {{0, 9, 0x0A, 0, 0, 0, 0x0A, 0xAF, 6}, 9, 0} };
static const expected_t mag_end[] = { {{0, 5, 0x0B, 0x4E, 0x9E}, 5, 0} };
static const expected_t sample[] = { {{0, 5, 0x1F, 0x1C, 0x2B}, 5, 0} };
static const expected_t save[] = { {{0, 5, 9, 0x6E, 0xDC}, 5, 0} };
#else
static const expected_t setup[] = {
    {{0xAA, 0x55, 9, 0, 7, 3, 0x03, 0x4D, 0x08}, 9, 50},
    {{0xAA, 0x55, 11, 0, 3, 3, 1, 2, 3, 0x2B, 0x1C}, 11, 50},
    {{0xAA, 0x55, 7, 0, 0x0D, 0xF1, 0x89}, 7, 50}
};
static const expected_t reset[] = { {{0xAA, 0x55, 7, 0, 0x14, 0x72, 0x91}, 7, 100} };
static const expected_t mag_start[] = { {{0xAA, 0x55, 8, 0, 0x0F, 3, 0xF4, 0xD1}, 8, 0} };
static const expected_t mag_end[] = { {{0xAA, 0x55, 7, 0, 0x10, 0x32, 0x15}, 7, 0} };
static const expected_t sample[] = { {{0xAA, 0x55, 7, 0, 0x11, 0x22, 0x34}, 7, 0} };
#endif

static void expect_tx(size_t index, const expected_t *want, TickType_t tick)
{
    CHECK(index < tx_count);
    CHECK(tx[index].len == want->len);
    CHECK(memcmp(tx[index].bytes, want->bytes, want->len) == 0);
    CHECK(tx[index].tick == tick);
}
static TickType_t expect_trace(const expected_t *steps, size_t count, TickType_t start)
{
    size_t i;
    CHECK(tx_count == count);
    for (i = 0; i < count; ++i) {
        expect_tx(i, &steps[i], start);
        start += pdMS_TO_TICKS(steps[i].wait_ms);
    }
    return start;
}
static void fixture(void)
{
    TickType_t end;
    compass_power_ctl(false);
    now = 0;
    tx_count = rx_size = rx_pos = 0;
    delay_count = power_calls = flush_calls = init_calls = 0;
    compass_init();
    CHECK(power_on && power_calls == 1 && flush_calls == 2 && init_calls == 1);
    CHECK(critical_depth == 0);
    CHECK(baud_rate == (COMPASS_MODEL == 1 ? 9600U : 115200U));
    CHECK(strcmp(compass_model_name(), COMPASS_MODEL == 1 ? "JY901B" :
                 COMPASS_MODEL == 2 ? "MCP406" : "MCG505") == 0);
    end = expect_trace(setup, COUNT(setup), pdMS_TO_TICKS(500));
    CHECK(now == end + pdMS_TO_TICKS(10));
    CHECK(!compass_is_busy());
    CHECK(compass_get_cal_state()->sample_count == 0);
    CHECK(compass_get_cal_state()->cal_score == -1.0f);
    CHECK(!compass_get_cal_state()->score_valid);
    CHECK(!compass_is_alive(1000));
    tx_count = 0;
}
static void feed(const uint8_t *bytes, size_t len)
{
    CHECK(rx_pos == rx_size);
    CHECK(len <= sizeof(rx));
    memcpy(rx, bytes, len);
    rx_size = len;
    rx_pos = 0;
    compass_poll();
    CHECK(rx_pos == rx_size);
}
static void check_data(float heading, float pitch, float roll, TickType_t tick)
{
    const compass_data_t *data = compass_get_data();
    CHECK(fabsf(data->heading - heading) < 0.001f);
    CHECK(fabsf(data->pitch - pitch) < 0.001f);
    CHECK(fabsf(data->roll - roll) < 0.001f);
    if (data->tick_angle != tick) {
        fprintf(stderr, "angle timestamp: got %lu, expected %lu; now=%lu\n",
                (unsigned long)data->tick_angle, (unsigned long)tick, (unsigned long)now);
    }
    CHECK(data->tick_angle == tick);
}

#if COMPASS_MODEL == 1
static size_t angle_frame(uint8_t *frame, int16_t heading, int16_t pitch, int16_t roll)
{
    int16_t values[] = {roll, pitch, heading};
    size_t i;
    memset(frame, 0, 11);
    frame[0] = 0x55; frame[1] = 0x53;
    for (i = 0; i < COUNT(values); ++i) {
        frame[2 + 2*i] = (uint8_t)values[i];
        frame[3 + 2*i] = (uint8_t)((uint16_t)values[i] >> 8);
    }
    for (i = 0; i < 10; ++i) frame[10] = (uint8_t)(frame[10] + frame[i]);
    return 11;
}
#else
/* Independent polynomial-division CRC oracle, checked against published vectors. */
static uint16_t wire_crc(const uint8_t *bytes, size_t len)
{
    uint32_t remainder = 0;
    size_t i;
    unsigned bit;
    for (i = 0; i < len + 2; ++i) {
        unsigned value = i < len ? bytes[i] : 0;
        for (bit = 0; bit < 8; ++bit) {
            remainder = (remainder << 1) | ((value >> (7 - bit)) & 1U);
            if (remainder & 0x10000U) remainder ^= 0x11021U;
        }
    }
    return (uint16_t)remainder;
}
static void put_float(uint8_t *bytes, float value)
{
    uint32_t bits;
    CHECK(sizeof(value) == sizeof(bits));
    memcpy(&bits, &value, sizeof(bits));
    bytes[0] = (uint8_t)(bits >> 24); bytes[1] = (uint8_t)(bits >> 16);
    bytes[2] = (uint8_t)(bits >> 8); bytes[3] = (uint8_t)bits;
}
static size_t packet(uint8_t *frame, uint8_t cmd, const uint8_t *payload, size_t len)
{
    uint16_t crc;
#if COMPASS_MODEL == 2
    size_t total = len + 5, offset = 3;
    frame[0] = (uint8_t)(total >> 8); frame[1] = (uint8_t)total; frame[2] = cmd;
#else
    size_t total = len + 7, offset = 5;
    frame[0] = 0xAA; frame[1] = 0x55; frame[2] = (uint8_t)total;
    frame[3] = 0; frame[4] = cmd;
#endif
    memcpy(frame + offset, payload, len);
    crc = wire_crc(frame, total - 2);
    frame[total - 2] = (uint8_t)(crc >> 8); frame[total - 1] = (uint8_t)crc;
    return total;
}
static size_t angle_frame(uint8_t *frame, float heading, float pitch, float roll)
{
    uint8_t payload[16] = {3};
#if COMPASS_MODEL == 2
    const uint8_t ids[] = {25, 5, 24};
    const uint8_t cmd = 5;
#else
    const uint8_t ids[] = {3, 1, 2};
    const uint8_t cmd = 6;
#endif
    const float values[] = {roll, heading, pitch};
    size_t i;
    for (i = 0; i < 3; ++i) {
        payload[1 + i*5] = ids[i];
        put_float(payload + 2 + i*5, values[i]);
#if COMPASS_MODEL == 3
        /* Angle records on MCG505 are little-endian; scores remain big-endian. */
        {
            uint8_t *p = payload + 2 + i*5;
            uint8_t tmp = p[0]; p[0] = p[3]; p[3] = tmp;
            tmp = p[1]; p[1] = p[2]; p[2] = tmp;
        }
#endif
    }
    return packet(frame, cmd, payload, sizeof(payload));
}
static void send_score(float score)
{
    uint8_t bytes[16], payload[4];
    size_t len;
    put_float(payload, score);
    len = packet(bytes, COMPASS_MODEL == 2 ? 0x12 : 0x13, payload, 4);
    feed(bytes, 2);
    feed(bytes + 2, len - 2);
}
#endif

static void test_frames(void)
{
    uint8_t frame[64], bad[64], noise[300];
    size_t len, split;
    TickType_t tick;
    fixture();
#if COMPASS_MODEL == 1
    len = angle_frame(frame, 16384, -8192, -16384);
#else
    CHECK(wire_crc((const uint8_t *)"123456789", 9) == 0x31C3);
    CHECK(wire_crc(setup[0].bytes, setup[0].len - 2) ==
          (uint16_t)((setup[0].bytes[setup[0].len - 2] << 8) | setup[0].bytes[setup[0].len - 1]));
    len = angle_frame(frame, -450.0f, 45.0f, -90.0f);
#endif
    /* Every possible split, including a header fragmented across polls. */
    for (split = 1; split < len; ++split) {
        compass_power_ctl(false); compass_power_ctl(true);
        now += pdMS_TO_TICKS(500); /* 上电稳定后才接收姿态帧 */
        feed(frame, split);
        CHECK(compass_get_data()->tick_angle == 0);
        ++now; tick = now;
        feed(frame + split, len - split);
        check_data(270, 45, -90, tick);
    }
    compass_power_ctl(false); compass_power_ctl(true);
        now += pdMS_TO_TICKS(500); /* 上电稳定后才接收姿态帧 */
    for (split = 0; split < len; ++split) {
        feed(frame + split, 1);
        if (split + 1 < len) CHECK(compass_get_data()->tick_angle == 0);
    }
    tick = now;
    check_data(270, 45, -90, tick);
    CHECK(compass_is_alive(100));
    now += pdMS_TO_TICKS(100) - 1;
    CHECK(compass_is_alive(100));
    ++now; CHECK(!compass_is_alive(100));

    memcpy(bad, frame, len); bad[len - 1] ^= 0x80;
    ++now;
    feed(bad, 3); feed(bad + 3, len - 3);
    check_data(270, 45, -90, tick);
    /* Invalid checksum/CRC followed immediately by fragmented valid traffic. */
    feed(frame, 1); feed(frame + 1, len - 1);
    tick = now; check_data(270, 45, -90, tick);
    memset(noise, 0xFE, sizeof(noise));
    feed(noise, sizeof(noise));
    ++now; feed(frame, len);
    check_data(270, 45, -90, now);

    /* Invalid frame length/type/header must not publish data. */
#if COMPASS_MODEL == 1
    memcpy(bad, frame, len); bad[1] = 0x51; bad[10] -= 2;
    feed(bad, len);
#else
    {
#if COMPASS_MODEL == 2
        const uint8_t malformed[] = {0, 4, 0xFF, 0, 0x81, 0xFF};
#else
        const uint8_t malformed[] = {0xAA, 0xFE, 0xAA, 0x55, 6, 0xAA, 0x55, 0x81};
#endif
        feed(malformed, sizeof(malformed));
    }
#endif
    ++now; feed(frame, len); check_data(270, 45, -90, now);
#if COMPASS_MODEL == 1
    len = angle_frame(frame, -16384, 8192, 16384);
    ++now; feed(frame, len); check_data(90, -45, 90, now);
    len = angle_frame(frame, INT16_MIN, INT16_MIN, INT16_MAX);
    ++now; feed(frame, len); check_data(180, 180, 179.9945068f, now);
#else
    len = angle_frame(frame, 810, -100, 123.5f);
    ++now; feed(frame, len); check_data(90, -90, 123.5f, now);
    len = angle_frame(frame, 360, 100, -123.5f);
    ++now; feed(frame, len); tick = now; check_data(0, 90, -123.5f, tick);
    len = angle_frame(frame, NAN, 0, 0);
    ++now; feed(frame, len); check_data(0, 90, -123.5f, tick);
    len = angle_frame(frame, 0, INFINITY, 0);
    feed(frame, len); check_data(0, 90, -123.5f, tick);
#endif
#if COMPASS_MODEL == 3
    {
        /* Captured from the user's MCG505 at 115200 baud, including its CRC. */
        static const uint8_t captured[] = {
            0xAA, 0x55, 0x17, 0x00, 0x06, 0x03,
            0x01, 0x15, 0x52, 0xF6, 0x42,
            0x02, 0x64, 0x21, 0xA4, 0xBF,
            0x03, 0xF2, 0x7F, 0xB6, 0x42, 0x1B, 0xB4
        };
        CHECK(wire_crc(captured, sizeof(captured) - 2) == 0x1BB4);
        for (split = 1; split < sizeof(captured); ++split) {
            tick = compass_get_data()->tick_angle;
            ++now;
            feed(captured, split);
            CHECK(compass_get_data()->tick_angle == tick);
            feed(captured + split, sizeof(captured) - split);
            check_data(123.160316f, -1.282269f, 91.249893f, now);
        }
        puts("  PASS captured MCG505 frame: heading=123.160 pitch=-1.282 roll=91.250");
    }
#endif
    CHECK(tx_count == 0);
    puts("  PASS angle decoding, fragmentation, bad frame/noise recovery, liveness");
}

/* A queued operation must not write or delay until compass_step is called. */
static void run_sequence(const expected_t *steps, size_t count)
{
    size_t i;
    unsigned delays = delay_count;
    TickType_t expected_tick = now;
    CHECK(tx_count == 0);
    CHECK(compass_is_busy());
    for (i = 0; i < count; ++i)
    {
        TickType_t wait = pdMS_TO_TICKS(steps[i].wait_ms);
        compass_step();
        CHECK(tx_count == i + 1U);
        expect_tx(i, steps + i, expected_tick);
        CHECK(compass_is_busy());
        CHECK(!compass_factory_reset());
        CHECK(!compass_calib_accel());
        CHECK(!compass_calib_angle_ref());
        if (wait != 0U)
        {
            compass_step();
            CHECK(tx_count == i + 1U);
            now += wait - 1U;
            compass_step();
            CHECK(tx_count == i + 1U);
            CHECK(compass_is_busy());
            ++now;
        }
        expected_tick += wait;
    }
    compass_step();
    CHECK(!compass_is_busy());
    CHECK(tx_count == count);
    CHECK(now == expected_tick);
    CHECK(delay_count == delays);
}
static void test_sequences(void)
{
    expected_t reset_setup[COUNT(reset) + COUNT(setup)];
    TickType_t start;
    unsigned delays;
    fixture();
    memcpy(reset_setup, reset, sizeof(reset));
    memcpy(reset_setup + COUNT(reset), setup, sizeof(setup));
    start = now; delays = delay_count;
    CHECK(compass_factory_reset());
    CHECK(now == start && delay_count == delays);
    run_sequence(reset_setup, COUNT(reset_setup));
#if COMPASS_MODEL == 1
    tx_count = 0; start = now; delays = delay_count;
    CHECK(compass_calib_accel()); CHECK(now == start && delay_count == delays);
    run_sequence(accel, COUNT(accel));
    tx_count = 0; start = now; delays = delay_count;
    CHECK(compass_calib_angle_ref()); CHECK(now == start && delay_count == delays);
    run_sequence(angle, COUNT(angle));
#else
    tx_count = 0; start = now; delays = delay_count;
    CHECK(!compass_calib_accel()); CHECK(!compass_calib_angle_ref());
    CHECK(tx_count == 0 && now == start && delay_count == delays);
    CHECK(!compass_is_busy());
#endif
    /* 真实回绕测试：重新上电后等待启动稳定，再在 Tick 溢出点执行复位。 */
    compass_power_ctl(false);
    now = UINT32_MAX - pdMS_TO_TICKS(50);
    compass_power_ctl(true);
    now += pdMS_TO_TICKS(500);
    tx_count = 0;
    run_sequence(setup, COUNT(setup));
    tx_count = 0;
    CHECK(compass_factory_reset());
    run_sequence(reset_setup, COUNT(reset_setup));
    puts("  PASS reset/setup, async command timing, busy rejection, tick wraparound");
}
static void test_mag(void)
{
    TickType_t start;
    size_t before;
    fixture();
    start = now;
    compass_calib_mag_start();
    run_sequence(mag_start, COUNT(mag_start));
    CHECK(now >= start);
    CHECK(compass_mag_uses_samples() == (COMPASS_MODEL != 1));
    CHECK(compass_get_cal_state()->sample_count == (COMPASS_MODEL == 1 ? 0U : 1U));
    CHECK(!compass_get_cal_state()->score_valid);
    CHECK(compass_get_cal_state()->cal_score == -1);
    tx_count = 0;
    compass_calib_take_sample();
#if COMPASS_MODEL == 1
    CHECK(tx_count == 0);
    CHECK(!compass_calib_score_valid(0.1f));
#else
    run_sequence(sample, COUNT(sample));
#endif
    tx_count = 0;
    compass_calib_mag_end();
    run_sequence(mag_end, COUNT(mag_end));
    before = tx_count;
    CHECK(!compass_calib_score_valid(NAN));
    CHECK(!compass_calib_score_valid(INFINITY));
    CHECK(!compass_calib_score_valid(-INFINITY));
    CHECK(!compass_calib_score_valid(-0.1f));
    CHECK(!compass_calib_score_valid(0));
    CHECK(tx_count == before);
#if COMPASS_MODEL != 1
    {
        uint8_t bytes[32];
#if COMPASS_MODEL == 2
        uint8_t count_payload[] = {0x01, 0x23, 0x45, 0x67};
        const uint32_t wanted = 0x01234567U;
        const float threshold = 1.02f;
#else
        uint8_t count_payload[] = {0xE7};
        const uint32_t wanted = 0xE7U;
        const float threshold = 0.36f;
#endif
        float scores[] = {nextafterf(threshold, 0), threshold, nextafterf(threshold, INFINITY), 0, -0.1f, NAN, INFINITY};
        size_t i, len = packet(bytes, COMPASS_MODEL == 2 ? 0x11 : 0x12, count_payload, sizeof(count_payload));
        feed(bytes, len - 1);
        CHECK(compass_get_cal_state()->sample_count == 1);
        feed(bytes + len - 1, 1);
        CHECK(compass_get_cal_state()->sample_count == wanted);
        CHECK(compass_calib_score_valid(nextafterf(0, INFINITY)));
        CHECK(compass_calib_score_valid(threshold));
        CHECK(!compass_calib_score_valid(nextafterf(threshold, INFINITY)));
        for (i = 0; i < COUNT(scores); ++i)
        {
            bool finite = isfinite(scores[i]) != 0;
            bool acceptable = i < 2;
            tx_count = 0;
            compass_calib_mag_start();
            run_sequence(mag_start, COUNT(mag_start));
            tx_count = 0;
            send_score(scores[i]);
            CHECK(compass_get_cal_state()->score_valid == finite);
            if (isnan(scores[i])) CHECK(isnan(compass_get_cal_state()->cal_score));
            else CHECK(compass_get_cal_state()->cal_score == scores[i]);
            CHECK(compass_calib_score_valid(scores[i]) == acceptable);
            compass_calib_take_sample();
            if (finite) CHECK(tx_count == 0);
            else run_sequence(sample, COUNT(sample));
            tx_count = 0;
            compass_calib_mag_end();
            if (!finite) run_sequence(mag_end, COUNT(mag_end));
#if COMPASS_MODEL == 2
            else if (acceptable) run_sequence(save, COUNT(save));
#endif
            else CHECK(tx_count == 0);
        }
    }
#endif
    puts("  PASS model-specific asynchronous magnetic calibration and score policy");
}
static void test_power_cancel(void)
{
    unsigned operation;
    fixture();
    for (operation = 0; operation < (COMPASS_MODEL == 1 ? 3U : 1U); ++operation) {
        size_t before;
        unsigned powers, flushes;
        CHECK(operation == 0 ? compass_factory_reset() :
              operation == 1 ? compass_calib_accel() : compass_calib_angle_ref());
        compass_step(); CHECK(compass_is_busy());
        before = tx_count;
        compass_power_ctl(false);
        CHECK(!compass_is_busy() && !power_on);
        check_data(0, 0, 0, 0);
        CHECK(!compass_factory_reset());
        CHECK(!compass_calib_accel());
        CHECK(!compass_calib_angle_ref());
        CHECK(!compass_is_alive(1000));
        now += pdMS_TO_TICKS(10000); compass_step(); compass_poll();
        CHECK(tx_count == before);
        powers = power_calls; flushes = flush_calls;
        compass_power_ctl(false);
        CHECK(power_calls == powers && flush_calls == flushes);
        tx_count = 0;
        compass_power_ctl(true);
        CHECK(!compass_is_ready());
        compass_step();
        CHECK(tx_count == 0);
        now += pdMS_TO_TICKS(500) - 1U;
        compass_step();
        CHECK(tx_count == 0);
        ++now;
        run_sequence(setup, COUNT(setup));
        CHECK(!compass_is_busy());
        powers = power_calls; flushes = flush_calls;
        compass_power_ctl(true);
        CHECK(power_calls == powers && flush_calls == flushes);
    }
    CHECK(critical_depth == 0);
    puts("  PASS power-off cancellation and powered-off operation rejection");
}
int main(int argc, char **argv)
{
    const char *group = argc == 2 ? argv[1] : "all";
    bool all = strcmp(group, "all") == 0;
    CHECK(argc <= 2);
    CHECK(all || strcmp(group, "frames") == 0 || strcmp(group, "sequences") == 0 ||
          strcmp(group, "mag") == 0 || strcmp(group, "power") == 0);
    if (all || strcmp(group, "frames") == 0) test_frames();
    if (all || strcmp(group, "sequences") == 0) test_sequences();
    if (all || strcmp(group, "mag") == 0) test_mag();
    if (all || strcmp(group, "power") == 0) test_power_cancel();
    printf("PASS %s COMPASS_MODEL=%d tick_rate=%u group=%s: %u checks\n", compass_model_name(),
           COMPASS_MODEL, (unsigned)configTICK_RATE_HZ, group, checks);
    return EXIT_SUCCESS;
}
