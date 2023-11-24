#ifndef NFC_H
#define NFC_H

#include <Wire.h>
#include "Adafruit_PN532_NTAG424.h"
#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "FreeRTOSConfig.h"
#include "screen.h"
#include "logger.h"

#define NFC_SDA    13
#define NFC_SCL    15
#define NFC_IRQ    26
#define NFC_RST    25

extern TFT_eSPI tft;  
extern bool isRfOff;
extern TaskHandle_t nfcTaskHandle;
extern EventGroupHandle_t nfcEventGroup;
extern EventGroupHandle_t appEventGroup;

extern String lnurlwNFC;

extern std::string qrcodeData;

bool initNFC(Adafruit_PN532** nfc);
void printTaskState(TaskHandle_t taskHandle);
void setRFoff(bool turnOff, Adafruit_PN532* nfc);
void nfcTask(void *args);
void scanDevices(TwoWire *w);
void printRecordPayload(const uint8_t* payload, size_t len);
std::string decodeUriPrefix(uint8_t prefixCode);
bool isLnurlw(String url);
void idleMode1(Adafruit_PN532 *nfc, EventBits_t uxBits, const EventBits_t uxAllBits);
void idleMode2(Adafruit_PN532 *nfc);
bool readAndProcessNFCData(Adafruit_PN532 *nfc, uint8_t *uid, uint8_t uidLength, int &readAttempts);

#endif