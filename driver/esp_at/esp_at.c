#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "esp_at.h"

//开 USART2 → 发 AT 验通信 → 复位 ESP → 等模块就绪 → 发 AT 指令 → 收响应 → 匹配 OK/ERROR → 执行业务

#define ESP_AT_DEBUG    1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))//数组元素个数，sizeof(arr)为数组总大小，sizeof((arr)[0])为结构体元素大小

static bool esp_at_wait_ready(uint32_t timeout);
static bool esp_at_write_command(const char *command, uint32_t timeout);
static const char *esp_at_get_response(void);

typedef enum//枚举，可能响应的状态
{
    AT_ACK_NONE,//0
    AT_ACK_OK,//1
    AT_ACK_ERROR,//2
    AT_ACK_BUSY,//3
    AT_ACK_READY,//4
} at_ack_t;

typedef struct//结构体，建立映射关系，和上面的enum对应起来
{
    at_ack_t ack;//状态码
    const char *string;//预期返回文本
} at_ack_match_t;

static const at_ack_match_t at_ack_matches[] = //数组，查找表
{
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p...\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};

static char rxbuf[1024];//接收缓冲区，寄存接收的数据
static uint32_t rxlen = 0;//接收数据的长度
static char *rxline;
static at_ack_t rxack;
static SemaphoreHandle_t at_ack_semaphore;

static void esp_at_usart_write(const char *data);//发送命令到esp模块

static bool esp_at_wait_boot(uint32_t timeout);

static void esp_at_io_init(void)
{
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

}

static void esp_at_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    USART_Init(USART2, &USART_InitStructure);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    USART_Cmd(USART2, ENABLE);
}

static void esp_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
	DMA_StructInit(&DMA_InitStruct);
	DMA_InitStruct.DMA_Channel = DMA_Channel_4;
	DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;//告诉DMA要写入USART2的DR寄存器
	DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; 
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA1_Stream6, &DMA_InitStruct);
}

static void esp_at_int_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART2_IRQn, 5);
}

static void esp_at_lowlevel_init(void)
{
    esp_at_usart_init();
    esp_dma_init();
    esp_at_int_init();
    esp_at_io_init();
}

bool esp_at_init(void)//初始化esp模块
{   
    at_ack_semaphore = xSemaphoreCreateBinary();
    configASSERT(at_ack_semaphore);

    esp_at_lowlevel_init();
    
    if (!esp_at_wait_boot(3000))
        return false;
    if (!esp_at_write_command("AT+RESTORE\r\n", 2000))
        return false;
    if (!esp_at_wait_ready(5000))
        return false;
    
    return true;
}

static void esp_at_usart_write(const char *data)//发送命令到esp模块
{
    uint32_t len = strlen(data);
    DMA1_Stream6->M0AR = (uint32_t)data;//告诉DMA数据在哪里
    DMA1_Stream6->NDTR = len;//告诉DMA要发送的字节数量
	
		DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);
    DMA_Cmd(DMA1_Stream6, ENABLE);//使能dma1_stream6，开始传输数据
}

static at_ack_t match_internal_ack(const char *str)//将esp模块返回的数据与查找表相对应，返回状态码
{
    for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)//遍历查找表，i是第几个元素
    {
        //这里的at_ack_matches[i].string就是数组里面的“OK”那些，然后直接与esp模块返回的字符串进行匹配
        if (strcmp(str, at_ack_matches[i].string) == 0)
            return at_ack_matches[i].ack;//这里string对应的ack直接就是枚举里对应的状态码比如AT_ACK_OK就是1，不用和0(NONE)比较
    }
    return AT_ACK_NONE;//如果没有找到匹配的字符串，返回AT_ACK_NONE(0)
}

static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)//等待从ESP32接收响应，在超时前解析响应状态
{
    rxlen = 0;
    rxline = rxbuf;
    
    bool asked = xSemaphoreTake(at_ack_semaphore, pdMS_TO_TICKS(timeout)) == pdPASS;

    return asked ? rxack : AT_ACK_NONE;
}

static bool esp_at_wait_ready(uint32_t timeout)//等待esp模块就绪
{
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}

static bool esp_at_write_command(const char *command, uint32_t timeout)//发送命令到esp模块，等待响应
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", command);
#endif

    esp_at_usart_write(command);//将传入的command传入esp_at_usart_write函数，发送到esp模块的uart2
    at_ack_t ack = esp_at_usart_wait_receive(timeout);//等待esp模块的响应，在超时前解析响应状态，返回这个状态码at_ack_matches[i].ack，i是第几个元素

#if ESP_AT_DEBUG
    printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif

    return ack == AT_ACK_OK;
}

