#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "stm32f4xx.h"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "st7789.h"
#include "font.h"
#include "image.h"

// SCK---->PB13---->ch0(clk)
// SDA---->PB15---->ch1
// RST---->PB8---->ch2
// DC---->PB9---->ch3
// CS---->PB12---->ch4


#define RST_PORT GPIOB
#define RST_PIN GPIO_Pin_8
#define DC_PORT GPIOB
#define DC_PIN GPIO_Pin_9
#define CS_PORT GPIOB
#define CS_PIN GPIO_Pin_12

static SemaphoreHandle_t write_gram_semaphore;//信号量，等待dma写完数据

static void st7789_init_display(void);

static void st7789_io_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Pin = RST_PIN | DC_PIN | CS_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void st7789_spi_init(void)
{
    SPI_InitTypeDef SPI_InitStructure;
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI2, &SPI_InitStructure);
		SPI_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_Cmd(SPI2, ENABLE);
}

static void st7789_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
	DMA_StructInit(&DMA_InitStruct);
	DMA_InitStruct.DMA_Channel = DMA_Channel_0;
	DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;//告诉DMA要写入SPI2的DR寄存器
	DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; 
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);
	DMA_Init(DMA1_Stream4, &DMA_InitStruct);
}

static void st7789_int_init(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA1_Stream4_IRQn, 5);
}

void st7789_init(void)
{
    write_gram_semaphore = xSemaphoreCreateBinary();//创建信号量
    configASSERT(write_gram_semaphore);

    st7789_spi_init();
    st7789_dma_init();
    st7789_int_init();
    st7789_io_init();
    
    st7789_init_display();
}

static void st7789_write_register(uint8_t reg, uint8_t data[], uint16_t length) // 写入寄存器
{
		SPI_DataSizeConfig(SPI2, SPI_DataSize_8b);
	
    GPIO_ResetBits(CS_PORT, CS_PIN); // 选中从机

    GPIO_ResetBits(DC_PORT, DC_PIN); // 发送指令

    SPI_SendData(SPI2, reg); // 发送指令
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
        
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);

    GPIO_SetBits(DC_PORT, DC_PIN); // 发送数据

    for (uint16_t i = 0; i < length; i++) // 发送参数具体配置数据
    {
        SPI_SendData(SPI2, data[i]); // 循环发送配置参数
        while (!SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE));
    }
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);

    GPIO_SetBits(CS_PORT, CS_PIN); // 配置结束
}

static void st7789_write_gram(uint8_t data[], uint32_t length, bool singlecolor)//以极限速度传输像素点
{
	SPI_DataSizeConfig(SPI2, SPI_DataSize_16b);

    GPIO_ResetBits(CS_PORT, CS_PIN);
    GPIO_SetBits(DC_PORT, DC_PIN);

	length >>=1;
	do
	{
	  uint32_t chunk_size = length < 65535 ? length :65535;

      DMA_InitTypeDef DMA_InitStruct;
      DMA_StructInit(&DMA_InitStruct);

      if(singlecolor) DMA1_Stream4->CR &= ~DMA_SxCR_MINC;
      else            DMA1_Stream4->CR |= DMA_SxCR_MINC;
      DMA1_Stream4->M0AR = (uint32_t)data;
      DMA1_Stream4->NDTR = chunk_size;

			DMA_Cmd(DMA1_Stream4, ENABLE);
      xSemaphoreTake(write_gram_semaphore, portMAX_DELAY);

	  if(!singlecolor)data += chunk_size * 2;
	  length -= chunk_size;
	}while(length > 0);
    
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);
   
    GPIO_SetBits(CS_PORT, CS_PIN);
}

static void st7789_reset(void)
{
    GPIO_ResetBits(RST_PORT, RST_PIN);
    vTaskDelay(pdMS_TO_TICKS(2)); 
    GPIO_SetBits(RST_PORT, RST_PIN);
    vTaskDelay(pdMS_TO_TICKS(120)); 
}

