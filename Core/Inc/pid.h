/**
@brief 在此实现pid控制电机，直接抄课上代码了
*/
#ifndef PID_H
#define PID_H 

#ifdef __cplusplus
extern "C" {
#endif
/**
PID控制结构体
 kp ki kd 比例 积分 微分增益参数
 error 当前误差=target-测量值
 last_error 上一周期的error
 integral 误差累积
 target 目标值
 ol il 输出上限 积分上限
 
*/
typedef struct {
  float kp;
  float ki;
  float kd;

  float error;
  float last_error;
  float integral;

  float target;

  float output_limit;
  float integral_limit;
} PID;



void pid_init(PID* pid, float kp,float ki,float kd, float output_limit,float integral_limit);
float pid_calculate(PID *pid,float target,float measure,float dt);
float absolute(float a);//在这里写个绝对值函数，留着"死区判定"的时候用

#ifdef __cplusplus
}
#endif

#endif