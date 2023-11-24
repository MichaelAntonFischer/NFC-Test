/**************************************************************************/
/*! 
    @file     readntag203.pde
    @author   KTOWN (Adafruit Industries)
    @license  BSD (see license.txt)

    This example will wait for any NTAG424 card or tag,
    and will attempt to read the NDEF file from it.

*/
/**************************************************************************/
#define NTAG424DEBUG

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532_NTAG424.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

// If using the breakout with SPI, define the pins for SPI communication.
/*
#define PN532_SCK  (18)
#define PN532_MOSI (23)
#define PN532_SS   (5)
#define PN532_MISO (19)
*/
// NFC Pins

// If using the breakout with SPI, define the pins for SPI communication.
//#define PN532_SCK (17)
//#define PN532_MOSI (13)
//#define PN532_SS (15)
//#define PN532_MISO (12)


// This is optional to powerup/down the PN532-board.
// For RSTPD_N to work i had to desolder a 10k resistor between RSTPD_N and VCC
//#define PN532_RSTPD_N (2)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define NFC_SDA    13
#define NFC_SCL    15
#define NFC_IRQ    26
#define NFC_RST    25

Adafruit_PN532 *nfc = NULL;
PN532_I2C *pn532_i2c = NULL;
PN532 *pn532 = NULL;
NfcAdapter *nfcAdapter = NULL;

// Uncomment just _one_ line below depending on how your breakout or shield
// is connected to the Arduino:

// Use this line for a breakout with a software SPI connection (recommended):
//Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Use this line for a breakout with a hardware SPI connection.  Note that
// the PN532 SCK, MOSI, and MISO pins need to be connected to the Arduino's
// hardware SPI SCK, MOSI, and MISO pins.  On an Arduino Uno these are
// SCK = 13, MOSI = 11, MISO = 12.  The SS line can be any digital IO pin.
//Adafruit_PN532 nfc(PN532_SS);

// Or use this line for a breakout or shield with an I2C connection:

void scanDevices(TwoWire *w)
{
    uint8_t err, addr;
    int nDevices = 0;
    uint32_t start = 0;
    for (addr = 1; addr < 127; addr++) {
        start = millis();
        w->beginTransmission(addr); delay(2);
        err = w->endTransmission();
        delay(10);
        if (err == 0) {
            nDevices++;
            String message = "[nfcTask] I2C device found at address 0x";
            if (addr < 16) {
                message += "0";
            }
            message += String(addr, HEX);
            Serial.println(message);
            break;

        } else if (err == 4) {
            String message = "[nfcTask] Unknown error at address 0x";
            if (addr < 16) {
                message += "0";
            }
            message += String(addr, HEX);
            Serial.println(message);
        }
    }
    if (nDevices == 0)
        Serial.println("[nfcTask] No I2C devices found\n");
}

void setup(void) {
    Serial.begin(115200);
    Wire.begin(NFC_SDA, NFC_SCL);
    Wire.setClock(10000);
    nfc = new Adafruit_PN532(NFC_SDA, NFC_SCL);
    pn532_i2c = new PN532_I2C(Wire);
    pn532 = new PN532(*pn532_i2c);
    nfcAdapter = new NfcAdapter(*pn532_i2c);
    nfc->begin();
    nfc->SAMConfig();
    nfc->SAMConfig();
    scanDevices(&Wire);
    int error = Wire.endTransmission();
    if (error == 0) {
        uint32_t versiondata = nfc->getFirmwareVersion();
        if (! versiondata) {
            Serial.println("[nfcTask] Didn't find PN53x board");
        }
        // Got ok data, print it out!
        String message = "[nfcTask] Found chip PN5" + String((versiondata >> 24) & 0xFF, HEX);
        Serial.println(message);
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 16) & 0xFF, DEC);
        Serial.println(message);
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 8) & 0xFF, DEC);
        Serial.println(message);
    } else {
        String message = "[nfcTask] Wire error: " + String(error);
        Serial.println(message);
    } 
}

void loop(void) {
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    Serial.println("Waiting for tag...");
    success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 420);
    
    if (success) {
        uint8_t cardType = nfc->ntag424_isNTAG424();
        if (cardType) {
            uint8_t data[256];
            uint8_t bytesread = nfc->ntag424_ISOReadFile(data);
            // Extract URL from data
            String url = String((char*)data);
            Serial.println("URL from NTAG424: " + url);
        } else {
            if (nfcAdapter->tagPresent()) {
                NfcTag tag = nfcAdapter->read();
                NdefMessage message = tag.getNdefMessage();
                for (int i = 0; i < message.getRecordCount(); i++) {
                    NdefRecord record = message.getRecord(i);
                    int payloadLength = record.getPayloadLength();
                    byte payload[payloadLength];
                    record.getPayload(payload);
                    // payload is a byte array containing the data from the record
                    // Convert byte array to string
                    String payloadAsString = "";
                    for (int j = 0; j < payloadLength; j++) {
                        payloadAsString += (char)payload[j];
                    }
                    Serial.println("Payload from NDEF: " + payloadAsString);
                }
            }
        }
    }
}

