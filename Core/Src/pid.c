#include "pid.h"
#include "motor_3508.h"



void pid_init(PID* pid, float kp,float ki,float kd, float output_limit,float integral_limit){
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->output_limit = output_limit;
  pid->integral_limit = integral_limit;
  pid->error = 0;
  pid->last_error = 0;
	pid->integral = 0;
}

float pid_calculate(PID* pid, float target, float measure,float dt){
  pid->error = target - measure;
  float p = pid->kp * pid->error;
  float d = pid->kd * (pid->error - pid->last_error) / dt;
  pid->integral += pid->ki * pid->error * dt;
  if(pid->integral > pid->integral_limit) pid->integral = pid->integral_limit;
  if(pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;


  float output = p + d + pid->integral;
  if(output > pid->output_limit) output = pid->output_limit;
  if(output < -pid->output_limit) output = -pid->output_limit;

  pid->last_error = pid->error;
  
  return output;
}


float absolute(float a){
	if(a<0) return -a;
	else return a;
}