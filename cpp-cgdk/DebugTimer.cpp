#include "DebugTimer.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#ifdef _WIN32
#   include <windows.h>
#endif

DebugTimer::~DebugTimer()
{
    for(const auto& nameInfoPair : m_events)
        printEvent(nameInfoPair);

    std::cout << "Done\n";

#ifdef _WIN32
    if(IsDebuggerPresent())
        DebugBreak();
    else
        MessageBoxA(0, "Done", __FUNCTION__, 0);
#endif
}


void DebugTimer::printEvent(const std::pair<std::string, Info>& eventInfo)
{
    if(eventInfo.second.totalTime.empty())
        return;

    std::vector<double> times = eventInfo.second.totalTime;

    int count = times.size();
    std::sort(times.begin(), times.end());

    double sum    = std::accumulate(times.begin(), times.end(), 0.0);
    double avg    = sum / count;
    double median = times[count / 2];
    double fastest10pc = times[static_cast<size_t>(count * 0.1)];
    double fastest30pc = times[static_cast<size_t>(count * 0.3)];
    double slowest30pc = times[static_cast<size_t>(count * 0.7)];
    double slowest10pc = times[static_cast<size_t>(count * 0.9)];

    std::cout << eventInfo.first << ": total events=" << times.size() << "; total time: " << sum << "; avg: " << avg << "; median: " << median << std::endl
              << "    min: " << times.front() << "; 10% fastest: " << fastest10pc << "; 30% fastest: " << fastest30pc << std::endl
              << "    max: " << times.back() << "; slowest 30% " << slowest30pc << "; 10% slowest: " << slowest10pc << std::endl;
}

