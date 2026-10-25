# 四轮麦轮底盘 · 开发 TODO

> 目标：遥控器拨杆 → 底盘速度指令 → 麦轮运动学 → 四轮速度 PID → CAN 电流指令 → 电机
> 硬件：STM32H723VGT6（LQFP100）· 3× FDCAN · UART5（DBUS 接收机）· 4× 麦轮
> 工具链：STM32CubeMX 6.18.1 + FW_H7 V1.13.0 · Keil MDK 5.43.1（ARM Compiler 6.24）

---

## 0. 当前状态（已完成 / 待办）

### 已完成

- [x] **修复 Keil 打开工程闪退**：根因是 `STM32H7xx_DFP 4.1.1` 的 H72x/73x 子系列器件描述缺 `<debugconfig>`，uVision 解析时 `0xC0000409` 崩溃
      → 已安装 **STM32H7xx_DFP 4.1.3**（4.1.1 可保留，Keil 取高版本）
- [x] **工程改用 ARM Compiler 6**：`car.uvprojx` 增加 `<uAC6>1</uAC6>` + `<pCCUsed>6240000::V6.24::ARMCLANG</pCCUsed>`，编译 **0 error / 0 warning**，`car.hex` 已生成
- [x] **时钟树写入 `car.ioc`**：HSE 24 MHz → SYSCLK 480 / HCLK 240 / APB 120 / FDCAN 96 MHz
- [x] FDCAN 时钟源从 PLL2 切到 **PLL1Q**（与 SYSCLK 同源，精度一致）

### 待办（下一步就做）

- [ ] **CubeMX 重新生成代码**（当前 `main.c` 还是旧的 HSI 64 MHz / VOS3 / 1WS，必须重新生成）
- [ ] 生成后逐项核对（见 §2 检查清单）
- [ ] FDCAN 位定时 / 滤波器 / RxFIFO / Start 还没配（`.ioc` 里仍是默认 2 Mbps）
- [ ] 1 kHz 时基 TIM6 还没加

---

## 1. 时钟树（已定稿，参数照抄）

```
HSE 24 MHz ──/M=3──► 8 MHz ──×N=60──► VCO 480 MHz ──/P=1──► SYSCLK 480 MHz
                                       │                    ├─/1─► CPU 480 MHz
                                       │                    └─/2─► HCLK 240 MHz
                                       │                            └─/2─► APB1~4 120 MHz
                                       │                                     └ 定时器 240 MHz
                                       └─/Q=5─► 96 MHz ──► FDCAN1/2/3
```

| 项目 | 值 | 约束 |
|---|---|---|
| HSE | 24 MHz | 板上晶振 |
| PLL1 M / N / P / Q / R | **3 / 60 / 1 / 5 / 2** | VCO 输入 8 MHz（8~16 MHz 档） |
| SYSCLK / CPU | 480 MHz | ≤550 |
| AXI / HCLK | 240 MHz（HPRE /2） | ≤275 |
| APB1~4 | 120 MHz（/2） | ≤120 |
| FDCAN 内核 | **96 MHz**（PLL1Q） | ≤100 |
| UART5 | 120 MHz（PCLK1） | BRR=1200，100 kbaud 误差 0% |
| VOS / Flash | **VOS0 / 4 WS** | 生成后核对 |

> 想省电可把 P 改 2 → SYSCLK 240（VOS1 / 2WS），外设时钟完全不变。

### 生成后检查清单（5 条）

- [ ] `main.c`：`PLLSource = RCC_PLLSOURCE_HSE`、`PLLM=3 / PLLN=60 / PLLP=1`
- [ ] `main.c`：`FLASH_LATENCY_4`
- [ ] `main.c`：`PWR_REGULATOR_VOLTAGE_SCALE0`
- [ ] `fdcan.c`：`NominalPrescaler=12 / TimeSeg1=6 / TimeSeg2=1`、`StdFiltersNbr≥1`、`RxFifo0ElmtsNbr=8`、有 `HAL_FDCAN_Start`
- [ ] `usart.c`：`100000 / 9bit / EVEN / 1 stop` 没被改回

---

## 2. M0 工程底座

- [ ] TIM6 产生 1 kHz：APB1 定时器 240 MHz → 预分频 239（1 MHz）→ ARR 999
- [ ] 中断里只置标志，业务在 `while(1)` 跑（`App_Control1kHz()`）
- [ ] 目录分层：`BSP/`（can、uart、tim）/ `App/`（rc、motor、chassis、pid）
- [ ] 调试输出：UART 或 RTT，准备 VOFA+ JustFloat 看波形
- [ ] 中断优先级：TIM6=2 > FDCAN=3 > UART5=4 > SysTick=15

