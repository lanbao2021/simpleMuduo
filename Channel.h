#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * 理清楚  EventLoop、Channel、Poller之间的关系   《= Reactor模型上对应 Demultiplex
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件
 */
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;              // 回调函数对象
    using ReadEventCallback = std::function<void(Timestamp)>; // 读事件回调函数对象

    Channel(EventLoop *loop, int fd); // 构造函数，初始化channel
    ~Channel();                       // 析构函数，关闭fd

    void handleEvent(Timestamp receiveTime); // fd得到poller通知以后，处理事件的

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void tie(const std::shared_ptr<void> &); // 设置channel绑定的TcpConnection对象

    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; } // used by pollers

    void enableReading() // 用于开启fd的读事件
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading() // 用于关闭fd的读事件
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting() // 用于开启fd的写事件
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting() // 用于关闭fd的写事件
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll() // 用于关闭fd所有的事件
    {
        events_ = kNoneEvent;
        update();
    }

    bool isNoneEvent() const { return events_ == kNoneEvent; } // 用于判断fd当前是否有事件发生
    bool isWriting() const { return events_ & kWriteEvent; }   // 用于判断fd是否注册了写事件
    bool isReading() const { return events_ & kReadEvent; }    // 用于判断fd是否注册了读事件

    int index() { return index_; }            // 获取channel在ChannelList中的索引位置
    void set_index(int idx) { index_ = idx; } // 设置channel在ChannelList中的索引位置
    EventLoop *ownerLoop() { return loop_; }  // 用于获取channel所属的EventLoop
    void remove();                            // 从EventLoop中移除channel

private:
    void update(); // 更新channel所属的EventLoop监听的fd感兴趣的事件

    void handleEventWithGuard(Timestamp receiveTime); // 事件处理函数，由handleEvent()调用

    static const int kNoneEvent;  // 无事件
    static const int kReadEvent;  // 读事件
    static const int kWriteEvent; // 写事件

    EventLoop *loop_; // 指向所属的EventLoop，事件循环
    const int fd_;    // fd, Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件

    int index_; // 表示当前channel在poller中的状态，未添加、已添加、已删除（对应EPollPoller中的kNew、kAdded、kDeleted）

    std::weak_ptr<void> tie_; // 保存TcpConnection对象的弱引用，防止TcpConnection对象被手动remove掉，channel还在执行回调操作
    bool tied_;               // 是否绑定了TcpConnection对象

    // 因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_; // 读事件回调操作
    EventCallback writeCallback_;    // 写事件回调操作
    EventCallback closeCallback_;    // 关闭事件回调操作
    EventCallback errorCallback_;    // 错误事件回调操作
};
