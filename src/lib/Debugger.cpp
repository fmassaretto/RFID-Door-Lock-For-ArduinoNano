#include "Debugger.h"

void Debugger::_enableDebugger(bool enable)
{
    debugger_enable = enable;
}

bool Debugger::_isDebuggerEnable()
{
    return debugger_enable;
}