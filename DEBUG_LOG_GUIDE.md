# CJ40076 调试日志使用说明

## 构建开关

调试版打开模块日志：

```bash
cmake -S . -B build_debug -DCOMPASS_MODEL=MCG505 -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --parallel 4
```

发布版关闭日志（默认值）：

```bash
cmake -S . -B build_release -DCOMPASS_MODEL=MCG505 -DCMAKE_BUILD_TYPE=Release
cmake --build build_release --parallel 4
```

`CMAKE_BUILD_TYPE=Debug` 时自动设置 `ENABLE_DEBUG_LOG=ON`；`CMAKE_BUILD_TYPE=Release` 时自动设置 `ENABLE_DEBUG_LOG=OFF`。Release固件中，`LOGI/LOGW/LOGE` 及模块原始数据打印宏展开为空操作，不会输出这些调试打印。当前保留 RTT 驱动代码，但没有调试调用产生输出。

## RTT 输出分类

| 前缀 | 含义 |
| --- | --- |
| `[RAW][COMPASS_TX]` | 发给电子罗盘的完整二进制帧，十六进制 |
| `[RAW][COMPASS]` | 从电子罗盘收到的完整帧，十六进制 |
| `[DATA][COMPASS]` | 解析后的航向、俯仰、横滚 |
| `[DBG][COMPASS]` | 罗盘供电、波特率、CRC、响应命令、自检状态 |
| `[RAW][BEIDOU]` | 完整NMEA原始行，包含校验失败的行 |
| `[DATA][BEIDOU][RMC]` | RMC转换后的时间、日期、经纬度、速度、地面航向 |
| `[DATA][BEIDOU][GGA]` | GGA转换后的定位质量、卫星数、HDOP、经纬度、高程 |
| `[RAW][RANGER_TX]` | 发给测距机的完整协议帧 |
| `[RAW][RANGER]` | 收到的完整测距协议帧 |
| `[DATA][RANGER]` | 转换后的状态、目标号、有效性、距离 |

## MCG505重点观察顺序

1. 确认出现：`[DBG][COMPASS] power=1 uart=38400 settle_ms=500`。
2. 确认出现3个启动发送帧：
   - `AA 55 09 00 07 03 17 1F BD`：设置航向、俯仰、横滚输出；
   - `AA 55 0B 00 03 03 01 02 03 2B 1C`：设置输出数据组成；
   - `AA 55 07 00 0D F1 89`：开始连续输出。
3. 确认接收到以 `AA 55` 开头且命令为 `06` 的数据帧。
4. 若收到帧但出现 `CRC_invalid`，检查串口电平、波特率、接地和线路干扰。
5. 若只看到TX没有RX，依次检查：
   - MCG505 V+ 是否为4.5V~9V；
   - MCG505 GND 与主控共地；
   - MCG505 TXD 接主控 USART2 RX，即 PA3；
   - MCG505 RXD 接主控 USART2 TX，即 PA2；
   - TTL电平是否匹配；
   - 罗盘实际串口波特率是否仍为默认38400；
   - 电子罗盘是否确实上电。
6. 若收到响应帧但没有 `[DATA][COMPASS]`，保存全部 `[RAW][COMPASS]` 帧，用于核对帧长度、地址、命令号、CRC和数据ID。

## 注意

日志频率较高，RTT缓冲区满时会按现有无阻塞策略丢弃尾部日志；调试电子罗盘时优先观察TX/RX原始帧和自检结果，不要仅根据转换数据判断链路是否正常。
