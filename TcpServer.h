#pragma once

/**
 * 用户使用muduo编写服务器程序
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,               // 传给TcpServer的EventLoop是baseloop，同时对应acceptor模块
              const InetAddress &listenAddr, // 必要的Socket Address
              const std::string &nameArg,    // 服务名称
              Option option = kNoReusePort); // 是否要设置端口复用（这个有点忘了具体是干啥的，后续查一下，目前不知道该怎么查比较好）
    ~TcpServer();

    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setThreadNum(int numThreads); // 设置底层subloop的个数
    void start();                      // 开启服务器监听进程

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_;                                 // 传给TcpServer的EventLoop是baseloop
    const std::string ipPort_;                        // IP地址和端口号（服务器端）
    const std::string name_;                          // 我们给当前TcpServer服务起的名字
    std::unique_ptr<Acceptor> acceptor_;              // 运行在baseloop，任务就是监听新连接事件，用智能指针管理是因为acceptor_是堆上空间
    std::shared_ptr<EventLoopThreadPool> threadPool_; // TcpServer会创建一个线程池，以轮询的方式给它们安排新连接，用智能指针管理threadPool_，因为是堆上空间

    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    ThreadInitCallback threadInitCallback_;       // loop线程初始化的回调（好像没用上，如果我没理解错的话）

    std::atomic_int started_; // TcpServer服务是否启动？注意这是atomic_int

    int nextConnId_;                                                         // 我们会给每个连接进行编号，baseloop占了编号0，所以接下去的新连接会从1开始
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>; // 每一个TcpConnection也有名字，并且我们用一个无序map保存它们
    ConnectionMap connections_;                                              // 保存所有的连接
};