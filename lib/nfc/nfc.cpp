#include "nfc.h"

void printTaskState(TaskHandle_t taskHandle) {
    eTaskState taskState = eTaskGetState(taskHandle);

    Serial.print("Task state: ");

    switch(taskState) {
        case eReady:
            logger::write("Task is ready to run");
            break;
        case eRunning:
            logger::write("Task is currently running");
            break;
        case eBlocked:
            logger::write("Task is blocked");
            break;
        case eSuspended:
            logger::write("Task is suspended");
            break;
        case eDeleted:
            logger::write("Task is being deleted");
            break;
        default:
            logger::write("Unknown state");
            break;
    }
}

bool initNFC(Adafruit_PN532** nfc) {
    logger::write("[nfcTask] Initializing NFC ...");
    // Initialize the I2C bus with the correct SDA and SCL pins
    Wire.begin(NFC_SDA, NFC_SCL);
    Wire.setClock(10000);
    // Initialize the Adafruit_PN532 object with the initialized Wire object
    *nfc = new Adafruit_PN532(NFC_SDA, NFC_SCL);
    // Use the nfc pointer to call begin() and SAMConfig()
    // Try to initialize the NFC reader
    (*nfc)->begin();
    // Reset the NFC reader
    (*nfc)->reset();
    (*nfc)->SAMConfig();
    scanDevices(&Wire);
    int error = Wire.endTransmission();
    if (error == 0) {
        uint32_t versiondata = (*nfc)->getFirmwareVersion();
        if (! versiondata) {
            logger::write("[nfcTask] Didn't find PN53x board");
            return false;
        }
        // Got ok data, print it out!
        String message = "[nfcTask] Found chip PN5" + String((versiondata >> 24) & 0xFF, HEX);
        logger::write(message.c_str());
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 16) & 0xFF, DEC);
        logger::write(message.c_str());
        message = "[nfcTask] Firmware ver. " + String((versiondata >> 8) & 0xFF, DEC);
        logger::write(message.c_str());
        //setRFoff(true, *nfc);
        return true;
    } else {
        String message = "[nfcTask] Wire error: " + String(error);
        logger::write(message.c_str());
        return false;
    } 
}

void setRFoff(bool turnOff, Adafruit_PN532* nfc) {
    // Check the desired state
    if (turnOff && !isRfOff) {
        logger::write("[nfcTask] Powering down RF");
        // Try to turn off RF
        if (nfc->turnOffRF()) {
            // If RF is successfully turned off, set the flag
            logger::write("[nfcTask] RF is off");
            isRfOff = true;
        } else {
            logger::write("[nfcTask] Error powering down RF");
        }
    } else if (!turnOff && isRfOff) {
        logger::write("[nfcTask] Powering up RF");
        // Try to turn on RF
        if (nfc->turnOnRF()) {
            // If RF is successfully turned on, set the flag
            logger::write("[nfcTask] RF is on");
            isRfOff = false;
        } else {
            logger::write("[nfcTask] Error powering up RF");
        }
    } else {
        logger::write("[nfcTask] RF already in desired state");
    }
}

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
            logger::write(message.c_str());
            break;

        } else if (err == 4) {
            String message = "[nfcTask] Unknown error at address 0x";
            if (addr < 16) {
                message += "0";
            }
            message += String(addr, HEX);
            logger::write(message.c_str());
        }
    }
    if (nDevices == 0)
        logger::write("[nfcTask] No I2C devices found\n");
}

bool isLnurlw(void) {
    logger::write("[nfcTask] Checking if URL is lnurlw", "debug");
    if (lnurlwNFC.startsWith("lnurlw")) {
        lnurlwNFC.replace("lnurlw", "https");
    } else if (lnurlwNFC.startsWith("http") && !lnurlwNFC.startsWith("https")) {
        lnurlwNFC.replace("http", "https");
    }
    // Check if the URL starts with "https://" 
    if (lnurlwNFC.startsWith("https://")) {
        logger::write("[nfcTask] URL is lnurlw", "debug");
        return true;
    } else {
        logger::write("[nfcTask] URL is not lnurlw", "debug");
        return false;
    }
}

