#ifndef Debugger_h
#define Debugger_h

class Debugger
{
protected:
  bool debugger_enable;

public:
  void _enableDebugger(bool enable);
  bool _isDebuggerEnable();
};

#endif
