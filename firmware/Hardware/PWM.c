#include "stm32f10x.h"                  // Device header

//频率为1KHz，占空比为50%，分辨率为1%的PWM波形
void PWM_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 , ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;                 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	TIM_InternalClockConfig(TIM2);                                       //时基单元由内部时钟驱动
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;                   //配置时基单元
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;          //1分频
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;      //向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;                    //ARR，周期
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;                  //PSC，预分频的值
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;                 //重复计数器的值（高级定时器才有） 
	TIM_TimeBaseInit(TIM2 , &TIM_TimeBaseInitStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);                              //给结构体赋初始值，默认给初始值
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;                    //输出比较模式
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCPolarity_High;           //输出比较极性
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;        //输出使能
	TIM_OCInitStructure.TIM_Pulse = 0;                                  //CCR，设置CCR寄存器值
	TIM_OC3Init(TIM2, &TIM_OCInitStructure);
	
	TIM_Cmd(TIM2 , ENABLE);                                              //启动定时器
}

void PWM_SetCompare3(uint16_t Compare)
{
	TIM_SetCompare3(TIM2, Compare);                                      //用来单独更改通道1的CCR值
}
