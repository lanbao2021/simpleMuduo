#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch); // explicit防止隐式转换
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_; // 时间戳
};