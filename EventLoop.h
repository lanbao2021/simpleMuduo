#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

/**
 * @brief 事件循环类，封装了 Channel 和 Poller 模块，使他们两个之间能够相互沟通
 *
 * 这里的Poller就是指Epoll，因为Poll和Select我们没有去实现
 *
 */
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>; // 放在private里不行吗？我觉得行

    EventLoop();
    ~EventLoop();

    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; } // 获取Poller获取到事件的返回事件，没用上

    void runInLoop(Functor cb);   // 在当前loop中执行cb
    void queueInLoop(Functor cb); // 把cb放入队列中，唤醒loop所在的线程，执行cb

    void wakeup(); // 用来唤醒loop所在的线程的（mainReactor唤醒subReactor用的）

    void updateChannel(Channel *channel); // 更新当前EventLoop所管理Channel对象的状态
    void removeChannel(Channel *channel); // 移除某一个Channel对象
    bool hasChannel(Channel *channel);    // 判断是否有某一个Channel对象

    // 判断EventLoop对象是否在自己的线程里面，在的话就可以执行runInLoop，否则就是queueInLoop
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();        // wake up
    void doPendingFunctors(); // 执行回调

    // 标识，都是原子操作，通过CAS实现的（这个CAS是啥？）
    std::atomic_bool looping_;                // 标识是否开启循环
    std::atomic_bool quit_;                   // 标识是否退出loop循环（感觉跟loop_有重叠？）
    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否正在执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有的回调操作

    std::mutex mutex_; // 互斥锁，用来保护上面vector容器的线程安全操作，因为STL容器都不是线程安全的

    // 记录当前loop所在线程的id，用于后续执行回调操作的判断？
    const pid_t threadId_;

    Timestamp pollReturnTime_;       // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; // 为啥用智能指针呢？因为poller_指向堆内存，这样才能自动析构

    // mainReactor如何唤醒subReactor的？通过wakeupFd_，wakeupChannel_来实现
    // mainReactor如何选择subReactor的？通过轮询算法来选择
    // wakeupFd_是一个eventfd，用于唤醒subloop，wakeupChannel_是wakeupFd_对应的Channel对象
    // 这个eventfd是什么？是一个文件描述符，可以用来实现进程间的通信，可以再查一下相关资料
    // eventfd相比socketpair性能更好，这个后续再去了解一下
    // 为啥用智能指针呢？因为wakeupChannel_指向堆内存，这样才能自动析构
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_; // 把wakeupFd_也封装成一个Channel对象，存放在这

    using ChannelList = std::vector<Channel *>; // 定义ChannelList类型
    ChannelList activeChannels_;                // 记录当前EventLoop所管理Channel对象中有活跃事件发生的那些
};