void idleMode1(Adafruit_PN532 *nfc, EventBits_t uxBits, const EventBits_t uxAllBits)
{
    // logger::write("[nfcTask] Bit 1 is set. Going into idle mode 1.", "debug");
    // logger::write("[nfcTask] Checking RF status", "debug");
    setRFoff(true, nfc);
    if (!isRfOff) 
    {
        xEventGroupClearBits(appEventGroup, 0xFF);
        xEventGroupSetBits(appEventGroup, (1<<0)); // NFC task is active
        logger::write("[nfcTask] NFC task is active", "info");
        taskYIELD();
        return;
    }
    xEventGroupClearBits(appEventGroup, 0xFF); // Clear all bits first
    xEventGroupSetBits(appEventGroup, (1<<1) | (1<<4)); // NFC task is idle, RF is off and transition to idle mode 1 is complete
    logger::write("[nfcTask] NFC task is idle and RF is off", "info");
    while ((uxBits & (1 << 0)) == 0) 
    {
        uxBits = xEventGroupWaitBits(nfcEventGroup, uxAllBits, pdFALSE, pdFALSE, 0);
    }
    // logger::write("[nfcTask] Checking RF status", "debug");
    setRFoff(false, nfc);
    if (isRfOff) 
    {
        xEventGroupClearBits(appEventGroup, 0xFF);
        xEventGroupSetBits(appEventGroup, (1<<1) | (1<<4)); // NFC task is idle and RF is off
        logger::write("[nfcTask] NFC task is idle and RF is off (Idle mode 1)", "info");
        taskYIELD();
        return;
    }
    xEventGroupClearBits(appEventGroup, 0xFF);
    xEventGroupSetBits(appEventGroup, (1<<0)); // NFC task is active
    // logger::write("[nfcTask] NFC task is active", "debug");
    taskYIELD();
}

void idleMode2(Adafruit_PN532 *nfc)
{
    // logger::write("[nfcTask] Bit 2 is set. Going into idle mode 2.", "debug");
    // logger::write("[nfcTask] Checking RF status", "debug");
    setRFoff(true, nfc);
    if (!isRfOff) 
    {
        xEventGroupClearBits(appEventGroup, 0xFF);
        xEventGroupSetBits(appEventGroup, (1<<0)); // NFC task is active
        // logger::write("[nfcTask] NFC task is active", "debug");
        taskYIELD();
        return;
    }
    logger::write("[nfcTask] NFC reader turned off, returning to NFC idling mode", "info");
    xEventGroupClearBits(appEventGroup, 0xFF);
    xEventGroupSetBits(appEventGroup, (1<<2) | (1<<4)); // NFC task is idle, RF is off and transition to idle mode 2 is complete
    taskYIELD();
}

bool readAndProcessNFCData(Adafruit_PN532 *nfc, uint8_t *uid, uint8_t uidLength, int &readAttempts)
{
    if ((uidLength == 7) || (uidLength == 4)) 
    {
        if (nfc->ntag424_isNTAG424()) 
        {
            logger::write("[nfcTask] Detected card type: NTAG424", "info");
        }
        else
        {
            logger::write("[nfcTask] Detected card type: Not NTAG424", "info");
        }
        uint8_t fileData[256];
        uint8_t bytesread = nfc->ntag424_ISOReadFile(fileData);
        if (bytesread) 
        {
            fileData[bytesread] = 0;
            String lnurl = (char*)fileData;
            logger::write(("[nfcTask] Response: " + lnurl).c_str(), "info");
            xEventGroupClearBits(appEventGroup, 0xFF);
            while (!isRfOff) 
            {
                setRFoff(true, nfc); // Attempt to switch off RF
            }
            xEventGroupClearBits(appEventGroup, 0xFF);
            xEventGroupSetBits(appEventGroup, (1<<6) | (1<<2)); // A file has been read and RF is off
            screen::showNFCsuccess();
            return true;
        }
        else 
        {
            logger::write("[nfcTask] Unable to read from the tag.", "info");
            readAttempts++;
            if (readAttempts >= 12) 
            {
                setRFoff(true, nfc); // Switch off RF
                if (isRfOff) 
                {
                    xEventGroupClearBits(appEventGroup, 0xFF);
                    xEventGroupSetBits(appEventGroup, (1<<5) | (1<<2)); // Signal unsuccessful attempt, RF is off and transition to idle mode is complete
                    screen::showNFCfailed();
                    return false;
                }
            }
        }
    }
    else
    {
        logger::write("[nfcTask] This doesn't seem to be an NTAG424 tag. (UUID length != 7 bytes and UUID length != 4)!", "info");
    }
}

// nfcTask
// The bits in the event groups represent the following states:
// appEventGroup:
// Bit 0 (1 << 0): Indicates nfcTask is actively processing.
// Bit 1 (1 << 1): Signifies Idle Mode 1 for nfcTask.
// Bit 2 (1 << 2): Signifies Idle Mode 2 for nfcTask.
// Bit 3 (1 << 3): Indicates an NFC card detection by nfcTask.
// Bit 4 (1 << 4): Confirmation bit for the successful shutdown of the RF module and transition into idle mode in nfcTask.
// Bit 5 (1 << 5): Indicates that the NFC card was not successfully read or no LNURLw was found.
// Bit 6 (1 << 6): Indicates that the NFC card was successfully read.
// nfcEventGroup:
// Bit 0 (1 << 0): Instructs nfcTask to power up or remain active.
// Bit 1 (1 << 1): Signals nfcTask to enter Idle Mode 1.
// Bit 2 (1 << 2): Commands nfcTask to turn off the RF functionality and transition to idle mode 2. This bit is used particularly after a successful payment is processed.

