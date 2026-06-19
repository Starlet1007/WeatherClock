#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "esp_at.h"
#include "page.h"


void wifi_init(void)
{
	if(!esp_at_init())
    {
        printf("[AT] init Failed\r\n");
        goto err;
    }

    if(!esp_at_wifi_init())
    {
        printf("[WIFI] init Failed\r\n");
        goto err;
    }

    if(!esp_at_sntp_init())
    {
        printf("[SNTP] init Failed\r\n");
        goto err;
    }
	printf("[SNTP] init\r\n");
		
	return;
		
		
err:
	error_page_display("Wireless Inir Failed");
	while(1)
	{
		;
	}
}

void wifi_wait_connect(void)
{		
	printf("[WIFI] connecting\n");
	esp_at_connect_wifi("iPhone", "22222222", NULL);
    
	for(uint32_t t = 0; t < 10*1000; t += 100)
	{
		vTaskDelay(pdMS_TO_TICKS(100));
		esp_wifi_info_t wifi = {0};
    if(esp_at_get_wifi_info(&wifi) && wifi.connected)
    {
        printf("[WIFI] Connected\r\n");
		printf("[WIFI] SSID: %s, RSSI: %d, CHANNEL: %d, BSSID: %s\r\n", wifi.ssid, wifi.rssi, wifi.channel, wifi.bssid);
        return;
    }
	}
	printf("[WIFI] Connection Timeout\n");
	error_page_display("Wireless Inir Failed");
	while(1)
	{
		;
	}
}		
