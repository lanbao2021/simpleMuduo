#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>


EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // 无须析构堆内存中的对象，因为用了智能指针管理
    // 其余的都是栈内存，自动释放
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    // 创建numThreads_个线程及其对应的EventLoop对象
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        
        // 创建EventLoopThread对象，里面会创建EventLoop对象和Thread对象
        EventLoopThread *t = new EventLoopThread(cb, buf); 
        
        // 将EventLoopThread对象放入容器中
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));

        // 将EventLoop对象放入容器中，在startLoop()才会真正创建线程和EventLoop
        loops_.push_back(t->startLoop()); 
    }

    // 整个服务端只有一个线程，运行着baseloop（这种情况必须传入有效的ThreadInitCallback对象）
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

// 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_; // 没有subloop时，返回baseLoop_

    if (!loops_.empty()) // 通过轮询获取下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        loops_;
    }
}