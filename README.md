# simpleMuduo

A simplified Muduo network lib using C++ 11 features.

* [X] 用C++ 11重写陈硕的muduo网络库，去掉对Boost库的依赖
* [X] EchoServer实现、运行
* [ ] HTTP Server实现、运行

# simpleMuduo网络库的运行原理

* [ ] 如何理解Reactor网络模型？
* [ ] 各个模块都是干嘛的？
* [ ] 一个客户连接来了后会发生什么？

# 模块解析

* [X] noncopyable
* [X] Logger
* [X] Timestamp
* [ ] InetAddress
* [ ] Channel
* [ ] Poller
* [ ] EpollPoller
* [ ] EventLoop
* [ ] Thread
* [ ] EventLoopThread
* [ ] Socket
* [ ] Acceptor
* [ ] TcpServer

## noncopyable

noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作，通过 `= delete` 实现，有点像单例模式？感觉就是啊

## Logger

定义了几个宏来写不同级别的日志：

* LOG_INFO
* LOG_ERROR
* LOG_FATAL
* LOG_DEBUG

只要看懂下面这个代码就能读懂

```C++
// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)
```

## InetAddress

封装socket地址类型

# 多Reactor 多线程

这里有一个问题后续再明确一下，就是SubReactor是否会将业务处理交给WorkThread

![img](image/README/1694440902198.png)

这个[项目](https://github.com/Shangyizhou/A-Tiny-Network-Library)里的示意图是有画的是有工作线程池的

![img](https://camo.githubusercontent.com/43f02acdbd589ba7df40fdd0a7890a47314fc8d08e831f5f22c6f0414d0acd4d/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313637303835333133343532382d63383864323766322d313061322d343664332d623330382d3438663736333261326630392e706e673f782d6f73732d70726f636573733d696d616765253246726573697a65253243775f3933372532436c696d69745f30)
