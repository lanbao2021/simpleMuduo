#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL")) // 如果环境变量中设置了MUDUO_USE_POLL，就使用poll
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}