static void st7789_init_display(void) // 初始化显示
{
    st7789_reset();

    st7789_write_register(0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    st7789_write_register(0x11, NULL, 0);
     vTaskDelay(pdMS_TO_TICKS(5));

    st7789_write_register(0x3A, (uint8_t[]){0x05}, 1);

    st7789_write_register(0xC5, (uint8_t[]){0x1A}, 1);

    st7789_write_register(0x36, (uint8_t[]){0x00}, 1);

    st7789_write_register(0xb2, (uint8_t[]){0x05, 0x05, 0x00, 0x33, 0x33}, 5);

    st7789_write_register(0xb7, (uint8_t[]){0x05}, 1);

    st7789_write_register(0xBB, (uint8_t[]){0x3F}, 1);

    st7789_write_register(0xC0, (uint8_t[]){0x2c}, 1);

    st7789_write_register(0xC2, (uint8_t[]){0x01}, 1);

    st7789_write_register(0xC3, (uint8_t[]){0x0F}, 1);

    st7789_write_register(0xC4, (uint8_t[]){0x20}, 1);

    st7789_write_register(0xC6, (uint8_t[]){0X01}, 1);

    st7789_write_register(0xd0, (uint8_t[]){0xa4, 0xa1}, 2);

    st7789_write_register(0xE8, (uint8_t[]){0x03}, 1);

    st7789_write_register(0xE9, (uint8_t[]){0x09, 0x09, 0x08}, 3);

    st7789_write_register(0xE0, (uint8_t[]){0xD0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3F, 0x07, 0x13, 0x14, 0x28, 0x30}, 14);

    st7789_write_register(0xE1, (uint8_t[]){0xD0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F}, 14);

    st7789_write_register(0x20, NULL, 0);
     vTaskDelay(pdMS_TO_TICKS(120));

    st7789_write_register(0x29, NULL, 0);
    st7789_fill_color(0, 0, ST7789_WIDTH, ST7789_HEIGHT, 0x0000);
}

static bool in_screen_range(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) // 判断坐标是否在屏幕范围内
{
    if (x1 >= ST7789_WIDTH || y1 >= ST7789_HEIGHT)
        return false;
    if (x2 > ST7789_WIDTH || y2 >= ST7789_HEIGHT)
        return false;

    return true;
}

/* spi设置的是8位数据，st7789需要16位数据，所以需要将16位数据转换为8位数据，分两次发送，第一次发送高8位，第二次发送低8位*/
static void st7789_set_range_and_prepare_gram(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) // 设置画框显示窗口，起点x1,y1，终点x2,y2
{
    st7789_write_register(0x2A, (uint8_t[]){(x1 >> 8) & 0xff, x1 & 0xff, (x2 >> 8) & 0xff, x2 & 0xff}, 4); // x1，x2的高8位和低8位
    st7789_write_register(0x2B, (uint8_t[]){(y1 >> 8) & 0xff, y1 & 0xff, (y2 >> 8) & 0xff, y2 & 0xff}, 4); // y1，y2的高8位和低8位
    st7789_write_register(0x2C, NULL, 0);
}

void st7789_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) // 刷背景色
{
    if (!in_screen_range(x1, y1, x2, y2))
        return;

    st7789_set_range_and_prepare_gram(x1, y1, x2, y2);

    uint32_t pixels = (x2 - x1 + 1) * (y2 - y1 + 1);// 计算像素数量
    st7789_write_gram((uint8_t *)&color, pixels * 2, true);
}

static void st7789_draw_font(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *model, uint16_t color, uint16_t bg_color) // 负责绘制字体模型（英文字母，数字，标点）
{
    uint16_t bytes_per_row = (width + 7) / 8;                             // 计算每一行需要发送的字节数

    static uint8_t buff[72 * 72 * 2];
    uint8_t *pbuf = buff;
    
    for (uint16_t row = 0; row < height; row++)
    {
        const uint8_t *row_data = model + row * bytes_per_row;
        for (uint16_t col = 0; col < width; col++)
        {
            uint8_t pixel = row_data[col / 8] & (1 << (7 - col % 8));
            uint16_t pixel_color = pixel ? color : bg_color;
            
            *pbuf++ = pixel_color & 0xff;
            *pbuf++ = (pixel_color >> 8) & 0xff;
        }
    }

    st7789_set_range_and_prepare_gram(x, y, x + width - 1, y + height - 1);
    st7789_write_gram(buff, pbuf - buff, false);
}

