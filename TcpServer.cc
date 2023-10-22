#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),                                   // BaseLoop需要是非空
      ipPort_(listenAddr.toIpPort()),                                  // 保存IP和端口号为string字符串对象
      name_(nameArg),                                                  // 该TcpServer服务名称
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), // 新建Acceptor对象，但是构造函数中并没有启动listen
      threadPool_(new EventLoopThreadPool(loop, name_)),               // 创建线程池对象，但是构造函数中还没真的创建多个线程
      connectionCallback_(),                                           // ！这个为啥要在初始化列表出现？我觉得没有意义，并且没传入参数，不知道为啥还能正常运行
      messageCallback_(),                                              // ！这个为啥要在初始化列表出现？我觉得没有意义(2023-10-23，确实没意义，只是用来检测一下的其实，可以问GPT)
      nextConnId_(1),                                                  // 下一个连接的编号就要从1开始算了
      started_(0)                                                      // 建立TcpServer时还没启动，还需要后续调用start()
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        /**
         * 先用一个TcpConnectionPtr增加一下item.second（待销毁的TcpConnection对象）的引用计数
         * 
         * 然后item.second.reset()，这样待销毁的TcpConnection对象引用计数就剩下刚刚的conn智能指针了，它下一次「赋值」或「出了for循环」就会释放所指向的TcpConnection对象的堆内存了
         * 
         * 为什么要先暂存一下而不是直接销毁呢？因为还需要执行一下TcpConnection::connectDestroyed完成善后工作，这样才能正确安全的析构TcpConnection对象
         */
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        /**
         * 这条语句的执行路径是怎样的呢？首先 conn->getLoop() 可以获取conn对应的 EventLoop 的指针
         * 
         * 进入runInLoop的线程是baseloop，而conn对应的是clientloop，显然两者不是在一个线程中的，所以会执行queueInLoop(cb)
         * 
         * 注意执行的是this->queueInLoop哦，所以对应的EventLoop对象会在queueInLoop中执行this->wakeup后被唤醒，这样就达到了线程间通信的目的
         * 
         */
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

/**
 * @brief 设置线程池中线程的数量;
 * 
 * 单纯的设置一下clientloop数量，还是没有开始创建Thread;
 * 
 * clientloop 即 subreactor.
 * 
 * @param numThreads 
 */
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}


/**
 * @brief 启动TcpServer服务。
 * 
 * 做了两件事，
 * 
 * 第一件事：在线程池对象中正式创建numThreads个线程
 * 
 * 第二件事：启动当前TcpServer服务对应的EventLoop事件循环，监听新的客户连接
 * 
 */
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询算法，选择一个subLoop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd, // Socket Channel
        localAddr,
        peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}