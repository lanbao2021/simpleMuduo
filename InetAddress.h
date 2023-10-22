#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装 Socket address
class InetAddress
{
public:
    // explicit 防止隐式转换
    // 构造函数的两种形式
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr) : addr_(addr) {}

    std::string toIp() const;     // 返回字符串类型的 IP 地址
    std::string toIpPort() const; // 返回字符串类型的 IP 地址和端口号
    uint16_t toPort() const;      // 返回端口号

    // 获取 socket address
    const sockaddr_in *getSockAddr() const { return &addr_; }

    // 设置 socket address
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_; // IPv4 socket address，这边只支持 IPv4
};