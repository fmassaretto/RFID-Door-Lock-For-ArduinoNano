#include "Debugger.h"

Debugger::Debugger(bool enable) {
    debugger_enable = enable;
}

void Debugger::init() {
    if(debugger_enable) {
        Serial.begin(9600);
        Serial.println("Serial init...");
    }
}

bool Debugger::_isDebuggerEnable() {
    return debugger_enable;
}

void Debugger::logToSerial(int msg) {
    if(debugger_enable) {
        Serial.print(msg);
    }
}

void Debugger::logToSerial(uint8_t msg) {
    if(debugger_enable) {
        Serial.print(msg);
    }
}

void Debugger::logToSerial(const char* msg) {
    if(debugger_enable) {
        Serial.print(msg);
    }
}

void Debugger::logToSerial(const __FlashStringHelper* msg) {
    if(debugger_enable) {
        Serial.print(msg);
    }
}



void Debugger::logToSerialLn(int msg) {
    if(debugger_enable) {
        Serial.println(msg);
    }
}

void Debugger::logToSerialLn(uint8_t msg) {
    if(debugger_enable) {
        Serial.println(msg);
    }
}

void Debugger::logToSerialLn(const char* msg) {
    if(debugger_enable) {
        Serial.println(msg);
    }
}

void Debugger::logToSerialLn(const __FlashStringHelper* msg) {
    if(debugger_enable) {
        Serial.println(msg);
    }
}