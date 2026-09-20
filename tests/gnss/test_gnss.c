#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dev_gnss.h"
#include "bsp_uart.h"
#include "app_coord.h"

static const char* rx_data;
static size_t rx_len;
static size_t rx_pos;
static unsigned critical_depth;
static unsigned critical_enters;
static unsigned critical_exits;
static bool powered;
static unsigned flushes;
static unsigned reads;
static uint32_t tick = 100U;

void host_enter_critical(void)
{
    assert(critical_depth == 0U);
    ++critical_depth;
    ++critical_enters;
}

void host_exit_critical(void)
{
    assert(critical_depth == 1U);
    critical_depth = 0U;
    ++critical_exits;
}

uint32_t xTaskGetTickCount(void)
{
    return tick;
}

void vTaskDelay(uint32_t ticks)
{
    tick += ticks;
}

void bsp_pwr_gnss(bool on)
{
    powered = on;
}

void bsp_uart_init(bsp_uart_t port, uint32_t baud)
{
    (void)port;
    (void)baud;
}

size_t bsp_uart_read(bsp_uart_t port, void* buf, size_t max_len)
{
    ++reads;
    (void)port;
    if (rx_pos == rx_len || max_len == 0U)
    {
        return 0U;
    }
    *(uint8_t*)buf = (uint8_t)rx_data[rx_pos++];
    return 1U;
}

void bsp_uart_write(bsp_uart_t port, const void* data, size_t len)
{
    (void)port;
    (void)data;
    (void)len;
}

void bsp_uart_putc(bsp_uart_t port, uint8_t byte)
{
    (void)port;
    (void)byte;
}

void bsp_uart_flush_rx(bsp_uart_t port)
{
    (void)port;
    ++flushes;
    rx_pos = rx_len;
}

static void feed(const char* data)
{
    rx_data = data;
    rx_len = strlen(data);
    rx_pos = 0U;
    gnss_poll();
}

static void test_snapshot(void)
{
    static const char gga[] =
        "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*59\r\n";
    gnss_fix_snapshot_t snapshot;

    gnss_power_ctl(true);
    tick += 100U;
    feed(gga);
    gnss_get_fix_snapshot(&snapshot);
    assert(snapshot.fix_quality == GNSS_FIX_GNSS);
    assert(snapshot.tick_gga == 200U);
    assert(snapshot.latitude > 48.1172 && snapshot.latitude < 48.1174);
    assert(snapshot.longitude > 11.5166 && snapshot.longitude < 11.5168);
    assert(snapshot.altitude_m > 545.3f && snapshot.altitude_m < 545.5f);
    assert(critical_depth == 0U);
    assert(critical_enters == critical_exits);

    {
        app_geo_point_t self;
        unsigned reads_before = reads;
        assert(coord_get_self(&self));
        assert(self.valid);
        assert(reads == reads_before);
        assert(self.latitude > 48.1172 && self.latitude < 48.1174);
    }
}

static void test_power_flush(void)
{
    unsigned before = critical_enters;

    powered = false;
    gnss_power_ctl(false);
    assert(!powered);
    assert(flushes == 1U);
    assert(critical_enters == before + 1U);
    assert(critical_enters == critical_exits);
}

int main(void)
{
    test_snapshot();
    test_power_flush();
    puts("dev_gnss snapshot/power: PASS");
    return 0;
}
