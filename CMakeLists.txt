cmake_minimum_required(VERSION 3.20)

# ==============================================================================
# 1. 工具链设置（必须置于 project() 之前）
# ==============================================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_LIST_DIR}/cmake/arm-none-eabi-gcc.cmake"
        CACHE FILEPATH "N32 GNU Arm toolchain file")
endif()

project(CJ40076 C ASM)

set(APP_TARGET ${PROJECT_NAME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ==============================================================================
# 2. 芯片与硬件特性配置
# ==============================================================================
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/Nations.N32G4FR_Library.2.6.0"
    CACHE PATH "Nations N32G4FR SDK root")
set(N32_TARGET n32g4fr CACHE STRING "Nations target platform name")

set(N32_FLASH_ORIGIN "0x08000000" CACHE STRING "Flash origin" FORCE)
set(N32_FLASH_SIZE "512K" CACHE STRING "Flash size for N32G4FRHEQ7" FORCE)
set(N32_RAM_ORIGIN "0x20000000" CACHE STRING "RAM origin" FORCE)
set(N32_RAM_SIZE "144K" CACHE STRING "RAM size for N32G4FRHEQ7" FORCE)
set(N32_STACK_SIZE "0x1500" CACHE STRING "MSP stack size" FORCE)
set(N32_HEAP_SIZE "0x300" CACHE STRING "syscalls _sbrk heap size" FORCE)

option(N32_USE_FPU "Build for Cortex-M4F single precision FPU" ON)
set(N32_FLOAT_ABI "hard" CACHE STRING "GCC float ABI: hard, softfp, or soft" FORCE)

# 功能开关：newlib-nano 浮点打印 (%f) 支持
option(ENABLE_PRINTF_FLOAT "Enable printf/vsnprintf float formatting in newlib-nano" ON)


# ==============================================================================
# 3. 依赖路径与有效性校验
# ==============================================================================
set(CMSIS_CORE_DIR "${SDK_ROOT}/firmware/CMSIS/core")
set(CMSIS_DEVICE_DIR "${SDK_ROOT}/firmware/CMSIS/device")
set(STDPERIPH_DIR "${SDK_ROOT}/firmware/${N32_TARGET}_std_periph_driver")
set(STARTUP_FILE "${CMSIS_DEVICE_DIR}/startup/startup_${N32_TARGET}_gcc.s")
set(FREERTOS_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/FreeRTOS-Kernel")

foreach(required_path IN ITEMS
        "${CMSIS_CORE_DIR}"
        "${CMSIS_DEVICE_DIR}"
        "${STDPERIPH_DIR}/inc"
        "${STDPERIPH_DIR}/src"
        "${STARTUP_FILE}"
        "${FREERTOS_KERNEL_DIR}/include"
        "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing required SDK path: ${required_path}")
    endif()
endforeach()


# ==============================================================================
# 4. 生成链接脚本
# ==============================================================================
set(LINKER_SCRIPT_IN "${CMAKE_CURRENT_SOURCE_DIR}/linker/n32g4fr_flash.ld.in")
set(LINKER_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/n32g4fr_flash.ld")
configure_file("${LINKER_SCRIPT_IN}" "${LINKER_SCRIPT}" @ONLY)

# ==============================================================================
# 5. 收集源码并创建可执行目标
# ==============================================================================
file(GLOB STDPERIPH_SOURCES CONFIGURE_DEPENDS
    "${STDPERIPH_DIR}/src/*.c"
)

set(PROJECT_SOURCES
    src/main.c
    src/system/n32g4fr_it.c
    src/system/syscalls.c
    src/common/rtt_log.c

    # BSP 驱动层
    src/bsp/bsp_gpio.c
    src/bsp/bsp_power.c
    src/bsp/bsp_uart.c
    src/bsp/bsp_adc.c
    src/bsp/bsp_pwm.c
    src/bsp/bsp_timer.c
    src/bsp/bsp_flash.c
    src/bsp/bsp_iwdg.c

    # 设备层
    src/device/dev_ranger.c
    src/device/dev_compass.c
    src/device/dev_gnss.c
    src/device/dev_display.c
    src/device/dev_display_map.c
    src/device/dev_key.c
    src/device/dev_storage.c
    src/device/dev_heater.c

    # 应用层
    src/app/app_store.c
    src/app/app_power.c
    src/app/app_thermal.c
    src/app/app_key.c
    src/app/app_attitude.c
    src/app/app_coord.c
    src/app/app_measure.c
    src/app/app_display.c
    src/app/app_calib.c
    src/app/app.c

    # 芯片启动与底层时钟
    "${CMSIS_DEVICE_DIR}/system_n32g4fr.c"
    "${STARTUP_FILE}"

    # FreeRTOS 内核源码
    "${FREERTOS_KERNEL_DIR}/tasks.c"
    "${FREERTOS_KERNEL_DIR}/queue.c"
    "${FREERTOS_KERNEL_DIR}/list.c"
    "${FREERTOS_KERNEL_DIR}/timers.c"
    "${FREERTOS_KERNEL_DIR}/event_groups.c"
    "${FREERTOS_KERNEL_DIR}/stream_buffer.c"
    "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F/port.c"
    "${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c"

    # 标准外设驱动
    ${STDPERIPH_SOURCES}
)

add_executable(${APP_TARGET} ${PROJECT_SOURCES})

set_target_properties(${APP_TARGET} PROPERTIES
    LINK_DEPENDS "${LINKER_SCRIPT}"
)

# ==============================================================================
# 6. 目标架构与硬件指令集选项 (MCU_FLAGS)
# ==============================================================================
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
)

if(N32_USE_FPU)
    list(APPEND MCU_FLAGS -mfpu=fpv4-sp-d16 -mfloat-abi=${N32_FLOAT_ABI})
else()
    list(APPEND MCU_FLAGS -mfloat-abi=soft)
endif()

# ==============================================================================
# 7. 编译宏定义
# ==============================================================================
set(COMPASS_MODEL "MCG505" CACHE STRING "Compass model: JY901B, MCP406, MCG505")
set_property(CACHE COMPASS_MODEL PROPERTY STRINGS JY901B MCP406 MCG505)
string(TOUPPER "${COMPASS_MODEL}" COMPASS_MODEL_UPPER)
if(COMPASS_MODEL_UPPER STREQUAL "JY901B")
    set(COMPASS_MODEL_VALUE 1)
elseif(COMPASS_MODEL_UPPER STREQUAL "MCP406")
    set(COMPASS_MODEL_VALUE 2)
elseif(COMPASS_MODEL_UPPER STREQUAL "MCG505")
    set(COMPASS_MODEL_VALUE 3)
else()
    message(FATAL_ERROR "Unsupported COMPASS_MODEL: ${COMPASS_MODEL}")
endif()
message(STATUS "Compass model: ${COMPASS_MODEL_UPPER}")

target_compile_definitions(${APP_TARGET} PRIVATE
    N32G4FR
    USE_STDPERIPH_DRIVER
    COMPASS_MODEL=${COMPASS_MODEL_VALUE}
)

# ==============================================================================
# 8. 头文件包含目录
# ==============================================================================
target_include_directories(${APP_TARGET} PRIVATE
    inc
    inc/config
    inc/system
    inc/bsp
    inc/device
    inc/app
    inc/common
    "${CMSIS_CORE_DIR}"
    "${CMSIS_DEVICE_DIR}"
    "${STDPERIPH_DIR}/inc"
    "${FREERTOS_KERNEL_DIR}/include"
    "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F"
)

# ==============================================================================
# 9. 编译选项配置
# ==============================================================================
target_compile_options(${APP_TARGET} PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wstrict-prototypes
    -ffunction-sections
    -fdata-sections
    -fno-common
    -fmessage-length=0
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<CONFIG:Debug>:-Og -g3 -gdwarf-2>
    $<$<CONFIG:Release>:-Os -g2>
)

# ==============================================================================
# 10. 链接选项与库依赖
# ==============================================================================
target_link_options(${APP_TARGET} PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--cref
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    --specs=nosys.specs
    $<$<BOOL:${ENABLE_PRINTF_FLOAT}>:-Wl,-u,_printf_float>
)

target_link_libraries(${APP_TARGET} PRIVATE
    c
    m
    nosys
)

# ==============================================================================
# 11. 链接后自定义命令（生成 HEX / BIN 并输出段大小）
# ==============================================================================
add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${APP_TARGET}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex -S $<TARGET_FILE:${APP_TARGET}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${APP_TARGET}> ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN images"
)
