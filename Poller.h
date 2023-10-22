#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
// 事件分发器 = IO复用(Reactor) + 事件处理器(Handler)
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>; // 定义ChannelList类型

    Poller(EventLoop *loop);     // 构造函数
    virtual ~Poller() = default; // 虚析构函数

    // 给所有IO复用保留统一的接口
    // IO复用的核心操作，等待事件的发生（分发IO事件）
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0; // 纯虚函数，等待事件发生

    virtual void updateChannel(Channel *channel) = 0; // 更新通道，添加或者移除
    virtual void removeChannel(Channel *channel) = 0; // 移除通道

    bool hasChannel(Channel *channel) const; // 判断参数channel是否在当前Poller当中

    static Poller *newDefaultPoller(EventLoop *loop); // EventLoop可以通过该接口获取默认的IO复用的具体实现

protected:
    // map的key：sockfd  value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>; // 定义ChannelMap类型
    ChannelMap channels_;                                  // Poller所管理的所有的channel，fd -> channel的映射

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};