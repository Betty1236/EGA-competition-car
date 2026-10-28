/**
 @file motor_wheel.h
 @brief 在这里实现麦轮的运动学解算，pid控制，以及参数的传递
*/
#ifndef MOTOR_WHEEL_H
#define MOTOR_WHEEL_H

#include "motor_3508.h"
#include "fxi6x.h"

#ifdef __cplusplus
extern "C" {
#endif


void PID_Controller();



#ifdef __cplusplus
}
#endif

#endif