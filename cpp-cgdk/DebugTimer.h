#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>

class DebugTimer
{
    struct Info
    {
        std::vector<double> totalTime;

        Info()
            : totalTime()
        {
            totalTime.reserve(4000);
        }
    };

    std::map<std::string, Info> m_events;

    DebugTimer() = default;
    ~DebugTimer();

public:

#define TIME_PROFILE
    void addEvent(const std::string& name, double time)
    {
#ifdef TIME_PROFILE
        m_events[name].totalTime.push_back(time);
#endif // TIME_PROFILE
    }

    void printEvent(const std::pair<std::string, Info>& eventInfo);

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
            auto startThis = std::chrono::high_resolution_clock::now();

            auto finish = std::chrono::high_resolution_clock::now();
            double lastTime = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(finish - m_start).count();

            DebugTimer::instance().addEvent(m_name, lastTime);

            auto finishThis = std::chrono::high_resolution_clock::now();
            double logTime = 2 * std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(finishThis - startThis).count();

            static const std::string autolog = "DebugTimer::AutoLog";
            DebugTimer::instance().addEvent(autolog, logTime);
        }
        
    };
};

