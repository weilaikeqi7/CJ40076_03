# CJ40076 V3 固件（N32G4FRHEQ7）

多功能观测仪：激光测距（DYC-15A）+ 姿态罗盘（JY901B 垂直安装）+ GNSS 定位（BV-220）+ P1237 段码屏。

## 构建

```bash
cmake -B cmake-build-debug-n32 -G Ninja -DCMAKE_BUILD_TYPE=Debug -S .
cmake --build cmake-build-debug-n32
```

产物：`cmake-build-debug-n32/CJ40076.hex / .bin / .elf`（工具链 arm-none-eabi-gcc，J-Link 烧录 / RTT 通道 0 看日志）。

## 目录结构

```
src/
├── main.c                  入口（电源保持 -> 应用任务）
├── board/                  板级：GPIO/电源开关/按键/ADC/加热丝PWM/三路UART
├── device/                 外设驱动：lcd(段码屏) gnss(BV-220) jy901b ranger(DYC-15A)
├── app/                    业务层：app(主状态机) key measure attitude coord display calib store
└── common/                 rtt_log（SEGGER RTT 通道0）
inc/                        头文件，子目录与 src 对应
vendor/                     N32G4FR 标准库 2.6.0 + FreeRTOS-Kernel
```

分层依赖：`app -> device -> board -> vendor`。

## 引脚分配（40076 V4.0 主板）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 电池 ADC | PA0 | 20K/10K 分压，VBAT=ADC×3 |
| 温度 ADC | PA1 | NTC，10K 上拉 |
| 模式键 / 电源键 | PA4 / PA5 | 低有效，上拉输入 |
| JY901B | PA2/PA3(USART2) + PA8 电源 | 9600 8N1，垂直安装 ORIENT=1 |
| GNSS | PA9/PA10(USART1) + PB15 电源 | **115200** 8N1，NMEA 1Hz |
| 测距机 | PB0/PB1(UART6_RMP3) + PB3 电源 | **115200** 8N1，常供电 |
| 加热丝 | PB6 | TIM4_CH1 PWM 1kHz（默认复用） |
| 显示屏 | PA6=DISP PA7=EI PB13=LP PB14=FR + PB4 电源 | FR=TIM3 中断 62Hz 方波 |
| 电源保持 | PB12 | 高有效（开机必置高，置低关机） |

## 业务规则速查

- **按键**：电源键短按=启动/停止测量，长按 3s=关机（保存计数）；模式键单击=单次→连续→多功能→测试（600ms 窗口），三击=计数清零（**立即写 Flash**），四击=HEr，五/六击=磁场校准开始/结束，七击=PIt（双键 1s 切 HIt，再按保存退出），八击=加计校准，九击=角度参考；页内电源+/模式− 0.1° 步进（800ms 后 100ms 连发）
- **测距**：每轮先发多目标设置再发单次测距；末帧静默 **200ms** 聚合发布；单目标只出 F；3s 超时横杠；连续=8s 周期、测试=12s 周期；每轮（含无目标）计数+1（上限 9999）
- **姿态**：俯仰=−原始+PIt，航向=−原始+HIt+HEr（归一化 0~359.99°），默认 PIt=0 / **HIt=+90°** / HEr=0，0.01° 整数运算
- **显示**：全部 1 位小数；距离 XXXX.X / 航向 XXX.X° / 俯仰 −XX.X° / 高程 XXXX.X（负值取绝对值）/ 坐标 DDD°MM′SS.ss″（经纬度 1s 交替）；F/E 交替单次连续 1s、多功能测试 2s；最高位特殊段只显示 1~3（**0 熄灭**）；S6=LOCAL 本机 / S5=TARGET 目标
- **电池**：4 段（≥3800=4、≥3700=3、≥3600=2、以下=1）；**<2600mV 欠压关机并保存计数**
- **Flash**：末页 0x0807F800，magic+CRC16；写时机=三击清零/双键保存补偿/正常关机/欠压关机

## 联调 TODO（实物核对）

1. **最高位特殊段字形**：`src/device/lcd_map.c` 的 `lcd_hiseg_strokes` / `lcd_hiseg_font` 为占位推测。用 `lcd_symbol(8~11 / 25~28 / 43,44,46,47)` 逐段点亮确定笔画顺序后填表（0 已确认无法显示）
2. **模式图标段号**：`src/app/app_display.c` 的 `LCD_ICON_SINGLE(=S31)` / `LCD_ICON_CONT(=S32)` 为占位，确认屏上实际的"单次/连续"图标段后改宏
3. **测距小数位分辨率**：`src/device/ranger.c` 按 0.1m 处理（dist = 整数 + 小数位/10），发一次测距对比实物距离验证
4. **HIt=+90° 默认值**：四方向航向测试（手册 IMU-01）验证垂直安装的轴映射；固定偏差先调 HIt，环境误差调 HEr
5. **JY901B 垂直安装方向**：ORIENT=1 要求 Y 轴箭头朝上，装反了俯仰/航向符号会反

## RTT 日志

J-Link RTT Viewer 连通道 0。关键日志：启动自检（JY901B 角度帧 OK/FAILED）、测距发布/超时、校准 started/stopped/saved、Flash 保存失败、欠压关机。`rtt_printf` 不支持 %f（nano 库），浮点先放大为整数打印。
