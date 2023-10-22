#include "InetAddress.h"

#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);                   // 将 addr_ 清零
    addr_.sin_family = AF_INET;                    // IPv4
    addr_.sin_port = htons(port);                  // 端口号
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // IP 地址
}

std::string InetAddress::toIp() const
{
    // The :: before the function name indicates that it is a global function and not a member function of a class.
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 将网络字节序转换为字符串
    return buf;
}

std::string InetAddress::toIpPort() const
{
    // 字符串形式的ip:port
    // The :: before the function name indicates that it is a global function and not a member function of a class.
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 将网络字节序转换为字符串
    size_t end = strlen(buf);                               // 字符串的长度
    uint16_t port = ntohs(addr_.sin_port);                  // 端口号
    sprintf(buf + end, ":%u", port);                        // 将端口号加到字符串的末尾
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port); // 端口号
}

// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;

//     return 0;
// }