#include <iostream>

#include "Logger.h"
#include "TimeStamp.h"

// 获取日志唯一的实例对象
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}
// 设置日志级别
void Logger::setLogLevel(int level)
{
    loglevel_ = level;
}
// 写日志 [级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (loglevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印当前时间和msg
    std::cout << TimeStamp::now().toString() << " : " << msg << std::endl;
}