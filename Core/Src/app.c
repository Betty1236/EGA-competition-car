#include "app.h"
#include "buzzer.h"
#include "led.h"
#include "servo.h"
#include "pid.h"
/* 两组电机对象。 */



/**
 * @brief FDCAN RX FIFO0 新消息回调。
 *
 * 读取全部待处理帧，并交给两组电机对象解析。
 */
 

#define MAX_OMEGA 6.0f
#define MAX_VELOCITY 3.2f
#define MIN_X 0.04f
#define MIN_Y 0.04f
#define MIN_ANGLE 0.05f
#define RA 0.18f
#define RB 0.145f
#define RS 0.325f
#define RW 0.0762f


float exp_vx;
float exp_vy;
float exp_omega;
float exp_wheel_omega[4];
float exp_wheel_v[4];

float current[4];



PID *pid[4];

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs)
{
    (void)RxFifo0ITs;

    if (hfdcan != &hfdcan3)
    {
        return;
    }

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U)
    {
        FDCAN_RxHeaderTypeDef header;
        uint8_t data[8];

        if (HAL_FDCAN_GetRxMessage(hfdcan,
                                   FDCAN_RX_FIFO0,
                                   &header,
                                   data) != HAL_OK)
        {
            return;
        }

        if ((header.IdType != FDCAN_STANDARD_ID) ||
            (header.RxFrameType != FDCAN_DATA_FRAME) ||
            (header.DataLength > 8U))
        {
            continue;
        }

        Motor3508_Update(&g_m3508,
                         header.Identifier,
                         data,
                         (uint8_t)header.DataLength);
        Motor2006_Update(&g_m2006,
                         header.Identifier,
                         data,
                         (uint8_t)header.DataLength);
    }
}

void cal_exp_velocity(const Fxi6x_Data *rc)//这里开始执行运动学解算
{
		if(absolute(rc->rocker_l_v) <= MIN_X){
			exp_vx = 0;
		}
		else{
			exp_vx = (rc->rocker_l_v) * MAX_VELOCITY;
		}
		if(absolute(rc->rocker_l_h) <= MIN_Y){
			exp_vy=0;
		}
		else{
			exp_vy = (rc->rocker_l_h) * MAX_VELOCITY;
		}
		if(absolute(rc->knob_l) <= MIN_ANGLE){
			exp_omega=0;
		}
		else{
			exp_omega = (rc->knob_l) * MAX_OMEGA;
		}
		exp_wheel_omega[0] = (exp_vx - exp_vy - exp_omega * RS) / RW;
		exp_wheel_omega[1] = (exp_vx + exp_vy + exp_omega * RS) / RW;
		exp_wheel_omega[2] = (exp_vx + exp_vy - exp_omega * RS) / RW;
		exp_wheel_omega[3] = (exp_vx - exp_vy + exp_omega * RS) / RW;
}





void app_init(void)
{
    /*
     * 先初始化电机对象，再启动 CAN 中断。
     * 首帧反馈到达时，编码器基准已经就绪。
     */
	
	  //TODO: pid_init(); 此时我们还需要调参，暂时无法确定参数，先放这里假装有这么个东西
	
		Motor3508_Init(&g_m3508, 0b1010U);//电机左右正反安装不相同，应该把右前和右后的电机反转电流，才可以做到方向一致，让后续运动学公式有意义
    Motor2006_Init(&g_m2006, 0x00U);
		
    Can_Init(&hfdcan3);

    Buzzer_Init();
    Led_Init();
    Servo_Init();
    Fxi6x_Init();
	
    /* 上电提示：2 kHz，100 ms。 */
    Buzzer_Tone(2000U);
    HAL_Delay(100U);
    Buzzer_Off();

    /* 舵机上电，通道 0 回中。 */
		Servo_Set(0U, 1500U);//此处进行了小修改，应该先set再上电吧,应该先给好指令再上电，这样更稳妥一些
    Servo_On(0U);
    Servo_Power(true);
    
}


void app_loop(void)
{
		//ds说这个程序没法做到1ms控制循环，所以我在这里加一个控制频率
	  //我有一个1kHz的内部时钟源，正好用它来实现一个模拟时钟
		static uint32_t lasttick = 0U;//static修饰词，直接分配在堆区
		uint32_t nowtick = HAL_GetTick();
		if(nowtick == lasttick) return;//如果还在同一个毫秒内，那就直接退出，啥也不干
		lasttick = nowtick;//更新lasttick值，
		
    const Fxi6x_Data *rc;

    /*
     * 记录距离最近一次有效遥控帧的循环次数。
     * 控制循环为 1 kHz 时，200 次约等于 200 ms。
			此时我们大概率在CubeMX配置时，已经设置好了1Hz吧
     */
    static uint16_t rc_age;

    /* 解析遥控数据。 */
    if (Fxi6x_Update())
    {
        rc_age = 0U;
    }
    else if (rc_age < 1000U)
    {
        rc_age++;
    }

    rc = Fxi6x_GetData();
		
		cal_exp_velocity(rc);
		for(uint8_t i=0; i<=3; i++)
		{
				current[i] = pid_calculate(pid[i], exp_wheel_omega[i], g_m3508.velocity[i], 0.001f);
		}
		
		
    /* 遥控链路超时或接收机 failsafe 时，电机电流清零。 */
    if ((rc_age > 200U) || rc->failsafe)
    {
        for (uint8_t slot = 0U; slot < 4U; ++slot)
        {
            Motor3508_SetCurrent(&g_m3508, slot, 0);
        }

        Motor2006_SetCurrent(&g_m2006, 0U, 0);
        Led_SetRGB(32U, 0U, 0U);
    }
    else
    {
        for (uint8_t slot = 0U; slot < 4U; slot++)
        {
            Motor3508_SetCurrent(&g_m3508, slot, current[slot]);
        }

        Motor2006_SetCurrent(&g_m2006,
                             0U,
                             (int16_t)(rc->knob_r * 5000.0f));
        Led_SetRGB(0U, 32U, 0U);
    }

    /* 每个控制周期发送两组电机控制帧。 */
    Motor3508_Send(&g_m3508);
    Motor2006_Send(&g_m2006);
}