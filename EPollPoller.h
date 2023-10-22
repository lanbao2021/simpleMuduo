#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/**
 * epoll的使用
 * epoll_create <---> EPollPoller
 * epoll_ctl   add/mod/del <---> updateChannel/removeChannel...
 * epoll_wait <---> poll
 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override; // 说明基类的析构函数是虚函数

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override; // 等待事件发生

    void updateChannel(Channel *channel) override; // 更新通道，添加或者移除
    void removeChannel(Channel *channel) override; // 移除通道

private:
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const; // 填写活跃的连接

    void update(int operation, Channel *channel); // 更新channel通道

    static const int kInitEventListSize = 16; // 初始化epoll_event数组的大小

    using EventList = std::vector<epoll_event>; // 定义EventList类型

    EventList events_; // 保存发生的事件

    int epollfd_; // epoll的文件描述符
};