void nfcTask(void *args) 
{
    logger::write("[nfcTask] Starting NFC reader", "debug");
    bool initFlag = false;
    bool tagDetected;
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
    uint8_t uidLength;
    Adafruit_PN532 *nfc = NULL;
    if (!initFlag) 
    {
        logger::write("[nfcTask] Initializing NFC", "debug");
        initNFC(&nfc);
        vTaskSuspend(NULL);
    }
    EventBits_t uxBits;
    const EventBits_t uxAllBits = (1<<0) | (1<<1) | (1<<2);
    int loopCounter = 0;
    while (1) 
    {
        logger::write("[nfcTask] Starting main loop", "debug");
        uxBits = xEventGroupWaitBits(nfcEventGroup, uxAllBits, pdFALSE, pdFALSE, 0);
        logger::write(("[nfcTask] Received bits: " + String(uxBits, BIN)).c_str(), "debug");
        
        // NFC task is active
        xEventGroupClearBits(appEventGroup, 0xFF); // Clear all bits first
        xEventGroupSetBits(appEventGroup, (1<<0));
        logger::write("[nfcTask] NFC task is active", "debug");

        if ((uxBits & (1 << 0)) != 0) 
        {
            logger::write("[nfcTask] Checking RF status", "debug");
            setRFoff(false, nfc);
            if (isRfOff) 
            {
                xEventGroupClearBits(appEventGroup, 0xFF); // Clear all bits
                xEventGroupSetBits(appEventGroup, (1<<2) | (1<<4)); // NFC RF is off
                logger::write("[nfcTask] NFC task is idle and RF is off (Idle mode 2)", "debug");
                taskYIELD();
                continue;
            }
            logger::write("[nfcTask] Waiting for NFC tag", "debug");
            int readAttempts = 0;
            int noTagCounter = 0;
            while (1) 
            {
                uxBits = xEventGroupWaitBits(nfcEventGroup, uxAllBits, pdFALSE, pdFALSE, 0);
                logger::write(("[nfcTask] Received bits: " + String(uxBits, BIN)).c_str(), "debug");
                if ((uxBits & (1 << 1)) != 0) 
                {
                    idleMode1(nfc, uxBits, uxAllBits);
                } 
                else if ((uxBits & (1 << 2)) != 0) 
                {
                    idleMode2(nfc);
                } 
                else 
                {
                    logger::write("[nfcTask] No shutdown signal", "debug");
                }
                if ((uxBits & (1 << 0)) == 0) 
                {
                    break;
                }
                  
                // Wait for an ISO14443A type cards (Mifare, etc.). When one is found
                // 'uid' will be populated with the UID, and uidLength will indicate
                // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
                logger::write("[nfcTask] Checking for tag", "debug");
                success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 420);
                
                if (success) {
                    // We seem to have a tag present
                    logger::write("[nfcTask] Tag detected.", "info");
                    screen::showNFC();
                    tagDetected = true;
                } else {
                    // No tag present
                    logger::write("[nfcTask] No tag detected.", "debug");
                    tagDetected = false;
                }
                if (Wire.endTransmission() != 0)
                {
                    logger::write("[nfcTask] NFC reader cannot be contacted. Killing the task...", "debug");
                    vTaskDelete(NULL);
                }
                if (tagDetected) 
                {
                    noTagCounter = 0; // Reset the no tag counter as a tag has been detected
                    logger::write("[nfcTask] Detected NFC tag", "info");
                    xEventGroupClearBits(appEventGroup, 0xFF);
                    xEventGroupSetBits(appEventGroup, (1<<3)); // NFC card has been detected
                    
                    // Reading and processing the NFC data
                    logger::write("[nfcTask] Reading and processing NFC data", "info");
                    bool result = readAndProcessNFCData(nfc, uid, uidLength, readAttempts);
                    if (result) 
                    {
                        logger::write("[nfcTask] NFC card reading exited with success", "info");
                    } 
                    else 
                    {
                        logger::write("[nfcTask] NFC card reading exited with failed", "info");
                    }
                    xEventGroupSetBits(nfcEventGroup, (1<<2)); // Signal to enter idle mode 2
                    
                    break;
                }
                else 
                {
                    noTagCounter++;
                    if (noTagCounter > 50) // If no tag is detected for 5 seconds
                    {
                        xEventGroupClearBits(appEventGroup, (1<<3)); // NFC card has not been detected
                        logger::write("[nfcTask] No NFC tag detected");
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                }     
            }
        }
}







