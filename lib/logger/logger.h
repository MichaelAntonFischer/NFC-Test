#ifndef LOGGER_H
#define LOGGER_H

#include "config.h"
#include "util.h"
#include "opago_spiffs.h"
#include <Arduino.h>
#include <iostream>
#include <map>
#include <string>

namespace logger {
	void init();
	void loop();
	void write(const std::string &msg, const char* logLevel);
	void write(const std::string &msg);
	void write(const char* msg);
	void write(const char* msg, const char* logLevel);
	std::string getLogFilePath(const uint8_t &num = 0);
}

#endif
