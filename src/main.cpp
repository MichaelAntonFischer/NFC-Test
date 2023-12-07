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

String lnurlwNFC = "";
bool isRfOff = false;
bool lnurlwFound = false;

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

bool oldInit(PN532_I2C** pn532_i2c, PN532** nfc) {
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    // Initialize the IRQ pin as input
    pinMode(NFC_IRQ, INPUT);
    // Initialize the RST pin as output
    pinMode(NFC_RST, OUTPUT);
    // Set the RST pin to HIGH to reset the module
    digitalWrite(NFC_RST, HIGH);
    vTaskDelay(21);
    // Set the RST pin to LOW to finish the reset
    digitalWrite(NFC_RST, LOW);
    //logger::write("Initializing NFC ...");
    // Initialize the I2C bus with the correct SDA and SCL pins
    Wire.begin(NFC_SDA, NFC_SCL);
    //Wire.setClock(10000);
    // Initialize the PN532_I2C object with the initialized Wire object
    *pn532_i2c = new PN532_I2C(Wire);
    // Initialize the PN532 object with the initialized PN532_I2C object
    *nfc = new PN532(**pn532_i2c);
    // Use the nfc pointer to call begin() and SAMConfig()
    // Try to initialize the NFC reader
    (*nfc)->begin();
    (*nfc)->SAMConfig();
    scanDevices(&Wire);
    if (Wire.endTransmission() == 0) {
        uint32_t versiondata = (*nfc)->getFirmwareVersion();
        if (! versiondata) {
            Serial.print("Didn't find PN53x board");
        }
        // Got ok data, print it out!
        Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
        Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
        Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
        //setRFoff(true, *pn532_i2c);
        return true;
    } else {
        Serial.println("Didn't find PN53x board");
        return false;
    } 
}

void setRFoff(bool turnOff, PN532_I2C* pn532_i2c) {
    uint8_t commandRFoff[3] = {0x32, 0x01, 0x00}; // RFConfiguration command to turn off the RF field
    uint8_t commandRFon[7] = { 0x02, 0x02, 0x00, 0xD4, 0x02, 0x2A, 0x00 };
    // Check the desired state
    if (turnOff && !isRfOff) {
        Serial.println("[nfcTask] Powering down RF");
        // Try to turn off RF
        if (pn532_i2c->writeCommand(commandRFoff, sizeof(commandRFoff)) == 0) {
            // If RF is successfully turned off, set the flag
            Serial.println("[nfcTask] RF is off");
            isRfOff = true;
            Serial.println("[nfcTask] RF is off - Flag set");
        } else {
            Serial.println("[nfcTask] Error powering down RF");
        }
    } else if (!turnOff && isRfOff) {
        Serial.println("[nfcTask] Powering up RF");
        // Try to turn on RF
        vTaskDelay(21);
        if (pn532_i2c->writeCommand(commandRFon, sizeof(commandRFon)) == 0) {
            // If RF is successfully turned on, clear the flag
            Serial.println("[nfcTask] RF is on");
            isRfOff = false;
        } else {
            Serial.println("[nfcTask] Error powering up RF");
            ESP.restart();
        }
    } else {
        Serial.println("[nfcTask] RF already in desired state");
    }
}

bool initNFC(PN532_I2C** pn532_i2c, Adafruit_PN532** nfc, PN532** pn532, NfcAdapter** nfcAdapter) {
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    // Initialize the IRQ pin as input
    pinMode(NFC_IRQ, INPUT);
    // Initialize the RST pin as output
    pinMode(NFC_RST, OUTPUT);
    // Set the RST pin to HIGH to reset the module
    digitalWrite(NFC_RST, HIGH);
    vTaskDelay(21);
    // Set the RST pin to LOW to finish the reset
    digitalWrite(NFC_RST, LOW);
    // Initialize the I2C bus with the correct SDA and SCL pins
    Wire.begin(NFC_SDA, NFC_SCL);
    Wire.setClock(10000);
    // Initialize the PN532_I2C object with the initialized Wire object
    *pn532_i2c = new PN532_I2C(Wire);
    // Initialize the PN532 object with the initialized PN532_I2C object
    *pn532 = new PN532(**pn532_i2c);
    //initialize NFC Adapter object
    *nfcAdapter = new NfcAdapter(**pn532_i2c);
    // Initialize the Adafruit_PN532 object with the initialized PN532_I2C object
    *nfc = new Adafruit_PN532(NFC_SDA, NFC_SCL);
    // Use the  pointer to call begin() and SAMConfig()
    // Try to initialize the NFC reader
    (*pn532)->begin();
    (*pn532)->SAMConfig();
    scanDevices(&Wire);
    byte error = Wire.endTransmission();
    if (error == 0) {
        uint32_t versiondata = (*pn532)->getFirmwareVersion();
        if (! versiondata) {
            Serial.println("[nfcTask] Didn't find PN53x board");
            return false;
        }
        // Got ok data, print it out!
        String message = "[nfcTask] Found chip PN5" + String((versiondata >> 24) & 0xFF, HEX);
        Serial.println(message.c_str());
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 16) & 0xFF, DEC);
        Serial.println(message.c_str());
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 8) & 0xFF, DEC);
        Serial.println(message.c_str());
        setRFoff(true, *pn532_i2c);
        return true;
    } else {
        String message = "[nfcTask] Wire error: " + String(error);
        Serial.println(message.c_str());
        return false;
    } 
}

