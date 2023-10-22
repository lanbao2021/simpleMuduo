#include "Poller.h"
#include "Channel.h"


Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

// static Poller *newDefaultPoller(EventLoop *loop); 为什么不在这里实现呢？
// 从语法上没啥问题，但这意味着要引用派生类的头文件，这是不太合理的
// 所以具体实现放在了一个单独的文件中：DefaultPoller.cc