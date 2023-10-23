#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

// 线程池类用于创建出多个线程及其对应的EventLoop对象
// 往上层抽象，它是被TcpServer类所使用的
class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; 

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }
private:

    EventLoop *baseLoop_; // 主线程的EventLoop会管理EventLoopThreadPool 
    std::string name_;
    bool started_;
    int numThreads_; // 线程池中的线程数量
    int next_; // 下一个subloop的索引，轮询的方式安排新连接给subloop

    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 存放线程池的容器
    std::vector<EventLoop*> loops_; // 存放EventLoop的容器，跟threads_一一对应
};