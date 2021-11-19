/**
 *
 * Copyright [2021] <hpcers>
 * */

#ifndef TEST_TIME_H_
#define TEST_TIME_H_

#include <sys/time.h>

#define Tic()                        \
    do {                             \
        Time::Instance().GetSTime(); \
    } while (0);
#define Toc() Time::Instance().GetETime()

class Time
{
public:
    static Time& Instance()
    {
        static Time time;
        return time;
    }

    void GetSTime() { gettimeofday(&s, nullptr); }

    double GetETime()
    {
        gettimeofday(&e, nullptr);

        double timeMicoSec = (e.tv_sec - s.tv_sec) * 1.0e6 + (e.tv_usec - s.tv_usec);
        return timeMicoSec;
    }

private:
    Time()
        : s()
        , e()
    {
    }

private:
    struct timeval s;
    struct timeval e;
};   // class Time

#endif   // TEST_TIME_H_