**验收**：`tick` 1 秒正好 +1000，无丢拍。

---

## 3. M1 遥控器接收（UART5 + DMA + IDLE）

- [ ] CubeMX：UART5 = Asynchronous，**100000 / 9bit(含校验) / Even / 1 stop**，只收，过采样 16，FIFO 开
- [ ] DMA：`USART5_RX`，**Circular**，Byte/Byte，Memory 递增
- [ ] NVIC：USART5 全局中断使能（IDLE 用）
- [ ] GPIO：`PD2` RX 上拉；`PB13` TX 可 disable
- [ ] 时钟源：USART5 → PCLK1（120 MHz）
- [ ] `HAL_UARTEx_ReceiveToIdle_DMA(&huart5, buf, 32)`，回调里判 `Size==25 && buf[0]==0x20`，只 memcpy + 置标志后重挂
- [ ] DBUS 解析：`buf[1..22]` = 16×11bit（LSB 先行）；`buf[23]`/`buf[24]` 低 2 位 = S1/S2
- [ ] 归一化：364/1024/1684 → `[-1, 0, +1]`，死区 ±20
- [ ] 一阶低通滤波（α≈0.3）
- [ ] 掉线检测：>100 ms 无新帧 → `online=0`

**验收**：摇杆打满打印 ±1.000，中位 0.000；拔接收机 100 ms 内停车。
> 若是 S.BUS（反相、8E2）需硬件反相或换协议，先确认接收机型号。

---

## 4. M2 CAN 电机驱动

- [ ] FDCAN 位定时（96 MHz 下）：**Prescaler=12, TimeSeg1=6, TimeSeg2=1, SJW=1** → 1 Mbps / 87.5%
- [ ] 标准 ID 滤波器（反馈 0x201~0x204 或按驱动手册）+ **RxFifo0 深度 8** + 勾选 Activate
- [ ] 总线分配表：FDCAN1 / FDCAN2 挂电机，FDCAN3 备用（按实际接线）
- [ ] 发送：电流指令打包（C620 例：`0x200` 8 字节，±16384 ↔ ±20 A）
- [ ] 接收：FIFO0 中断按 ID 分发 → 解析角度 / 转速 / 电流 / 温度
- [ ] 离线检测 >20 ms、Bus-Off 恢复、使能 / 失能 / 清错
- [ ] 驱动抽象 `motor_ops_t`（enable / set_cmd / parse / send）

**验收**：4 电机反馈 1 kHz 更新；手转轮子转速符号正确；拔线 20 ms 内标记离线。

---

## 5. M3 单轮速度闭环 PID

- [ ] 增量式 PID；输出限幅 = 电调允许电流；积分限幅 + 遇限幅停止积分
- [ ] 微分先用测量值微分（避免目标阶跃冲击）
- [ ] 目标/反馈统一 **rpm**；前馈 `k_ff` 可选
- [ ] 阶跃测试 0→500→−500 rpm，记录上升时间/超调/稳态误差
- [ ] 整定顺序：Kp 到临界振荡 → 降 30% → 加 Ki → 加 Kd

**验收**：超调 <10%，稳态误差 <5 rpm，±1000 rpm 不振荡。

---

## 6. M4 麦轮运动学

- [ ] 坐标约定：`+x 前`、`+y 左`、`+wz 逆时针`
- [ ] 逆运动学（`k = L/2 + W/2`）：

```
v_FL = vx − vy − k·wz        v_FR = vx + vy + k·wz
v_RL = vx + vy − k·wz        v_RR = vx − vy + k·wz

wheel_rpm_i = v_i / (2πR) · 60 · gear_ratio
```

- [ ] 辊子方向核对（X 型 / O 型，装反会斜着走）
- [ ] 限幅：先限合成速度，再限单轮 rpm
- [ ] 正运动学 + 轮速积分（可选，里程计）

**验收**：原地旋转四轮等速反向；45° 斜行不跑偏；纯前后左右对角对称。

---

## 7. M5 遥控映射 + 整车联调

- [ ] 摇杆映射 vx/vy/wz，加灵敏度曲线
- [ ] 拨杆状态机：`INIT → IDLE → RUN → FAULT`
- [ ] 使能条件：拨杆回中 + 摇杆归零 + 电机在线
- [ ] 目标速度斜坡限幅（约 2 m/s²）
- [ ] 保护：遥控掉线 / 电机离线 / Bus-Off / 过温 → 零电流 + FAULT

