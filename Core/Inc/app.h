/**
@brief 这是电机的实际应用层 对不起，我只能写一份把pid，运动学解算都融为一体的石山应用层了
*/
#ifndef APP_H
#define APP_H 

#ifdef __cplusplus
extern "C" {
#endif

#include "can.h"
#include "motor_3508.h"
#include "motor_2006.h"
#include "fxi6x.h"
//我把两个电机变量的extern声明全放在了各自的.h库函数中，把实际的变量放在了各自的c文件里
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs);//接收反馈帧中断执行函数
void app_init(void);//初始化电机对象
void cal_exp_velocity(const Fxi6x_Data *rc);//读取rc数据包中的摇杆与旋钮数据，计算预期速度值
void app_loop(void);


#ifdef __cplusplus
}
#endif

#endif