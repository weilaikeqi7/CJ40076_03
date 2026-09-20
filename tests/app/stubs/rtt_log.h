#ifndef TEST_APP_RTT_LOG_H
#define TEST_APP_RTT_LOG_H

void rtt_log_init(void);
void test_log(const char* format, ...);
#define LOGI(...) test_log(__VA_ARGS__)
#define LOGW(...) test_log(__VA_ARGS__)

#endif
