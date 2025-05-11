#pragma once
#ifndef Debugger_h
#define Debugger_h

#include <Arduino.h>

class Debugger
{
protected:
  bool debugger_enable;

public:
  Debugger(bool enable);
  bool _isDebuggerEnable();
  void init();
  void logToSerial(String msg);
};

#endif
