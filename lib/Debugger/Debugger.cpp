#include "Debugger.h"

Debugger::Debugger(bool enable) {
    debugger_enable = enable;
}

void Debugger::init() {
    Serial.begin(9600);
    Serial.println("Serial init...");
}

bool Debugger::_isDebuggerEnable() {
    return debugger_enable;
}

void Debugger::logToSerial(String msg) {
    if(debugger_enable) {
        Serial.println(msg);
    }
    Serial.flush();
}