static const uint8_t *ascii_get_model(const char ch, const font_t *font)
{
    uint16_t bytes_per_row = (font->size / 2 + 7) / 8;
    uint16_t bytes_per_char = font->size * bytes_per_row;
    if (font->ascii_map)
    {
        const char *map = font->ascii_map;
        do
        {
            if (*map == ch)
            {
                return font->ascii_model + (map - font->ascii_map) * bytes_per_char;
            }
        } while (*(++map) != '\0');
    }
    else
    {
        return font->ascii_model + (ch - ' ') * bytes_per_char;
    }
    return NULL;
}

static void st7789_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if (font == NULL)
        return;

    uint16_t fheight = font->size, fwidth = font->size / 2;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;

    if (ch < 0x20 || ch > 0x7E)
        return;

    const uint8_t *model = ascii_get_model(ch, font);
    if (model)
        st7789_draw_font(x, y, fwidth, fheight, model, color, bg_color);
}

static void st7789_write_chinese(uint16_t x, uint16_t y, char *ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if (ch == NULL || font == NULL)
        return;

    uint16_t fheight = font->size, fwidth = font->size;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;

    const font_chinese_t *c = font->chinese;
    for (; c->name != NULL; c++)
    {
        if (strcmp(c->name, ch) == 0)
            break;
    }
    if (c->name == NULL)
        return;

    st7789_draw_font(x, y, fwidth, fheight, c->model, color, bg_color);
}

static bool is_gb2312(char ch)
{
    return ((unsigned char)ch >= 0xA1 && (unsigned char)ch <= 0xF7);
}

// static int utf8_char_length(const char *str)
//{
//     if ((*str & 0x80) == 0) return 1; // 1 byte
//     if ((*str & 0xE0) == 0xC0) return 2; // 2 bytes
//     if ((*str & 0xF0) == 0xE0) return 3; // 3 bytes
//     if ((*str & 0xF8) == 0xF0) return 4; // 4 bytes
//     return -1; // Invalid UTF-8
// }

void st7789_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font) // 自动识别并混合显示中英文字符
{
    while (*str)
    {
        // int len = utf8_char_length(*str);
        int len = is_gb2312(*str) ? 2 : 1;
        if (len <= 0)
        {
            str++;
            continue;
        }
        else if (len == 1)
        {
            st7789_write_ascii(x, y, *str, color, bg_color, font);
            str++;
            x += font->size / 2;
        }
        else
        {
            char ch[5];
            strncpy(ch, str, len);
            ch[len] = '\0';
            st7789_write_chinese(x, y, ch, color, bg_color, font);
            str += len;
            x += font->size;
        }
    }
}

void st7789_draw_image(uint16_t x, uint16_t y, const image_t *image) // 绘制图片
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT ||
        x + image->width - 1 >= ST7789_WIDTH || y + image->height - 1 >= ST7789_HEIGHT)
        return;

    st7789_set_range_and_prepare_gram(x, y, x + image->width - 1, y + image->height - 1);

    st7789_write_gram((uint8_t *)image->data, image->width * image->height * 2, false);
}

void DMA1_Stream4_IRQHandler(void)
{ //pdFALSE表示没有更高优先级任务被唤醒，pdTRUE表示有更高优先级任务被唤醒
  if(DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET)//DMA_IT_TCIF4是否传输完成
  {
    BaseType_t pxHigherPriorityTaskWoken;// 用于存储是否有更高优先级的任务被唤醒
    
    xSemaphoreGiveFromISR(write_gram_semaphore, &pxHigherPriorityTaskWoken);
    //if (被唤醒的任务优先级 > 当前任务优先级) {*pxHigherPriorityTaskWoken = pdTRUE;}

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);// 如果有更高优先级任务被唤醒，就切换到更高优先级任务

    DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
  }
}
