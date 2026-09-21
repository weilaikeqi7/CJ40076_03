# N32G4FR Rangefinder Firmware

Embedded rangefinder project based on the N32G4FR MCU, using the FreeRTOS real-time operating system.

## Project Overview

This project is a professional laser rangefinder firmware that implements multiple measurement modes and rich features:

- **Single Measurement**: Precise single distance measurement
- **Continuous Measurement**: Real-time continuous distance tracking
- **Directional Measurement**: Integrated electronic compass supporting heading and pitch angle measurement
- **Positioning Functionality**: Supports GNSS positioning
- **Attitude Display**: Real-time display of device attitude information
- **Data Logging**: Measurement data storage function

## System Architecture

### Application Layer (app)
- `app.c` - Main program logic and task management
- `app_attitude.c` - Attitude data processing (heading, pitch)
- `app_calib.c` - Calibration functions (magnetometer, accelerometer)
- `app_coord.c` - Coordinate calculation
- `app_display.c` - LCD display rendering
- `app_key.c` - Key scanning and event handling
- `app_measure.c` - Measurement core logic
- `app_power.c` - Power management
- `app_store.c` - Storage management
- `app_thermal.c` - Thermal management

### BSP Layer (bsp)
- `bsp_adc.c` - ADC sampling (battery voltage, temperature)
- `bsp_flash.c` - Flash read/write
- `bsp_gpio.c` - GPIO control
- `bsp_iwdg.c` - Independent watchdog timer
- `bsp_power.c` - Power control
- `bsp_pwm.c` - PWM control
- `bsp_timer.c` - Timers
- `bsp_uart.c` - Serial communication

### Device Layer (device)
- `dev_compass.c` - Electronic compass driver
- `dev_display.c` - LCD display driver
- `dev_gnss.c` - GNSS module driver
- `dev_heater.c` - Heater control
- `dev_key.c` - Key driver
- `dev_ranger.c` - Rangefinder driver
- `dev_storage.c` - Storage driver

### Test Directory
- `tests/adc` - ADC test
- `tests/app` - Application layer test
- `tests/compass` - Compass test
- `tests/display` - Display test
- `tests/gnss` - GNSS test
- `tests/key` - Key test
- `tests/measurement` - Measurement test
- `tests/runtime` - Runtime test

## Hardware Requirements

- MCU: N32G4FR
- Rangefinder module
- Electronic compass (heading/pitch)
- GNSS module
- LCD display
- Keypad input
- Battery-powered system

## Software Dependencies

- FreeRTOS Kernel V11.3.0
- ARM GCC Toolchain
- CMake Build System

## Build Method

### Environment Preparation

```bash
# Install ARM GCC Toolchain
# Download and configure cmake
```

### Build Steps

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake ..

# Compile
make
```

## Function Description

### Measurement Modes

1. **Single Measurement Mode (SINGLE)**
   - Trigger single measurement via key press
   - Display distance result

2. **Continuous Measurement Mode (CONTINUOUS)**
   - Real-time continuous measurement
   - Display current distance

3. **Compass Mode**
   - Display heading angle
   - Display pitch angle
   - Supports 0.01 degree precision

### Calibration Functions

- **Magnetometer Calibration**: 360-degree rotation calibration
- **Accelerometer Calibration**: Multi-angle calibration
- **Data Save**: Persistent storage of calibration parameters

### Display Interface

- Distance display (three digits + decimal point)
- Angle display
- Battery level icon
- Mode icon
- Status symbols

### Power Management

- Battery voltage monitoring
- Battery level display
- Automatic shutdown protection
- Low battery alarm

## Testing

The project includes comprehensive unit tests and integration tests:

```bash
# Run all tests
tests/run_all.sh

# Run specific module tests
tests/adc/run.sh
tests/compass/run.sh
tests/display/run.sh
tests/gnss/run.sh
tests/key/run.sh
tests/measurement/run.sh
tests/runtime/run.sh
tests/app/run.sh
```

## Project Structure

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

## License

This project uses the [Apache-2.0](LICENSE) license.