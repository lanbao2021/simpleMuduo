#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

// 获取日志唯一的实例对象
Logger &Logger::instance()
{
    static Logger logger; // 静态局部变量，线程安全
    return logger;        // 返回日志对象
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    logLevel_ = level; // 设置日志级别
}

// 2023-10-22突然想到，这里的case是不是应该不能break，因为如果是INFO级别的日志，那么ERROR和FATAL级别的日志也应该打印出来
// 写日志  [级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (logLevel_)
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

    // 打印时间和msg
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}