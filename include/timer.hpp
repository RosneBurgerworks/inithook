/*
 * timer.hpp
 *
 *  Created on: Jul 29, 2017
 *      Author: nullifiedcat
 */

#ifndef CH_TIMER_HPP
#define CH_TIMER_HPP
#include <chrono>

class Timer
{
public:
    typedef std::chrono::system_clock clock;

    inline Timer(){};

    inline bool check(unsigned ms) const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - last).count() >= ms;
    }
    inline bool test_and_set(unsigned ms)
    {
        if (check(ms))
        {
            update();
            return true;
        }
        return false;
    }
    inline void update()
    {
        last = clock::now();
    }

public:
    std::chrono::time_point<clock> last{};
};

#include <ctime>
class CTimer
{
public:
    inline void set(std::time_t sec)
    {
        t = std::time(nullptr) + sec;
    }
    inline bool check()
    {
        return t <= std::time(nullptr);
    }
    inline bool set_and_check(std::time_t sec)
    {
        if (t && !check())
            return false;

        set(sec);
        return true;
    }
private:
    std::time_t t;
};
#endif
