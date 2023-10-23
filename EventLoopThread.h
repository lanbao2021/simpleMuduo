#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

// 这个类将会创建One loop per thread
// 继续往上层抽象还有EventLoopThreadPool类，它将会创建多个EventLoopThread对象
// 往下看的话，EventLoopThread里管理着一个EventLoop对象，一个Thread对象
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; 

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), 
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadFunc();

    EventLoop *loop_; // EventLoop对象
    bool exiting_;
    Thread thread_; // Thread对象
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};