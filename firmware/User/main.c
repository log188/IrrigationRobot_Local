#include "main.h"                  // Device header

uint16_t ADValue;
float angle = 90.0;  // 初始90度
uint16_t WaterCooldown = 0;   // 浇水冷却计数（每循环约100ms）

int main(void)
{
	OLED_Init();
	AD_Init();
	Light_Init();
	LED_Init();
	Serial_Init();
	Servo_Init();
	
	// 初始化PA1为推挽输出（继电器实测高电平触发：低电平=泵停）
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, GPIO_Pin_1);  // 初始低电平，泵停

	while(1)
	{
    ADValue = AD_GetValue();

    // ====== 第1行：土壤湿度百分比 ======
    uint8_t hum_percent;
    if(ADValue >= 4095)
        hum_percent = 0;
    else if(ADValue <= 0)
        hum_percent = 100;
    else
        hum_percent = (uint8_t)((4095 - ADValue) / 40.95);
    
    OLED_ShowString(1, 1, "SoilHumidity:");
    OLED_ShowNum(1, 14, hum_percent, 2);
    OLED_ShowString(1, 16, "%");

    // ====== 第2行：水泵状态 ======
    OLED_ShowString(2, 1, "PumpStatus:");
    if(GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 1)
        OLED_ShowString(2, 13, "ON ");
    else
        OLED_ShowString(2, 13, "OFF");

    // ====== 第4行：舵机角度 ======
    OLED_ShowString(4, 1, "ServoAngle:");
    if((uint16_t)angle >= 100)                     // 三位数：100~180
    {
        OLED_ShowNum(4, 12, (uint16_t)angle, 3);   // 数字占12~14列
        OLED_ShowString(4, 15, "dg");              // dg占15~16列
    }
    else                                           // 两位数：0~99
    {
        OLED_ShowString(4, 12, " ");               // 清掉三位数残留的百位
        OLED_ShowNum(4, 13, (uint16_t)angle, 2);   // 数字占13~14列
        OLED_ShowString(4, 15, "dg");              // dg占15~16列
    }

	  // ====== 云端手动浇水：收到'W'命令就浇水3秒（不受自动冷却限制） ======
		if(Serial_GetRxFlag() == 1)
		{
			uint8_t cmd = Serial_GetRxData();
				if(cmd == 'W')   // 手动浇水：任何时候都能触发
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_1);   // 开泵
			OLED_ShowString(2, 13, "ON ");     // OLED同步
			Serial_Printf("H:%d,P:%d,L:%d,A:%d\r\n", ADValue, 1,
			(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0) ? 1 : 0,
			(uint16_t)angle);              // 云端能看到 P:1
			Delay_ms(3000);
			GPIO_ResetBits(GPIOA, GPIO_Pin_1); // 停泵
			WaterCooldown = 150;               // 浇完照样进冷却，自动浇水不会马上再浇
		}
	}
		
    // 自动浇水（高电平触发：PA1=1 开泵；浇水后冷却约15秒，避免干土时一直浇）
    if(WaterCooldown) WaterCooldown--;
       if(ADValue > 2500 && WaterCooldown == 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_1);   // 高电平，开泵
				OLED_ShowString(2, 13, "ON ");     // 同步刷新OLED 
        Serial_Printf("H:%d,P:%d,L:%d,A:%d\r\n", ADValue, 1,
            (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0) ? 1 : 0,
            (uint16_t)angle);              // 浇水事件：云端能看到 P:1
        Delay_ms(3000);
        GPIO_ResetBits(GPIOA, GPIO_Pin_1); // 低电平，停泵
        WaterCooldown = 150;               // 冷却约15秒
    }
	
	// 读取光敏传感器（PA3，ADC通道3）
	ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_55Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
	uint16_t light_value = ADC_GetConversionValue(ADC1);

	// 追光算法：手电照→左转，遮挡→右转，环境光→缓慢回中
	if(light_value < 2000)          // 光照较强（手电照）
	{
		angle -= 5.0f;
		if(angle < 0) angle = 0;
	}
	else if(light_value > 3200)     // 光照较弱（遮挡）
	{
		angle += 5.0f;
		if(angle > 180) angle = 180;
	}
	else                            // 环境光：缓慢回中到90°
	{
		if(angle > 90.5f) angle -= 5.0f;
		else if(angle < 89.5f) angle += 5.0f;
	}
	
		// 追光算法结束，恢复ADC通道为通道0（土壤湿度传感器）
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
	
	// 三脚光敏传感器：数字输出，光照不足时DO输出高电平
	if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 1)
	{
    GPIO_SetBits(GPIOA, GPIO_Pin_6);   // 光照不足，开补光灯
	}
	else
	{
    GPIO_ResetBits(GPIOA, GPIO_Pin_6);     // 光照充足，关补光灯
	}
	
	Serial_Printf("H:%d,P:%d,L:%d,A:%d\r\n", ADValue, 
    (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 1) ? 1 : 0,
    (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0) ? 1 : 0,
    (uint16_t)angle);
	
	// 第3行：光照状态
	OLED_ShowString(3, 1, "Light:");
	if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 1)
		OLED_ShowString(3, 8, "DARK  ");
	else
		OLED_ShowString(3, 8, "BRIGHT");
	
	Servo_SetAngle(angle);

    Delay_ms(100);
	}
}
