#pragma once
#ifndef Debugger_h
#define Debugger_h

#include <Arduino.h>

class Debugger
{
protected:
  bool debugger_enable;

public:
  explicit Debugger(bool enable);
  bool _isDebuggerEnable();
  void init();

  void logToSerial(int msg);
  void logToSerial(uint8_t msg);
  void logToSerial(const char* msg);
  void logToSerial(const __FlashStringHelper* msg);

  void logToSerialLn(int msg);
  void logToSerialLn(uint8_t msg);
  void logToSerialLn(const char* msg);
  void logToSerialLn(const __FlashStringHelper* msg);
};

#endif
