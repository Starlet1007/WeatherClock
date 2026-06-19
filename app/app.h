#ifndef __APP_H
#define __APP_H

#define APP_VERSION "v1.0"
#define WIFI_SSID "iPhone"
#define WIFI_PASSWD "22222222"

void wifi_init(void);
void wifi_wait_connect(void);
void mloop_init(void);

#endif /*__APP_H*/
