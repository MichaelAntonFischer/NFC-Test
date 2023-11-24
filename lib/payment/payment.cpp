#include "payment.h"


std::string parseCallbackUrl(const std::string &response) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    std::string callbackUrl = "";

    if (doc["callback"].is<std::string>()) {
        callbackUrl = doc["callback"].as<std::string>();
    } else if (doc["callback"]["_url"].is<std::string>()) {
        callbackUrl = doc["callback"]["_url"].as<std::string>();
    }

    if (callbackUrl.empty()) {
        screen::showX();
        delay(2100);
        logger::write("Callback URL not found in response");
    }

    return callbackUrl;
}

std::string parseInvoice(const std::string &json) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        logger::write("Failed to parse JSON: " + std::string(error.c_str()));
        return "";
    }

    const char *invoice = doc["pr"];
    if (invoice) {
        return std::string(invoice);
    } else {
        logger::write("Invoice not found in response");
        return "";
    }
}

std::string requestInvoice(const std::string &url) {
    HTTPClient http;
    http.begin(url.c_str());
    int httpResponseCode = http.GET();
    logger::write("HTTP Response code: " + std::to_string(httpResponseCode));

    if (httpResponseCode == 200) {
        std::string response = http.getString().c_str();
        logger::write("HTTP Response: " + response);
        
        // Parse the callback URL from the response
        std::string callbackUrl = parseCallbackUrl(response);
        logger::write("Callback URL: " + callbackUrl);

        http.end();  // Close the first connection

        // Start the second request with the callback URL
        http.begin(callbackUrl.c_str());
        httpResponseCode = http.GET();

        if (httpResponseCode == 200) {
            response = http.getString().c_str();
            // Parse the invoice from the second response
            std::string invoice = parseInvoice(response);
            logger::write("Invoice: " + invoice);
            http.end();  // Close the connection
            return invoice;
        } else {
            // Handle error
            logger::write("Error on HTTP request for invoice: " + std::to_string(httpResponseCode));
        }

    } else {
        // Handle error
        logger::write("Error on HTTP request for callback URL: " + std::to_string(httpResponseCode));
    }

    http.end();
    return "";
}

std::string fetchPaymentHash(const std::string &lnurl) {
    HTTPClient http;
    http.begin("https://lnbits.opago-pay.com/api/v1/payments/decode");
    http.addHeader("Content-Type", "application/json");

    // Create the request body
    String requestBody = "{ \"data\": \"" + String(lnurl.c_str()) + "\" }";

    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode == 200) {
        std::string response = http.getString().c_str();
        logger::write("Response from server: " + response);
        logger::write("Payment Hash Fetch Response: " + std::string(response));
        // Parse the response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            logger::write("Failed to parse JSON from decode response: " + std::string(error.c_str()));
            return "";
        }
        
        // Get the payment hash
        const char* paymentHash = doc["payment_hash"];
        if (!paymentHash) {
            logger::write("Payment hash not found in decode response");
            return "";
        }

        logger::write("Payment hash: " + std::string(paymentHash));
        http.end();  // Close the connection
        return std::string(paymentHash);
    } else {
        // Handle error
        logger::write("Error on HTTP request for decoding invoice: " + std::to_string(httpResponseCode));
    }

    http.end();
    return "";
}

bool isPaymentMade(const std::string &paymentHash, const std::string &apiKey) {
    // Check if Wi-Fi is connected
    if (WiFi.status() != WL_CONNECTED) {
        logger::write("Wi-Fi is not connected");
        return false;
    }

    // Check if paymentHash is not empty
    if (paymentHash.empty()) {
        logger::write("Payment hash is empty");
        return false;
    }

    // Check if apiKey is not empty
    if (apiKey.empty()) {
        logger::write("API key is empty");
        return false;
    }

    // Try to take the wifi semaphore before accessing wifi
    HTTPClient* http = nullptr;
    if(xSemaphoreTake(wifiSemaphore, portMAX_DELAY) == pdTRUE) {
        http = new HTTPClient();
        std::string url = "https://lnbits.opago-pay.com/api/v1/payments/" + paymentHash;
        http->begin(url.c_str());

        http->addHeader("accept", "application/json");
        http->addHeader("X-Api-Key", apiKey.c_str());

        int httpResponseCode = http->GET();

        if (httpResponseCode == 200) {
            std::string response = http->getString().c_str();
            logger::write("HTTP Response: " + response);

            // Parse the response
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, response);

            if (error) {
                logger::write("Failed to parse JSON from payment check response: " + std::string(error.c_str()));
                http->end();
                xSemaphoreGive(wifiSemaphore); // Release the wifi semaphore after accessing wifi
                delete http;
                return false;
            }

            // Get the payment status
            bool paid = doc["paid"];
            http->end();  // Close the connection
            xSemaphoreGive(wifiSemaphore); // Release the wifi semaphore after accessing wifi
            delete http;
            return paid;
        } else {
            // Handle error
            logger::write("Error on HTTP request for payment check: " + std::to_string(httpResponseCode));
        }

        http->end();
        xSemaphoreGive(wifiSemaphore); // Release the wifi semaphore after accessing wifi
    } else {
        logger::write("Failed to take wifi semaphore, aborting payment check.");
    }
    delete http;
    return false;
}