**验收**：全向平移、斜行、边走边转正常；拔接收机立刻停车。

---

## 8. M6 调参与收尾

- [ ] 参数集中到 `chassis_param_t`，串口在线改参
- [ ] 参数保存 Flash（可选）
- [ ] 标定：轮径/减速比（推 1 m 数圈）、死区、中位
- [ ] 30 min 连续跑机：温升、丢帧、CAN 错误计数
- [ ] 输出一份接线/ID 对照表

---

## 9. M7 可选增强

- [ ] IMU 闭环 wz（麦轮打滑，纯轮速积分会漂）
- [ ] 速度前馈 / 电流环
- [ ] 电池电压补偿（低电量限电流）
- [ ] 上位机曲线（VOFA+）

---

## 10. 关键数据结构

```c
typedef struct { uint16_t ch[16]; uint8_t s1, s2; uint8_t online; uint32_t last_ms; } rc_raw_t;
typedef struct { float vx, vy, wz; float vx_f, vy_f, wz_f; uint8_t s1, s2, online; } rc_cmd_t;

typedef struct {
    uint16_t can_id; int16_t current_cmd;
    float angle_deg, speed_rpm; int16_t current_act; uint8_t temp;
    uint32_t last_rx_ms; uint8_t online;
} motor_t;

typedef struct {
    void (*enable)(motor_t*, bool);
    void (*set_cmd)(motor_t*, float);
    void (*parse)(motor_t*, const uint8_t*);
    void (*send)(motor_t*);
} motor_ops_t;

typedef struct { float wheelbase, track, radius, gear; } chassis_geom_t;
typedef struct { chassis_geom_t geom; float vx, vy, wz; float wheel_rpm[4]; float rpm_max; } chassis_t;

typedef struct {
    float kp, ki, kd, i_term;
    float err, err_prev, err_prev2, meas, meas_prev;
    float out, out_max, out_min, i_max, deadband;
} pid_t;

typedef struct { pid_t pid[4]; float k_ff, acc_limit, rc_deadzone, v_max, w_max; } chassis_param_t;
```

## 11. 必要函数

```c
/* 时基 */  BSP_Init(); BSP_Millis(); TIM6_Start1kHz();
/* 遥控 */  RC_Init(); RC_Update(); RC_GetAxis(axis); RC_IsOnline(); RC_GetSwitch();
/* 电机 */  Motor_Init(); Motor_EnableAll(); Motor_SendAll(); Motor_OnCanRx(hdr,buf); Motor_Update();
/* 运动学 */Chassis_SetVelocity(); Chassis_InverseKinematics(); Chassis_ForwardKinematics();
/* PID */   PID_Init(); PID_Calc(pid,target,meas,dt); PID_Reset();
/* 应用 */  App_Init(); App_Loop(); App_Control1kHz(); Safety_Update();
```

## 12. 频率分配

| 任务 | 频率 | 位置 |
|---|---|---|
| CAN 反馈解析 | 事件（1 kHz 回帧） | FDCAN RX 中断 |
| CAN 指令发送 | 1 kHz | `App_Control1kHz` |
| 运动学 + 4×PID | 1 kHz | `App_Control1kHz` |
| 遥控解析 | 帧驱动（~71 Hz） | UART5 中断 |
| 遥控平滑/超时 | 1 kHz | `RC_Update` |
| 状态机/保护 | 1 kHz | `App_Control1kHz` |
| 调试打印 | 50~100 Hz | 1 kHz 分频 |

## 13. 风险提醒

- 每次 **CubeMX Generate Code** 都会覆盖 `car.uvprojx`，AC6 设置会丢 → 报 "ARM-Compiler Version 5 not available"，在 Options for Target → Target 里重选 AC6 即可
- 不要把 `STM32H7xx_DFP` 退回 4.1.1（会重新闪退）
- FDCAN 位定时现在是默认的 2 Mbps，不改电机一帧都收不到
- 滤波器 / RxFIFO 为 0 时同样收不到任何帧
- 麦轮辊子装反、CAN ID 冲突、DBUS/SBUS 混淆是联调最容易卡住的三件事

## 14. 待确认参数

| # | 项目 | 填什么 |
|---|---|---|
| 1 | 电机 + 电调型号 / 协议 | |
| 2 | FDCAN1/2/3 各自挂什么 | |
| 3 | 麦轮 L / W / R / 减速比 / 辊子方向 | |
| 4 | 遥控器型号（DBUS / SBUS / CRSF） | |
| 5 | 最大线速度 / 角速度 | |
