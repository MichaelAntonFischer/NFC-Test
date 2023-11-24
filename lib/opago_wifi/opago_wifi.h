#include <WiFi.h>
#include "config.h"

extern bool onlineStatus;
extern SemaphoreHandle_t wifiSemaphore;

void connectToWiFi(const char* ssid, const char* password);
bool isConnectedToWiFi();
void connectToWiFiTask(void* pvParameters);