// waitForPaymentOrCancel
// This function waits for a payment to be made or cancelled. It checks various states and signals from the NFC task and the user.
// The function uses two event groups (appEventGroup and nfcEventGroup) to wait for and handle various events.
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
bool waitForPaymentOrCancel(const std::string &paymentHash, const std::string &apiKey, const std::string &invoice) {
    // Initialize variables
    bool paymentisMade = false;
    bool keyPressed = false;
    EventBits_t uxBits;
    const EventBits_t uxAllBits = ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 3 ) | ( 1 << 4 ) | ( 1 << 5 ) | ( 1 << 6 );
    int loopCounter = 0;
    //logger::write("[payment] Free heap memory begin Payment: " + std::to_string(esp_get_free_heap_size()), "debug");
    bool isNFCTaskActive = (nfcTaskHandle != NULL);
    // Uncomment the following line to deactivate NFC
    vTaskDelete(nfcTaskHandle);
    eTaskState nfcTaskState = isNFCTaskActive ? eTaskGetState(nfcTaskHandle) : eDeleted;


    // If the NFC task is active and not deleted, ensure RF is on and NFC is not in any of the idle modes
    if (isNFCTaskActive && nfcTaskState != eDeleted) {
        // Clear idle bits
        //logger::write("[payment] Clearing idle bits on nfcEventGroup", "debug");
        xEventGroupClearBits(nfcEventGroup, (1 << 1) | (1 << 2)); // Clear idle bits
        //logger::write("[payment] Cleared idle bits on nfcEventGroup: " + std::to_string((1 << 1) | (1 << 2)), "debug");
        // Set power up bit
        //logger::write("[payment] Setting power up bit on nfcEventGroup", "debug");
        xEventGroupSetBits(nfcEventGroup, (1 << 0)); // Set power up bit
        //logger::write("[payment] Set power up bit on nfcEventGroup: " + std::to_string((1 << 0)), "debug");
        // Check the state of the NFC task
        nfcTaskState = eTaskGetState(nfcTaskHandle);
        // If the NFC task is not running, resume it
        if (nfcTaskState != eRunning) {
            vTaskResume(nfcTaskHandle); // Resume the NFC task
        }
    }

    // Main loop: wait for payment or cancellation
    while (!paymentisMade) {
        // Wait for bits from appEventGroup
        uxBits = xEventGroupWaitBits(appEventGroup, uxAllBits, pdFALSE, pdFALSE, pdMS_TO_TICKS(420));
        // Handle the received bits
        if ((uxBits & (1 << 3)) != 0) { //card has been detected
            vTaskDelay(pdMS_TO_TICKS(2100));
            continue;
        } else if ((uxBits & (1 << 6)) != 0) { //lnurlw was found on card
            //logger::write("[payment] lnurlw found signal received.", "debug");
            screen::showSand();
            paymentisMade = withdrawFromLnurlw(lnurlwNFC.c_str(), invoice);
            lnurlwNFC = ""; // Clear the lnurlwNFC variable
            if (!paymentisMade) {
                vTaskDelay(pdMS_TO_TICKS(4200));
                paymentisMade = isPaymentMade(paymentHash, apiKey);
                if (!paymentisMade) {
                    screen::showX();
                    vTaskDelay(pdMS_TO_TICKS(2100));
                    // Revert to showing the paymentQR code
                    screen::showPaymentQRCodeScreen(qrcodeData);
                    // Return to the while loop at the beginning of this function
                    continue;
                }
            }
        } else if ((uxBits & (1 << 5)) != 0) { //no lnurlw was found on card and too many incorrect attempts
            screen::showNFCfailed();
            vTaskDelay(pdMS_TO_TICKS(1200));
            screen::showPaymentQRCodeScreen(qrcodeData);
            continue;
        } else { //no card detected
            logger::write("[payment] Checking if paid.", "info");
            //logger::write("[payment] Payment hash: " + paymentHash, "debug");
            //logger::write("[payment] API key: " + apiKey, "debug");
            paymentisMade = isPaymentMade(paymentHash, apiKey);
            logger::write("[payment] Checked if paid.", "info");
            if (!paymentisMade) {
                loopCounter++;
                if (loopCounter >= 3) {
                    logger::write("[payment] Checking for user cancellation", "info");
                    keyPressed = getLongTouch('*');
                    logger::write("[payment] Checked for user cancellation", "info");
                    if (keyPressed) {
                        logger::write("[payment] Payment cancelled by user.", "info");
                        nfcTaskState = isNFCTaskActive ? eTaskGetState(nfcTaskHandle) : eDeleted;
                        if (isNFCTaskActive && nfcTaskState != eSuspended) {
                            vTaskSuspend(nfcTaskHandle); // Suspend the NFC task
                        }
                        return false;
                    }
                    loopCounter = 0; // Reset the loop counter
                    taskYIELD();
                }
            } else {
                logger::write("[payment] Payment made, signaling NFC shutdown.", "info");
                screen::showSuccess();
                // Signal nfcTask to shut down RF and enter Idle Mode 2
                xEventGroupSetBits(nfcEventGroup, (1 << 2)); 

                // Wait for confirmation that NFC reader is off and nfcTask is in Idle Mode 2
                for (int i = 0; i < 3; ++i) { // Attempt confirmation up to 3 times
                    uxBits = xEventGroupWaitBits(appEventGroup, (1 << 4), pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
                    if ((uxBits & (1 << 4)) != 0) {
                        //logger::write("[payment] Confirmed NFC reader is off.", "debug");
                        break;
                    } else {
                        //logger::write("[payment] No confirmation from NFC reader, retrying...", "debug");
                        vTaskDelay(pdMS_TO_TICKS(500)); // Short delay before retry
                    }
                }

                if ((uxBits & (1 << 4)) == 0) {
                    //logger::write("[payment] Failed to confirm NFC shutdown, proceeding with caution.", "debug");
                    // Handle the scenario where the NFC reader doesn't confirm shutdown
                    // This could involve logging an error, alerting the user, or taking other appropriate actions
                }
            }
        }
    }

    // Check whether payment has been made
    if (paymentisMade) {
        screen::showSuccess();
        logger::write("[payment] Payment has been made.", "info");
    }

    nfcTaskState = isNFCTaskActive ? eTaskGetState(nfcTaskHandle) : eDeleted;
    if (isNFCTaskActive && nfcTaskState != eDeleted) {
        // Check if the NFC reader is off before exiting
        while ((uxBits & (1 << 0)) != 0) {
            //logger::write("[payment] NFC reader is not off, sending shutdown signal...", "debug");
            xEventGroupClearBits(nfcEventGroup, (1 << 0) | (1 << 1)); // Ensure power up bit and idle bit 1 are not set
            //logger::write("[payment] Cleared power up bit and idle bit 1 on nfcEventGroup: " + std::to_string((1 << 0) | (1 << 1)), "debug");
            xEventGroupSetBits(nfcEventGroup, (1 << 2)); // New signal code for shutting down the NFC reader
            //logger::write("[payment] Set shutdown NFC reader bit on nfcEventGroup: " + std::to_string((1 << 2)), "debug");
            vTaskDelay(pdMS_TO_TICKS(210));
            uxBits = xEventGroupWaitBits(appEventGroup, uxAllBits, pdFALSE, pdFALSE, pdMS_TO_TICKS(420));
            //logger::write("[payment] Received bits from appEventGroup: " + std::to_string(uxBits), "debug");
            if ((uxBits & (1 << 4)) != 0) {
                //logger::write("[payment] NFC reader is off.", "debug");
                vTaskSuspend(nfcTaskHandle); // Suspend the NFC task
            } else {
                //logger::write("[payment] NFC reader is not off, waiting...", "debug");
                vTaskDelay(pdMS_TO_TICKS(210));  // Give the NFC reader more time to turn off
            }
        }
    }
    logger::write("[payment] Returning to App Loop", "debug");
    return paymentisMade;
}
