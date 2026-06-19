#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "st7789.h"
#include "FreeRTOS.h"
#include "task.h"
#include "workqueue.h"
#include "ui.h"
#include "app.h"
#include "page.h"

extern void board_lowlevel_init(void);
extern void board_init(void);

static void main_init(void *pararm)
{
	board_init();	
	
	ui_init();

	welcome_page_display();

	wifi_init();

	wifi_page_display();
	wifi_wait_connect(); 

	mloop_init();
  
	main_page_display();

	vTaskDelete(NULL); // 任务删除后，任务栈会被释放
}

int main(void)
{
	board_lowlevel_init();
	
	workqueue_init();

	xTaskCreate(main_init, "main_init", 1024, NULL, 9, NULL);

	vTaskStartScheduler();

	while(1)
	{
		;
	}
}
