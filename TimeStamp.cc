#include "TimeStamp.h"

#include <time.h>

TimeStamp::TimeStamp()
    : microSecondsSinceEpoch_(0) {}

TimeStamp::~TimeStamp() {}

TimeStamp::TimeStamp(int64_t microSecondsSinceEpochArg)
    : microSecondsSinceEpoch_(microSecondsSinceEpochArg) {}

TimeStamp TimeStamp::now()
{
    // 获取当前的时间
    return TimeStamp(time(NULL));
}

std::string TimeStamp::toString() const
{
    char buf[128] = {0};
    tm *time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%04d/%02d/%02d %02d:%02d:%02d",
             time->tm_year + 1900,
             time->tm_mon + 1,
             time->tm_mday,
             time->tm_hour,
             time->tm_min,
             time->tm_sec);

    return buf;
}

/* Test Code
#include <iostream>
int main()
{
    std::cout << TimeStamp::now().toString() << std::endl;
    return 0;
}
*/