#pragma once

#include <string>
#include <map>
#include <chrono>

class DebugTimer
{
    struct Info
    {
        size_t events;
        double totalTime;
    };

    std::map<std::string, Info> m_events;

    DebugTimer() = default;
    ~DebugTimer();

public:

    void addEvent(const char* name, double time)
    {
#define TIME_PROFILE
#ifdef TIME_PROFILE
        m_events[name].events++;
        m_events[name].totalTime += time;
#endif // TIME_PROFILE
    }

    static DebugTimer& instance()
    {
        static DebugTimer timer;
        return timer;
    }

    class AutoLog
    {
        using Time = decltype(std::chrono::high_resolution_clock::now());

        const char* m_name;
        Time        m_start;

    public:
        explicit AutoLog(const char* name)
            : m_name(name)
            , m_start(std::chrono::high_resolution_clock::now())
        {
        }

        ~AutoLog()
        {
            auto finish = std::chrono::high_resolution_clock::now();
            double lastTime = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(finish - m_start).count();

            DebugTimer::instance().addEvent(m_name, lastTime);
        }
        
    };
};