bool isLnurlw(void) {
    Serial.println("[nfcTask] Checking if URL is lnurlw");
    if (lnurlwNFC.startsWith("lnurlw")) {
        lnurlwNFC.replace("lnurlw", "https");
    } else if (lnurlwNFC.startsWith("http") && !lnurlwNFC.startsWith("https")) {
        lnurlwNFC.replace("http", "https");
    }
    // Check if the URL starts with "https://" 
    if (lnurlwNFC.startsWith("https://")) {
        Serial.println("[nfcTask] URL is lnurlw");
        return true;
    } else {
        Serial.println("[nfcTask] URL is not lnurlw");
        return false;
    }
}

bool readAndProcessNFCData(PN532_I2C *pn532_i2c, PN532 *pn532, Adafruit_PN532 *nfc, NfcAdapter *nfcAdapter, int &readAttempts)
{
    NdefMessage message;
    int recordCount;
    uint8_t cardType = nfc->ntag424_isNTAG424();
    if (cardType) 
    {
        uint8_t data[256];
        uint8_t bytesread = nfc->ntag424_ISOReadFile(data);
        // Extract URL from data
        lnurlwNFC = String((char*)data);
        Serial.println("URL from NTAG424: " + lnurlwNFC);
        if (isLnurlw()) 
        {
            while (!isRfOff) 
            {
                setRFoff(true, pn532_i2c); // Attempt to switch off RF
            }
            lnurlwFound = true;
            return true;
        }
    }
    else
    {
        NfcTag tag = nfcAdapter->read();
        String tagType = tag.getTagType();
        Serial.println("[nfcTask] Tag type read");
        Serial.println("Tag Type: " + tagType);
        if (tag.hasNdefMessage()) 
        {
            message = tag.getNdefMessage();
            recordCount = message.getRecordCount();
        }
        bool lnurlwFound = false;
        for (int i = 0; i < recordCount && !lnurlwFound; i++) 
        {
            NdefRecord record = message.getRecord(i);
            String recordType = record.getType();
            String logMessage = "Record Type: " + recordType;
            Serial.println(logMessage);
            if (recordType == "U" || recordType == "T") 
            { 
                uint8_t payload[record.getPayloadLength() + 1] = {0}; // Added +1 to the size and initialized to 0 to ensure null termination
                record.getPayload(payload);
                String recordPayload;
                if (recordType == "U") 
                {
                    switch (payload[0]) 
                    {
                        case 0x01:
                            recordPayload = "http://www.";
                            break;
                        case 0x02:
                            recordPayload = "https://www.";
                            break;
                        case 0x03:
                            recordPayload = "http://";
                            break;
                        case 0x04:
                            recordPayload = "https://";
                            break;
                        default:
                            recordPayload = String((char*)payload);
                            break;
                    }
                    recordPayload += String((char*)&payload[1]);
                } 
                else 
                {
                    for (int i = 0; i < record.getPayloadLength(); i++) 
                    {
                        recordPayload += (char)payload[i];
                    }
                }
                lnurlwNFC = recordPayload;
                String logMessage = "Record Payload: " + recordPayload;
                Serial.println(logMessage);
                if (isLnurlw()) 
                {
                    while (!isRfOff) 
                    {
                        setRFoff(true, pn532_i2c); // Attempt to switch off RF
                    }
                    lnurlwFound = true;
                    return true;
                }
            }
        }
    }
    lnurlwNFC = ""; // Reset the global variable if it's not lnurlw
    readAttempts++;
    if (readAttempts >= 12) 
    {
        setRFoff(true, pn532_i2c); // Switch off RF
        if (isRfOff) 
        {
            return false;
        }
    }
    return false;
}

void setup(void) {
    Serial.begin(115200);
    while (!initNFC(&pn532_i2c, &nfc, &pn532, &nfcAdapter)) {
        Serial.println("[nfcTask] Failed to initialize NFC");
    }
}

void loop(void) {
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    Serial.println("Waiting for tag...");
    success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 420);
    
    if (success) {
        int readAttempts = 0;
        bool cardRead = readAndProcessNFCData(pn532_i2c, pn532, nfc, nfcAdapter, readAttempts);
        if (cardRead) {
            Serial.println("Card read successfully.");
        } else {
            Serial.println("Failed to read card.");
        }
    }
}

