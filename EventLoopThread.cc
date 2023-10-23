#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),                                               // 创建Thread时没有立即绑定EventLoop
      exiting_(false),                                              // exiting表示退出线程
      thread_(std::bind(&EventLoopThread::threadFunc, this), name), // 初始化Thread类对象，传入回调函数
      mutex_(),                                                     // 信号量初始化
      cond_(),                                                      // 条件变量初始化
      callback_(cb)                                                 // 线程初始化的回调（暂时没有用上）
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();  // 关闭EventLoop
        thread_.join(); // 阻塞等待thread_结束
    }
}

/**
 * @brief 真正开始创建新线程 -> 创建EventLoop -> 开启EventLoop事件循环
 *
 * @return EventLoop*
 */
EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层的新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) // 什么时候loop_会不为空呢？threadFunc执行起来后，即线程创建起来了
        {
            cond_.wait(lock);
        }
        loop = loop_; // loop_是在threadFunc里创建出来的
    }
    return loop;
}

/**
 * @brief 传递给Thread对象的函数，会在新线程创建后开始执行，在里面会创建EventLoop对象，并开启事件循环
 */
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建EventLoop对象，与本EventLoopThread的Thread对象相对应的

    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop loop  => Poller.poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}