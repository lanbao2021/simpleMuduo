#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

const int kNew = -1;    // channel未添加到poller中，channel的成员index_ = -1
const int kAdded = 1;   // channel已添加到poller中
const int kDeleted = 2; // channel从poller中删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      // epoll_create1可以指定标志位，这里指定了EPOLL_CLOEXEC，表示在调用exec时关闭父进程使用的那些文件描述符
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_); // 关闭epoll文件描述符
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理，因为这个函数会被频繁调用影响性能
    // 但是为了方便调试，这里用LOG_INFO
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    // epoll_wait的第二个参数是epoll_event数组，这里用vector来模拟
    // 好处是可以动态扩容
    // 坏处是每次都要拷贝一次？？？（Copilot的提示，我暂时不确定这个说法是否正确2023-10-22）
    // &*events_.begin()是vector的第一个元素的地址
    // static_cast<int>(events_.size())是vector的大小，因为epoll_wait的第三个参数是int类型，所以要强转一下
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;           // 保存errno，防止被其他函数修改（实现线程安全），因为errno是全局变量
    Timestamp now(Timestamp::now()); // 获取当前时间

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels); // 填写活跃的连接

        if (numEvents == events_.size()) // 如果发生的事件数等于数组的大小，说明数组不够用了，需要扩容
        {
            events_.resize(events_.size() * 2); // 扩容
        }
    }
    else if (numEvents == 0) // 说明超时时间内没有事件发生
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__); // 调试信息
    }
    else
    {
        if (saveErrno != EINTR) // 如果不是被信号中断的，就报错
        {
            errno = saveErrno; // 恢复errno

            LOG_ERROR("EPollPoller::poll() err!"); // 错误信息
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 *                          EventLoop
 *      activate_channels_               Poller
 *                                  ChannelMap<fd, Channel*>
 *
 *     activate_channels_仅存放部分活跃的channel对象
 *     ChannelMap则是管理当前loop所有的channel对象
 */
// 更新channel通道的状态
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); // 获取channel的状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) // channel未添加到poller上或者已经从poller上删除了
    {
        if (index == kNew) // channel未添加到poller上
        {
            int fd = channel->fd();  // 获取channel的fd
            channels_[fd] = channel; // 将channel添加到channels_中
        }

        channel->set_index(kAdded);     // 设置channel的index_状态为已添加
        update(EPOLL_CTL_ADD, channel); // 将channel添加到poller上
    }
    else // channel已经在poller上注册过了，那么就是修改或者删除了
    {
        int fd = channel->fd();     // 获取channel的fd
        if (channel->isNoneEvent()) // 对任何事件都不感兴趣了，就删除它
        {
            update(EPOLL_CTL_DEL, channel); // 将channel从poller上删除
            channel->set_index(kDeleted);   // 设置channel的index_状态为已删除
        }
        else
        {
            update(EPOLL_CTL_MOD, channel); // 更新channel上的事件
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd(); // 获取channel的fd
    channels_.erase(fd);    // 从channels_中删除channel
    // 注意：这里只是删除了EPoller监听的map中的元素，并没有删除channel，他还在EventLoop的ChannelList中

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index(); // 获取channel的状态

    if (index == kAdded) // channel已经添加到poller上
    {
        update(EPOLL_CTL_DEL, channel); // 将channel从poller上删除
    }

    channel->set_index(kNew); // 设置channel的index_状态为未添加，相当于放回EventLoop管理的ChannelList中
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        // epoll_event.data.ptr指向的是channel
        // 为啥要强转呢？因为ptr是void*类型的，所以要强转成Channel*
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);

        // 设置channel上的事件
        channel->set_revents(events_[i].events);

        // EventLoop就可以拿到了它的poller给它返回的所有发生事件的channel列表了
        activeChannels->push_back(channel);
    }
}

// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    // event.data.fd = fd;       // fd和ptr是共用的，同时只能用一个，这里用fd
    event.data.ptr = channel; // fd和ptr是共用的，同时只能用一个，这里用ptr

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL) // 允许删除出问题
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else // 其余的add/mod出问题就直接FATAL终止
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}