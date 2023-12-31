#include "app.h"

std::string pin = "";
std::string qrcodeDatafallback = "";
std::string keysBuffer = "";
const std::string keyBufferCharList = "0123456789";
double amount = 0;
bool correctPin = false;
std::string pinBuffer = "";
int incorrectPinAttempts = 0;

const std::string keyPressed;
unsigned int sleepModeDelay;
unsigned long lastActivityTime = 0;
bool isFakeSleeping = false;

void appendToKeyBuffer(const std::string &key) {
	if (keyBufferCharList.find(key) != std::string::npos) {
		keysBuffer += key;
	}
}

std::string leftTrimZeros(const std::string &keys) {
	return std::string(keys).erase(0, std::min(keys.find_first_not_of('0'), keys.size() - 1));
}

double keysToAmount(const std::string &t_keys) {
	if (t_keys == "") {
		return 0;
	}
	const std::string trimmed = leftTrimZeros(t_keys);
	double amount = std::stod(trimmed.c_str());
	if (amountCentsDivisor > 1) {
		amount = amount / amountCentsDivisor;
	}
	return amount;
}

void handleSleepMode() {
	if (sleepModeDelay > 0) {
		if (millis() - lastActivityTime > sleepModeDelay) {
			if (power::isUSBPowered()) {
				if (!isFakeSleeping) {
					// The battery does not charge while in deep sleep mode.
					// So let's just turn off the screen instead.
					screen::sleep();
					isFakeSleeping = true;
				}
			} else {
				cache::init();
				cache::save("pin", pin);
				cache::save("keysBuffer", keysBuffer);
				cache::save("qrcodeData", qrcodeData);
				cache::save("lastScreen", screen::getCurrentScreen());
				cache::end();
				power::sleep();
			}
		} else if (isFakeSleeping) {
			screen::wakeup();
			const std::string lastScreen = screen::getCurrentScreen();
			if (lastScreen == "home") {
				screen::showHomeScreen();
			} else if (lastScreen == "enterAmount") {
				screen::showEnterAmountScreen(keysToAmount(keysBuffer));
			} else if (lastScreen == "paymentQRCode") {
				screen::showPaymentQRCodeScreen(qrcodeData);
			} else if (lastScreen == "paymentPin") {
				screen::showPaymentPinScreen(pin);
			}
			isFakeSleeping = false;
		}
	}
}

