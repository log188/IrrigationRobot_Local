#include "stm32f10x.h"                  // Device header

void AD_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);                        											//ADCCLK 分频器
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);  //选择规则组输入通道
	
	ADC_InitTypeDef ADC_Inittructure;
	ADC_Inittructure.ADC_Mode = ADC_Mode_Independent;
	ADC_Inittructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_Inittructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;           //不使用外部触发，用内部软件触发
	ADC_Inittructure.ADC_ContinuousConvMode = DISABLE;                           //连续切换模式
	ADC_Inittructure.ADC_ScanConvMode = DISABLE;                                 //扫描切换模式
	ADC_Inittructure.ADC_NbrOfChannel = 1;                                        //通道数目，指定在扫描模式下，会用到几个通道
	ADC_Init(ADC1,&ADC_Inittructure);
	
	ADC_Cmd(ADC1, ENABLE);
	
	//ADC校准
	ADC_ResetCalibration(ADC1);                                                   //复位校准
  while (ADC_GetResetCalibrationStatus(ADC1) == SET);                           //返回复位校准状态，0（RESET）跳出循环，1（SET）继续
  ADC_StartCalibration(ADC1);                                                   //开始校准
	while (ADC_GetCalibrationStatus(ADC1) == SET);                                //获取校准状态
}

uint16_t AD_GetValue(void)
{
	ADC_SoftwareStartConvCmd(ADC1 , ENABLE);                                     //启动 软件触发转换函数，ADC转换
	while (ADC_GetFlagStatus(ADC1 , ADC_FLAG_EOC) == RESET);                     //等待 转换  等待时间为 1/12 * （55.5 + 12.5）= 5.6us
	return ADC_GetConversionValue(ADC1);                                         //读取 获取转换值
}

void Light_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void LED_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_6);
}
