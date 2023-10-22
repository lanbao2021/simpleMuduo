#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

Timestamp Timestamp::now()
{
    return Timestamp(time(NULL)); // time(NULL)返回当前时间的秒数
}

std::string Timestamp::toString() const
{
    char buf[128] = {0}; // 用来存储时间的字符串
    tm *tm_time = localtime(&microSecondsSinceEpoch_); // 将时间戳转换为tm结构体
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d", // 将tm结构体转换为字符串
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }