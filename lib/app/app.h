#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include "power.h"
#include "logger.h"
#include "screen.h"
#include "keypad.h"
#include "util.h"
#include "config.h"
#include "cache.h"
#include "payment.h"
#include "lnurl.h"
#include "withdraw_lnurlw.h"

extern bool lnurlwNFCFound;
extern String lnurlwNFC;
extern QueueHandle_t appQueue;
extern uint16_t amountCentsDivisor;
extern unsigned short maxNumKeysPressed;
extern std::string qrcodeData;

void appendToKeyBuffer(const std::string &key);
std::string leftTrimZeros(const std::string &keys);
double keysToAmount(const std::string &t_keys);
void handleSleepMode();
void appTask(void* pvParameters);

#endif // APP_H