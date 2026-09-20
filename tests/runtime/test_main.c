#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "app.h"

static jmp_buf boot_exit;
static unsigned init_calls;
static unsigned init_stop_calls;
static unsigned suspend_all_calls;
static unsigned resume_all_calls;
static unsigned delete_calls;
static unsigned create_calls;
static unsigned suspend_calls;
static TaskHandle_t current_task;
static TaskHandle_t created_tasks[4];
static TaskHandle_t suspended_tasks[4];
static unsigned suspend_order[4];
static TaskHandle_t task_tokens[4] = {
    (TaskHandle_t)(uintptr_t)0x11U,
    (TaskHandle_t)(uintptr_t)0x22U,
    (TaskHandle_t)(uintptr_t)0x33U,
    (TaskHandle_t)(uintptr_t)0x44U,
};

static void reset_trace(void)
{
    init_calls = 0U;
    init_stop_calls = 0U;
    suspend_all_calls = 0U;
    resume_all_calls = 0U;
    delete_calls = 0U;
    create_calls = 0U;
    suspend_calls = 0U;
    current_task = NULL;
}

void app_system_init(void)
{
    ++init_calls;
    /* A shutdown request during bootstrap must be harmless: no worker exists. */
    app_stop_tasks();
    ++init_stop_calls;
}

void __disable_irq(void) {}
void NVIC_PriorityGroupConfig(unsigned group) { (void)group; }
void bsp_gpio_init(void) {}
void bsp_power_init(void) {}
void app_task_key(void *argument) { (void)argument; }
void app_task_sensor(void *argument) { (void)argument; }
void app_task_display(void *argument) { (void)argument; }
void app_task_power(void *argument) { (void)argument; }

TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }

BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg,
                       unsigned priority, TaskHandle_t *handle)
{
    (void)task;
    (void)name;
    (void)stack;
    (void)arg;
    (void)priority;
    assert(create_calls < 4U);
    *handle = task_tokens[create_calls];
    created_tasks[create_calls] = *handle;
    ++create_calls;
    return pdPASS;
}

void vTaskSuspend(TaskHandle_t task)
{
    assert(suspend_calls < 4U);
    suspended_tasks[suspend_calls] = task;
    suspend_order[suspend_calls] = suspend_calls;
    ++suspend_calls;
}

void vTaskSuspendAll(void) { ++suspend_all_calls; }
BaseType_t xTaskResumeAll(void)
{
    ++resume_all_calls;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task)
{
    assert(task == NULL);
    ++delete_calls;
    longjmp(boot_exit, 1);
}

void vTaskStartScheduler(void) { assert(!"unexpected scheduler start"); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main production_main
#include "../../src/main.c"
#undef main
#pragma GCC diagnostic pop

static void test_bootstrap_order(void)
{
    reset_trace();
    if (setjmp(boot_exit) == 0)
    {
        app_task_boot(NULL);
        assert(!"bootstrap returned without deleting itself");
    }

    assert(init_calls == 1U);
    assert(init_stop_calls == 1U);
    assert(create_calls == 4U);
    assert(suspend_all_calls == 1U);
    assert(resume_all_calls == 1U);
    assert(delete_calls == 1U);
    assert(suspend_calls == 0U);
    assert(created_tasks[0] == task_tokens[0]);
    assert(created_tasks[1] == task_tokens[1]);
    assert(created_tasks[2] == task_tokens[2]);
    assert(created_tasks[3] == task_tokens[3]);
}

static void test_stop_excludes_caller(void)
{
    unsigned i;
    reset_trace();
    /* Handles survive the bootstrap in the included production translation unit. */
    current_task = task_tokens[2];
    app_stop_tasks();
    assert(suspend_calls == 3U);
    for (i = 0U; i < 3U; ++i)
    {
        assert(suspended_tasks[i] != current_task);
    }
    assert(suspended_tasks[0] == task_tokens[0]);
    assert(suspended_tasks[1] == task_tokens[1]);
    assert(suspended_tasks[2] == task_tokens[3]);

    reset_trace();
    current_task = task_tokens[0];
    app_stop_tasks();
    assert(suspend_calls == 3U);
    assert(suspended_tasks[0] == task_tokens[1]);
    assert(suspended_tasks[1] == task_tokens[2]);
    assert(suspended_tasks[2] == task_tokens[3]);
}

int main(void)
{
    test_bootstrap_order();
    test_stop_excludes_caller();
    printf("runtime main bootstrap/stop: PASS\n");
    return 0;
}
