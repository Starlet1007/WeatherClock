#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "console.h"

static SemaphoreHandle_t write_async_semaphore;//信号量，等待dma写完数据
static console_received_func_t received_func;

static void console_io_init(void)
{
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

static void console_usart_init(void)
{
		USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200u;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    
	  USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
		USART_DMACmd(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static void console_dma_init(void)
{
		DMA_InitTypeDef DMA_InitStruct;
		DMA_StructInit(&DMA_InitStruct);
		DMA_InitStruct.DMA_Channel = DMA_Channel_4;
		DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;// 从USART1的DR寄存器读取数据
		DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
		DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
		DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
		DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
		DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
		DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; 
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
		DMA_Init(DMA2_Stream7, &DMA_InitStruct);
}

static void console_int_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART1_IRQn, 5);

    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA2_Stream7_IRQn, 5);
    
}

void console_init(void)
{
    write_async_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_async_semaphore);

    console_usart_init();
    console_dma_init();
		console_int_init();
		console_io_init();
}

void console_write(const char str[])
{
		uint32_t len = strlen(str);
		do
		{
			uint32_t chunk_size = len < 65535 ? len :65535;

      DMA_InitTypeDef DMA_InitStruct;
      DMA_StructInit(&DMA_InitStruct);

      DMA2_Stream7->M0AR = (uint32_t)str;
      DMA2_Stream7->NDTR = chunk_size;

			DMA_Cmd(DMA2_Stream7, ENABLE);//使能dma2_stream7，开始传输数据

      xSemaphoreTake(write_async_semaphore, portMAX_DELAY);//等待dma触发中断，信号量没有传输完毕（这个工程里指dma数据传输），则等待（portMAX_DELAY为一直等），直到传输完成
			
			str += chunk_size;
			len -= chunk_size;
		}while(len > 0);
	
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
		USART_ClearFlag(USART1, USART_FLAG_TC);
    
}

void console_received_register(console_received_func_t func)
{
    received_func = func;
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        if (received_func != NULL)
        {
            uint8_t data = USART_ReceiveData(USART1);
            received_func(data);
        }
        
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

void DMA2_Stream7_IRQHandler(void)
{ 
  //pdFALSE表示没有更高优先级任务被唤醒，pdTRUE表示有更高优先级任务被唤醒
  if(DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) == SET)//DMA_IT_TC是否传输完成
  { 
    BaseType_t pxHigherPriorityTaskWoken;// 用于存储更高优先级的任务是否被唤醒
    
    xSemaphoreGiveFromISR(write_async_semaphore, &pxHigherPriorityTaskWoken);
    //if (被唤醒的任务优先级 > 当前任务优先级) {*pxHigherPriorityTaskWoken = pdTRUE;}

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);// 如果有更高优先级任务被唤醒，就切换到更高优先级任务

    DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
  }
}
