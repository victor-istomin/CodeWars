#include "DebugTimer.h"
#include <iostream>
#include <windows.h>

DebugTimer::~DebugTimer()
{
    for(const auto& nameInfoPair : m_events)
    {
        std::cout << nameInfoPair.first << ": total=" << nameInfoPair.second.events << "; time=" << nameInfoPair.second.totalTime
                  << "; avg=" << nameInfoPair.second.totalTime / nameInfoPair.second.events << std::endl;
    }

    ::MessageBox(0, "done", "done", 0);
    // std::cout << "Done\n";
}

