

# N32G4FR 测距仪固件

基于 N32G4FR MCU 的嵌入式测距仪项目，使用 FreeRTOS 实时操作系统。

## 项目概述

本项目是一个专业的激光测距仪固件，实现了多种测量模式和丰富的功能：

- **单次测量**：精确的单次距离测量
- **连续测量**：实时连续距离追踪
- **方向测量**：集成电子罗盘，支持航向和俯仰角测量
- **定位功能**：支持 GNSS 定位
- **姿态显示**：实时显示设备姿态信息
- **数据记录**：测量数据存储功能

## 系统架构

### 应用层 (app)
- `app.c` - 主程序逻辑和任务管理
- `app_attitude.c` - 姿态数据处理（航向、俯仰）
- `app_calib.c` - 校准功能（磁力计、加速度计）
- `app_coord.c` - 坐标计算
- `app_display.c` - LCD 显示渲染
- `app_key.c` - 按键扫描和事件处理
- `app_measure.c` - 测量核心逻辑
- `app_power.c` - 电源管理
- `app_store.c` - 存储管理
- `app_thermal.c` - 热管理

### BSP 层 (bsp)
- `bsp_adc.c` - ADC 采样（电池电压、温度）
- `bsp_flash.c` - Flash 读写
- `bsp_gpio.c` - GPIO 控制
- `bsp_iwdg.c` - 独立看门狗
- `bsp_power.c` - 电源控制
- `bsp_pwm.c` - PWM 控制
- `bsp_timer.c` - 定时器
- `bsp_uart.c` - 串口通信

### 设备层 (device)
- `dev_compass.c` - 电子罗盘驱动
- `dev_display.c` - LCD 显示驱动
- `dev_gnss.c` - GNSS 模块驱动
- `dev_heater.c` - 加热器控制
- `dev_key.c` - 按键驱动
- `dev_ranger.c` - 测距仪驱动
- `dev_storage.c` - 存储驱动

### 测试目录
- `tests/adc` - ADC 测试
- `tests/app` - 应用层测试
- `tests/compass` - 罗盘测试
- `tests/display` - 显示测试
- `tests/gnss` - GNSS 测试
- `tests/key` - 按键测试
- `tests/measurement` - 测量测试
- `tests/runtime` - 运行时测试

## 硬件要求

- MCU: N32G4FR
- 测距仪模块
- 电子罗盘（航向/俯仰）
- GNSS 模块
- LCD 显示屏
- 按键输入
- 电池供电系统

## 软件依赖

- FreeRTOS Kernel V11.3.0
- ARM GCC 工具链
- CMake 构建系统

## 编译方法

### 环境准备

```bash
# 安装 ARM GCC 工具链
# 下载并配置 cmake
```

### 编译步骤

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译
make
```

## 功能说明

### 测量模式

1. **单次测量模式 (SINGLE)**
   - 按键触发单次测量
   - 显示距离结果

2. **连续测量模式 (CONTINUOUS)**
   - 实时连续测量
   - 显示当前距离

3. **罗盘模式**
   - 显示航向角度
   - 显示俯仰角度
   - 支持 0.01 度精度

### 校准功能

- **磁力计校准**：360度旋转校准
- **加速度计校准**：多角度校准
- **数据保存**：校准参数持久化存储

### 显示界面

- 距离显示（三位数字+小数点）
- 角度显示
- 电池电量图标
- 模式图标
- 状态符号

### 电源管理

- 电池电压监测
- 电量等级显示
- 自动关机保护
- 低电量报警

## 测试

项目包含完整的单元测试和集成测试：

```bash
# 运行所有测试
tests/run_all.sh

# 运行特定模块测试
tests/adc/run.sh
tests/compass/run.sh
tests/display/run.sh
tests/gnss/run.sh
tests/key/run.sh
tests/measurement/run.sh
tests/runtime/run.sh
tests/app/run.sh
```

## 项目结构

```
├── CMakeLists.txt
├── cmake/
│   └── arm-none-eabi-gcc.cmake
├── inc/
│   ├── app/
│   ├── bsp/
│   ├── common/
│   ├── config/
│   ├── device/
│   └── main.h
├── src/
│   ├── app/
│   ├── bsp/
│   ├── common/
│   ├── device/
│   ├── main.c
│   └── system/
├── tests/
│   ├── adc/
│   ├── app/
│   ├── compass/
│   ├── display/
│   ├── gnss/
│   ├── key/
│   ├── measurement/
│   └── runtime/
└── vendor/
    └── FreeRTOS-Kernel/
```

## 许可证

本项目使用 [Apache-2.0](LICENSE) 许可证。