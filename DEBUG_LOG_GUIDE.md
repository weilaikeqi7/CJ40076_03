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

1. 当前实机已确认使用 **115200**，确认出现：`[DBG][COMPASS] power=1 uart=115200 settle_ms=500`。手册中的默认波特率不代表当前实机配置。
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
   - 主控串口与当前罗盘配置是否均为115200；
   - 电子罗盘是否确实上电。
6. 若收到响应帧但没有 `[DATA][COMPASS]`，保存全部 `[RAW][COMPASS]` 帧，用于核对帧长度、地址、命令号、CRC和数据ID。

## MCG505 角度字节序与实机帧核对

当前实机 `0x06` 角度响应采用小端 IEEE 754 Float32；MCP406角度仍按大端处理。MCG505校准评分 `0x13` 暂按手册示例保留大端（`3D CC CC CD` 表示0.1），尚需实机评分帧确认。帧尾CRC仍为高字节在前，不能将整帧统一反转。

实机回归帧：

```text
AA 55 17 00 06 03 01 15 52 F6 42 02 64 21 A4 BF 03 F2 7F B6 42 1B B4
```

CRC校验值为 `0x1BB4`，正确解析日志为：

```text
[DATA][COMPASS] model=MCG505 heading=123.160 pitch=-1.282 roll=91.250
```

该日志为设备驱动解码后的姿态，不含应用层的用户安装角/磁偏角补偿。若读数曾出现极大横滚数值或被限幅成正负90度，原因为角度字节序错误，并非靠修改打印小数位解决。

北斗原始RMC中的 `V` 表示当前定位无效。此时输出 `valid=0 position_unavailable`，不再将空字段的零值打印成定位结果。有效RMC分为坐标、时间和运动信息三行，避免超过单条RTT格式化缓冲长度。

## 全模块状态日志（1Hz 节流）

| 前缀 | 内容 | 频率 |
| --- | --- | --- |
| `[STATUS][COMPASS]` | 罗盘有效标志、原因（no_frame/stale/settling/powered_off）、帧龄、串口积压、字节/帧/CRC计数；有效时随附 `[DATA][COMPASS]` 角度 | 1s |
| `[STATUS][BEIDOU]` | 北斗供电、settling、串口积压、定位质量、卫星数、GGA龄、经纬度与高程 | 1s（仅供电时） |
| `[STATUS][RANGER]` | 测距机供电、在线状态、末帧龄、串口积压、错误码 | 2s |
| `[STATUS][POWER]` | 带载电池电压 mV、电量档位（档位变化或电压变化≥20mV 时打印） | 500ms 检查 |
| `[STATUS][THERM]` | NTC 温度（0.1℃）、电池电压、占空比（‰）、状态与原因（含 laser_derate / prewarm） | 1s |
| `[EVENT][KEY]` | 按键事件码、参数、双键当前状态、是否校准页 | 事件驱动 |
| `[STATE][MEASURE]` | 测量状态机 SETUP/COMMAND_WAIT 状态迁移 | 事件驱动 |
| `[EVENT][MEASURE]` | 单次指令发出时的姿态快照、轮次发布（近/远目标、帧数、结束原因 silence/out_of_range/timeout） | 事件驱动 |
| `[STATE][POWER]` | 罗盘/GNSS 上下电策略变化 | 事件驱动 |
| `[FAULT][THERM]` | 温度采样无效、电池低压切断加热 | 事件驱动 |
| `[FAULT][POWER]` | 电池 ADC 采样失败 | 事件驱动 |
| `[FAULT][TASK]` | 任务保活缺失掩码（仅掩码变化时打印一次） | 事件驱动 |

设备层补充：`[DBG][HEATER]` 记录 PWM 占空比写入与关闭；罗盘原始帧/CRC/RX 日志在临界区外输出，不影响解析时序。

## 注意

日志频率较高，RTT缓冲区（Debug 4KB）满时会按现有无阻塞策略丢弃尾部日志；调试电子罗盘时优先观察TX/RX原始帧和自检结果，不要仅根据转换数据判断链路是否正常。