void appTask(void* pvParameters) {
    Serial.println("App task started");
    screen::showHomeScreen();
    int signal;
    while(1) {
        power::loop();
        handleSleepMode();
        const std::string currentScreen = screen::getCurrentScreen();
        logger::write("Current Screen: " + currentScreen);
        if (currentScreen == "") {
            screen::showHomeScreen();
            keysBuffer = "";
            screen::showEnterAmountScreen(keysToAmount(keysBuffer));
        }
        const std::string keyPressed = keypad::getPressedKey();
        if (keyPressed != "") {
            logger::write("Key pressed: " + keyPressed);
            lastActivityTime = millis();
        }
        if (currentScreen == "home") {
            Serial.println("Home Screen");
            if (keyPressed == "") {
                keysBuffer = "";
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            } else if (keyPressed == "0") {
                keysBuffer = "";
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            } else if (keyPressed == "*") {
                keysBuffer = "";
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            } else if (keyPressed == "#") {
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            } else {
                if (keyPressed != "0" || keysBuffer != "") {
                    appendToKeyBuffer(keyPressed);
                }
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            }
        } else if (currentScreen == "enterAmount") {
            if (keyPressed == "") {
                // Do nothing.
            } else if (keyPressed == "*") {
                keysBuffer = "";
                screen::showEnterAmountScreen(keysToAmount(keysBuffer));
            } else if (keyPressed == "#") {
                amount = keysToAmount(keysBuffer);
                if (amount > 0) {
                    qrcodeData = "";
                    qrcodeDatafallback = "";
                    pin = util::generateRandomPin();
                    if (config::getString("fiatCurrency") == "sat") { //sat currency in LNbits for some reason returns bits. This needs to be removed once bug is fixed. 
                        Serial.println("Dividing");
                        amount = amount / 100;
                    } 
                    Serial.println("Amount: " + String(amount));
                    Serial.println("New Amount: " + String(amount));
                    std::string signedUrl = util::createLnurlPay(amount, pin);
                    std::string encoded = util::lnurlEncode(signedUrl);
                    qrcodeData += config::getString("uriSchemaPrefix");
                    qrcodeData += util::toUpperCase(encoded);
                    qrcodeDatafallback = qrcodeData;
                    if (WiFi.status() == WL_CONNECTED) {
                        //if WiFi is connected, we can fetch the invoice from the server
                        screen::showSand();
                        onlineStatus = true;
                        std::string paymentHash = "";
                        bool paymentMade = false;
                        qrcodeData = requestInvoice(signedUrl);
                        if (qrcodeData.empty()) {
                            keysBuffer = "";
                            logger::write("Server connection failed. Falling back to offline mode.");
                            onlineStatus = false;
                            screen::showPaymentQRCodeScreen(qrcodeDatafallback);
                            logger::write("Payment request shown: \n" + signedUrl);
                            logger::write("QR Code data: \n" + qrcodeDatafallback);
                        } else {
                            keysBuffer = "";
                            screen::showPaymentQRCodeScreen(qrcodeData);
                            logger::write("Payment request shown: \n" + qrcodeData);
                            paymentHash = fetchPaymentHash(qrcodeData);
                            logger::write("Payment hash: " + paymentHash);
                            paymentMade = waitForPaymentOrCancel(paymentHash, config::getString("apiKey.key"), qrcodeData);
                            if (!paymentMade) { 
                                screen::showX();
                                vTaskDelay(pdMS_TO_TICKS(2100));
                                keysBuffer = "";
                                screen::showHomeScreen();
                            } else {
                                screen::showSuccess();
                                vTaskDelay(pdMS_TO_TICKS(2100));
                                keysBuffer = "";
                                screen::showHomeScreen();
                            }
                        }
                    } else {
                        logger::write("Device is offline, displaying payment QR code...");
                        screen::showPaymentQRCodeScreen(qrcodeData);
                        logger::write("Payment request shown: \n" + signedUrl);
                        logger::write("QR Code data: \n" + qrcodeData);
                    }
                }
            } else if (keysBuffer.size() < maxNumKeysPressed) {
                if (keyPressed != "0" || keysBuffer != "") {
                    appendToKeyBuffer(keyPressed);
                    logger::write("keysBuffer = " + keysBuffer);
                    screen::showEnterAmountScreen(keysToAmount(keysBuffer));
                }
            }
        } else if (currentScreen == "paymentQRCode") {
            if (keyPressed == "#") {
                pinBuffer = "";
                screen::showPaymentPinScreen(pinBuffer);
            } else if (getLongTouch('*')) { //long press to cancel payment
                screen::showX();
                vTaskDelay(pdMS_TO_TICKS(2100));
                screen::showHomeScreen();
            } else if (keyPressed == "1") {
                screen::adjustContrast(-10);// decrease contrast
            } else if (keyPressed == "4") {
                screen::adjustContrast(10);// increase contrast
            }
        } else if (currentScreen == "paymentPin") {
            if (keyPressed == "#" || keyPressed == "*") {
                if (!qrcodeDatafallback.empty()) {
                    screen::showPaymentQRCodeScreen(qrcodeDatafallback);
                } else {
                    screen::showPaymentQRCodeScreen(qrcodeData);
                }
            } else if (keyPressed == "0" || keyPressed == "1" || keyPressed == "2" || keyPressed == "3" || keyPressed == "4" || keyPressed == "5" || keyPressed == "6" || keyPressed == "7" || keyPressed == "8" || keyPressed == "9") {
                pinBuffer += keyPressed;
                if (pinBuffer.length() == 4) {
                    if (pinBuffer == pin) {
                        screen::showSuccess();
                        pinBuffer = "";
                        vTaskDelay(pdMS_TO_TICKS(2100));
                        screen::showHomeScreen();
                    } else {
                        screen::showX();
                        pinBuffer = "";
                        vTaskDelay(pdMS_TO_TICKS(2100));
                        if (++incorrectPinAttempts >= 5) {
                            pinBuffer = "";
                            incorrectPinAttempts = 0;
                            vTaskDelay(pdMS_TO_TICKS(2100));
                            screen::showHomeScreen();
                        }
                    }
                }
                else {
                    screen::showPaymentPinScreen(pinBuffer);
                }
            }
        }
        if (power::isUSBPowered()) {
            screen::hideBatteryPercent();
        } else {
            screen::showBatteryPercent(power::getBatteryPercent());
        }
        //UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        //Serial.print("High water mark App Main Loop: ");
        //Serial.println(uxHighWaterMark);
        taskYIELD();
    } 
} 