#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false) {}

Channel::~Channel() {}

// channel的tie方法什么时候调用过？一个TcpConnection新连接创建的时候 TcpConnection => Channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;   // 保存TcpConnection对象的弱引用
    tied_ = true; // 标记已经绑定了TcpConnection对象
}

/**
 * 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChannelList   Poller
 */
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中， 把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

/**
 * @brief 处理channel上的活跃事件
 *
 * 当和TcpConnection对象绑定了，说明是client的EventLoop，那么先检查和TcpConnection之间的绑定关系是否正常
 *
 * 如果tied_为true，那么进一步检测tie_.lock()是否能成功将weak_ptr -> shared_ptr，失败的话就没必要继续往下执行handleEventWithGuard咯
 *
 * 如果压根没有tied说明是acceptor对应的EventLoop，所以直接handleEventWithGuard（如果我没理解错的话）
 *
 * @param receiveTime
 */
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) // 如果channel绑定了TcpConnection对象
    {
        std::shared_ptr<void> guard = tie_.lock(); // 将weak_ptr转换为shared_ptr

        if (guard) // 如果转换成功
        {
            handleEventWithGuard(receiveTime); // 处理channel上的活跃事件
        }
    }
    else // 如果channel没有绑定TcpConnection对象
    {
        handleEventWithGuard(receiveTime); // 处理channel上的活跃事件
    }
}

// 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_); // 打印channel发生的具体事件

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) // 如果发生了EPOLLHUP事件，但是没有发生EPOLLIN事件
    {
        if (closeCallback_)
        {
            closeCallback_(); // 调用closeCallback_回调操作
        }
    }

    if (revents_ & EPOLLERR) // 如果发生了EPOLLERR事件
    {
        if (errorCallback_)
        {
            errorCallback_(); // 调用errorCallback_回调操作
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) // 如果发生了EPOLLIN或者EPOLLPRI事件
    {
        if (readCallback_)
        {
            readCallback_(receiveTime); // 调用readCallback_回调操作，传入Timestamp对象!
        }
    }

    if (revents_ & EPOLLOUT) // 如果发生了EPOLLOUT事件
    {
        if (writeCallback_)
        {
            writeCallback_(); // 调用writeCallback_回调操作 
        }
    }
}