#include "stm32f10x.h"                  // Device header
#include "stdio.h"                      //printf
#include "stdarg.h"                     //sprintf 封装

uint8_t Serial_RxData;
uint8_t Serial_RxFlag;

void Serial_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 , ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                                     //上拉/浮空输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitTypeDef USART_InitStruture;
	USART_InitStruture.USART_BaudRate = 9600;                                         //波特率
	USART_InitStruture.USART_HardwareFlowControl = USART_HardwareFlowControl_None;    //硬件流控制
	USART_InitStruture.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                    //模式       TX发送，RX接收
	USART_InitStruture.USART_Parity = USART_Parity_No;                                //校验（位） 无/奇/偶校验
	USART_InitStruture.USART_StopBits = USART_StopBits_1;                             //停止位
	USART_InitStruture.USART_WordLength = USART_WordLength_8b;                        //字长
	USART_Init(USART1 , &USART_InitStruture);
	
	USART_ITConfig(USART1 , USART_IT_RXNE , ENABLE);                                  //开启RXNE标志位到NVIC的输出，RXNE置1，就向NVIC申请中断
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;                                 //中断通道
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;                         //抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;                                //次优先级
	NVIC_Init(&NVIC_InitStructure);
	
	USART_Cmd(USART1 , ENABLE);
}

//发送字节 函数
void Serial_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);                                                    //发送
	while (USART_GetFlagStatus(USART1 , USART_FLAG_TXE) == RESET);                   //等待标志位TXE置1,由手册可知,下一次到SendData时,标志位会自动清零                 
} 

//发送数组 函数
void Serial_SendArray(uint8_t *Array , uint16_t Length)
{
	uint16_t i;
	for(i = 0; i < Length; i++) {
		Serial_SendByte(Array[i]);
	}
}

//发送字符串 函数
void Serial_SendString (char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++) {
		Serial_SendByte(String[i]);
	}
}

//发送字符形式的数字 函数
     //次方函数
uint32_t Serial_Pow (uint32_t X , uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--) {
		Result *= X;
	}		
	return Result;    // X的Y次方
} 

void Serial_SendNumber(uint32_t Number , uint8_t Length)
{
	uint8_t i;
	for(i = 0; i < Length; i++) {
		Serial_SendByte(Number / Serial_Pow(10 , Length - i - 1) % 10 + '0');
	}
}




int fputc(int ch , FILE *f)                                                         //printf函数 底层
{
	Serial_SendByte(ch);
	return ch;
}

//sprintf 封装
void Serial_Printf(char *format , ...)
{
	char String[100];
	va_list arg;
	va_start(arg , format);
	vsprintf(String , format , arg);
	va_end(arg);
	Serial_SendString(String);
}

//中断的封装
uint8_t Serial_GetRxFlag(void)
{
	if (Serial_RxFlag == 1) {
		Serial_RxFlag = 0;
		return 1;
	}
	return 0;
}

uint8_t Serial_GetRxData(void)
{
	return Serial_RxData;
}

//中断函数
void USART1_IRQHandler(void)
{
	if(USART_GetFlagStatus(USART1 , USART_IT_RXNE) == SET) {          //判断标志位
		
		Serial_RxData = USART_ReceiveData(USART1);
		Serial_RxFlag = 1;
		
		USART_ClearITPendingBit(USART1 , USART_IT_RXNE);                //清空DR
	}
}