static const char *esp_at_get_response(void)//获取esp模块的响应
{
    return rxbuf;
}

static bool esp_at_wait_boot(uint32_t timeout)
{
		for(int t = 0; t < timeout; t +=100)
		{
			if(esp_at_write_command("AT\r\n", 100))
				return true;
		}
		return false;
}

bool esp_at_wifi_init(void)//初始化wifi
{
    return esp_at_write_command("AT+CWMODE=1\r\n", 1000);
}

bool esp_at_connect_wifi(const char *ssid, const char *pwd, const char *mac)//连接wifi
{
    if(ssid  == NULL || pwd == NULL)
        return false;

    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);

    if (mac)
        snprintf(cmd + len, sizeof(cmd) - len, ",\"%s\"\r\n", mac);

    return esp_at_write_command(cmd, 5000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)//解析CWSTATE响应
{
	response = strstr(response, "+CWSTATE:");
	if (response == NULL)
		return false;
	
	int wifi_state;
	if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
		return false;
		
	info->connected = (wifi_state == 2);
	
	return true;
}

static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)//解析CWJAP响应
{
	response = strstr(response, "+CWJAP:");
	if (response == NULL)
		return false;
	
	if (sscanf(response, "+CWJAP:\"%63[^\"]\", \"%17[^\"]\", %d, %d", info->ssid, info->bssid, &info->channel, &info->rssi) !=4)
		return false;
		
	return true;
}
bool esp_at_get_wifi_info(esp_wifi_info_t *info)//获取wifi信息,解析响应数据
{
    if(!esp_at_write_command("AT+CWSTATE?\r\n", 2000))
        return false;

    if(!parse_cwstate_response(esp_at_get_response(), info))
        return false;

		if(info -> connected == true)
		{
				if(!esp_at_write_command("AT+CWJAP?\r\n", 2000))
					return false;
	
				if(!parse_cwjap_response(esp_at_get_response(), info))
					return false;
		}
    return true;
}

bool wifi_is_connect(void)//连接wifi
{
    esp_wifi_info_t info;
    if(esp_at_get_wifi_info(&info))
    {
        return info.connected;
    }
    return false;
}

bool esp_at_sntp_init(void)
{
    if(!esp_at_write_command("AT+CIPSNTPCFG=1,8\r\n", 2000))
        return false;

    return true;
}

static uint8_t month_str_to_num(const char *month_str)
{
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};                       
    for (int i = 0; i < 12; i++)
	{
        if (strcmp(month_str, months[i]) == 0) 
		{
            return i + 1;
        }
    }
    
    return 0; 
}

static uint8_t weekday_str_to_num(const char *weekday_str)
{
    const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (int i = 0; i < 7; i++) 
	{
        if (strcmp(weekday_str, days[i]) == 0) 
		{
            return i + 1;
        }
    }
    
    return 0; 
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)//解析sntp时间响应
{
	char weekday_str[8];
	char month_str[4]; 
	response = strstr(response, "+CIPSNTPTIME:");
	if(sscanf(response, "+CIPSNTPTIME: %3s %3s %hhu %hhu:%hhu:%hhu %hu", 
			  weekday_str, month_str,
			  &date->day, &date->hour, &date->minute, &date->second, &date->year) != 7)
		return false;	
	
	date->weekday = weekday_str_to_num(weekday_str);
	date->month = month_str_to_num(month_str);
	
	return true;
}

bool esp_at_get_time(esp_date_time_t *date)
{
    if(!esp_at_write_command("AT+CIPSNTPTIME?\r\n", 2000))
        return false;

    if(!parse_cipsntptime_response(esp_at_get_response(), date))
        return false;
        
    return true;
}

const char *esp_at_http_get(const char *url)//去网址获取数据
{
    char cmd[512]; //接收命令的缓冲区
    snprintf(cmd, sizeof(cmd), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
    if (esp_at_write_command(cmd, 10000)) 
    {
        return esp_at_get_response();
    }
    return false;
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        if(rxlen < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen++] = USART_ReceiveData(USART2);
            if (rxbuf[rxlen - 1] == '\n')
            {
                rxbuf[rxlen] = '\0';
                at_ack_t ack = match_internal_ack(rxline);//rxline是状态码1（OK），2（ERROR）这些
                if (ack != AT_ACK_NONE)
                {
                    rxack = ack;
                    BaseType_t pxHigherPriorityTaskWoken;
                    xSemaphoreGiveFromISR(at_ack_semaphore, &pxHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
                }  
                rxline = rxbuf + rxlen;//跳过已匹配部分 
            }
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}    
