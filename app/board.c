#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "aht20.h"
#include "rtc.h"
#include "tim_delay.h"
#include "stm32f4xx.h"

void board_lowlevel_init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);	
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
  PWR_BackupAccessCmd(ENABLE);
  RCC_LSEConfig(RCC_LSE_ON);
  while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}
	

void board_init(void)
{
	tim_delay_init();
	console_init();

	printf("[SYS] Build Date: %s %s\n", __DATE__, __TIME__);
	rtc_init();
	aht20_init();
}

int fputc(int ch, FILE *f)
{
  USART_SendData(USART1, (uint8_t)ch);
  while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
  return ch;
}

void vAssertCalled(const char *file, int line)//当系统逻辑错误时（中断优先级配错，api用错，参数出现问题等）调用
{
	printf("Assert Called: %s(%d)\n", file, line);//错误文件
  portDISABLE_INTERRUPTS();//停机
}

void vAPPlicationStackOverflowHool(TaskHandle_t xTask, char *pcTaskName)//当任务栈溢出时调用
{
	printf("Stack Overflowed: %s\n", pcTaskName);
	configASSERT(0);
}

void vAPPlicationMallocFailedHook(void)//内存不够使用时调用
{
	printf("Malloc Failed\n");
	configASSERT(0);
}
