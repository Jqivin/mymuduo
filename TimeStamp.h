#pragma once

#include <iostream>
#include <string>
class TimeStamp
{
public:
    TimeStamp();

    ~TimeStamp();

    explicit TimeStamp(int64_t microSecondsSinceEpochArg);

    static TimeStamp now();

    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};