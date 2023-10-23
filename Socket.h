#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装sockfd + 修改sockfd状态的方法
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on); // 无论数据包大小，都会立即发送
    void setReuseAddr(bool on); // 
    void setReusePort(bool on); // TIME_WAIT状态下的重用
    void setKeepAlive(bool on); // TCP心跳
private:
    const int sockfd_; // 文件描述符
};