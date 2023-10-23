# simpleMuduo

A simplified Muduo network lib using C++ 11 features.

* [X] 用C++ 11重写陈硕的muduo网络库，去掉对Boost库的依赖
* [X] EchoServer实现、运行
* [ ] HTTP Server实现、运行

# simpleMuduo网络库的运行原理

* [ ] 如何理解Reactor网络模型？
* [ ] 各个模块都是干嘛的？
* [ ] 一个客户连接来了后会发生什么？
* [ ] 为啥muduo网络库不使用ET模式？出于什么考量？😧

# 模块简要介绍

按如下顺序去梳理的话会比较合适

* [X] noncopyable
* [X] Logger
* [X] Timestamp
* [X] InetAddress
* [X] Channel
* [X] Poller
* [X] EpollPoller
* [ ] 获取线程tid
* [ ] EventLoop
* [ ] Thread
* [ ] EventLoopThread
* [ ] Socket
* [ ] Acceptor
* [ ] TcpServer

## noncopyable

noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作

这是通过 `= delete` 实现的

有点像单例模式？感觉就是啊

## Logger

muduo网络库里的日志系统并不是很优秀

采用的是同步的日志系统，可能会存在性能瓶颈

当然，合理使用调试和日志输出即可，影响不会太大

定义了几个宏来写不同级别的日志：

* LOG_INFO
* LOG_ERROR
* LOG_FATAL
* LOG_DEBUG

## InetAddress

封装了IPv4 socket地址，这个暂时没有太多值得说道的

## Channel

看到Channel就要开始大致理顺一下这个网络库的抽象层次结构了

它大概是这样的：

* TCPServer
  * EventLoop
    * Poller抽象类(用于封装select, poll, epoll)
      * PollPoller
      * EPollPoller(目前只实现了这个)
      * ChannelMap(管理当前Loop关心的那些Channel对象)
        * `Channelmap<fd, Channel*>`
        * ChannelMap记录着sockfd及其对应的Channel对象
    * ChannelList activechannel_
      * 保存活跃的channels，交给事件处理程序
      * 每次处理完毕activechannel_都要他都要clear一下
  * EventLoop
  * ...
  * EventLoop
  * 即One Thread Per Loop

可以看到，Channel类是核心的抽象数据结构，所以应该先搞懂它的内部设计再看外部使用它的那些类

## Poller抽象类

它是抽象类，用来规范派生类EPollPoller, PollPoller的行为

⚠️注意：目前我只实现了EPollPoller

## EPollPoller

封装系统调用epoll_create成EPollPoller方法

封装系统调用epoll_ctl进行add/mod/del，对应updateChannel/removeChannel...方法

封装epoll_wait成poll方法

通过以上封装就很好理解EPollPoller干的事情了，监听sockfd上的事件然后更新相应的数据结构，交给时间处理函数EventHandler

## 获取线程tid

如何获取当前线程的tid?

请注意：是获取“线程tid"不是进程“pid"

`ps -ef | grep mysqld`获取的是pid

## EventLoop





### baseloop和subloop之间的通信机制

EventLoop里面的runInLoop、queueInLoop方法一开始把我搞晕了

在runInLoop里面有这么一段逻辑

```c++
    if (isInLoopThread()) // 在当前的loop线程中，执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb , 就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
```

那什么时候会出现queueInLoop(cb)的情况呢

TCPServer对象里管理了多个loop对象，每次轮询的选一个，每个loop对象都属于不同的线程，假设TCPServer对应的那一个叫baseloop，那么被它选出来干活的就叫subloop

具体可以看一下TcpServer::newConnection方法

* 首先它轮询选择一个subloop对象用来干活
* 此时你要意识到，能执行newConnection方法的是baseloop
* 也就是说我们会在baseloop里面执行subloop的runInLoop方法
* 理解到这一步就算正确理解了runInLoop和queueInLoop的设计用意

那么接下来问题又来了，queueInLoop后baseloop如何通知subloop干活呢，因为光给subloop添加待执行的回调函数并没有办法让它执行！

这就是EventLoop::queueInLoop方法里的wakeup()方法的作用了，他可以帮baseloop唤醒subloop里面正阻塞在：⬇️

`poller_->poll(kPollTimeMs, &activeChannels_) `的 `EventLoop::loop`方法

(注：不喊他起床wakeup也行，但是要等 `kPollTimeMs`直到它睡醒)

每个eventloop都会有一个eventfd文件描述符，每个eventloop也会监听这个eventfd文件描述符，因此wakeup就是通过往eventfd里随便写点东西，然后让阻塞的Poller检测到有可读事件发生，然后就立即返回了

* 返回后先经过for (Channel *channel : activeChannels_)
  * 就是将eventfd的活跃状态去除，把缓冲区的东西读出来即可
* 然后再执行doPendingFunctors()，就完成了回调任务的执行

# 多Reactor 多线程

这里有一个问题后续再明确一下，就是SubReactor是否会将业务处理交给WorkThread

![img](image/README/1694440902198.png)

这个[项目](https://github.com/Shangyizhou/A-Tiny-Network-Library)里的示意图是有画的是有工作线程池的

![img](https://camo.githubusercontent.com/43f02acdbd589ba7df40fdd0a7890a47314fc8d08e831f5f22c6f0414d0acd4d/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313637303835333133343532382d63383864323766322d313061322d343664332d623330382d3438663736333261326630392e706e673f782d6f73732d70726f636573733d696d616765253246726573697a65253243775f3933372532436c696d69745f30)

下面这个图存一个档，我觉得画的不太贴合muduo网络库，但是抽象层面似乎挺有逻辑，看看能不能改进一下，结合muduo网络库具体的类再重制一下这个结构图

![1698043411705](image/README/1698043411705.png)
