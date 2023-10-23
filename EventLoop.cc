#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// __thread修饰的变量是线程局部存储的，每个线程有一份独立实体，各个线程的值互不干扰
// t_loopInThisThread全局变量是线程局部存储的EventLoop指针
// 每个线程只能有一个EventLoop对象，所以这里用t_loopInThisThread来保存EventLoop对象的指针
// 以后要是再创建EventLoop对象，就会直接报错
__thread EventLoop *t_loopInThisThread = nullptr;

// Poller IO复用接口的超时时间
// 在超时前如果监听的fd上没有事件发生那么就阻塞着
const int kPollTimeMs = 10000; // 单位：毫秒

// 创建eventfd，用notify唤醒subloop
int createEventfd()
{
    // 0表示eventfd对象的64位内部计数器的初始值
    // 初始值为0的话，读它的值会阻塞，直到有写入为止
    // EFD_NONBLOCK表示eventfd对象是非阻塞的，读它的值会立即返回，没有可读事件errno被设置为EAGAIN
    // EFD_CLOEXEC表示通过exec调用时关闭父进程的eventfd对象(应该是这样理解的，跟那个EPOLL_CLOEXEC标志位类似)
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),                             // 创建EventLoop对象时还没启动循环，还需要后续调用才能启动
      quit_(false),                                // 显然刚创建EventLoop对象时不会是退出状态
      callingPendingFunctors_(false),              // 标识当前loop是否正在执行的回调操作
      threadId_(CurrentThread::tid()),             // 获取当前线程的thread id，存着，以防止该线程继续创建EventLoop对象(实现One Loop Per Thread)
      poller_(Poller::newDefaultPoller(this)),     // 初始化poller_，其实就是初始化一个EPollPoller对象，因为我们只实现了epoll，没有实现select和poll
      wakeupFd_(createEventfd()),                  // 创建eventfd初始化wakeupFd_
      wakeupChannel_(new Channel(this, wakeupFd_)) // 为wakeupFd_创建对应的channel，每一个fd都有对应的channel，eventfd也不例外
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);

    if (t_loopInThisThread) // 防止同一个thread创建两个EventLoop
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this; // 防止同一个thread创建两个EventLoop
    }

    // 当wakeupChannel_上有读事件发生时，就会调用当前EventLoop的handleRead()方法
    // readCallback_ = std::bind(&EventLoop::handleRead, this) 后this->handleRead()等价于readCallback_()
    // 这里有一个bug，setReadCallback对应using ReadEventCallback = std::function<void(Timestamp)>;而handleRead没有参数，为啥也能正常编译不报错呢？
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this)); // 先设置回调函数
    wakeupChannel_->enableReading();                                          // 开启事件监听
    // enableReading虽然是Channel对象封装的方法，但实际上是这么一个过程：
    // Channel.enableReading -> Channel.update -> EventLoop.updateChannel -> EpollPoller.updateChannel -> EPollPoller.update
    // 为啥要绕一圈呢？因为Poller才能操作fd的属性，所以要借助EventLoop调用epoll的方法
}

EventLoop::~EventLoop()
{
    // 这里不用判断是否在当前线程，因为EventLoop的析构函数只能在创建它的线程中调用
    // 非智能指针管理的成员要自己手动释放
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true; // 进入循环，置为true
    quit_ = false;   // 进入循环之前可能是true，所以要置为false

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        // 要先清空上一次使用的activeChannels_里的内容
        activeChannels_.clear();

        // 获取当前活跃的事件，返回的是发生事件的fd的个数
        // 这里监听了两类fd：一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        // 挨个取出有活跃事件的channel，进行相应处理
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生了事件，上报给EventLoop，EventLoop再调用Channel的handleEvent方法
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前EventLoop事件循环需要处理的回调操作
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
/**
 *              mainLoop
 *
 *                                             no ==================== 生产者-消费者的线程安全的队列
 *
 *  subLoop1     subLoop2     subLoop3
 */
void EventLoop::quit()
{
    quit_ = true;

    // 如果是在其它线程中，调用的quit   在一个subloop(woker)中，调用了mainLoop(IO)的quit
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 在eventloop中执行回调函数
void EventLoop::runInLoop(Functor cb)
{
    // 调用runInLoop的loop对象对应线程和当前线程是同一个线程，就直接执行cb
    // 有点绕，但是要知道，baseloop里面通过轮询算法管理着多个subloop
    // 所以会出现baseloop调用subloop对象的runInLoop方法
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        // 当前线程和loop对象记录的线程id不一致，那么就把cb放到loop对象的等待队列中
        queueInLoop(cb);
    }
}

// 把回调函数cb放入待执行队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    { // 进入临界区
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    } // 离开临界区

    // 当前线程不是loop所在线程，或者loop正在执行回调，那么就唤醒loop所在的线程
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

/**
 * @brief 处理wakeupFd_上的EPOLLIN读事件
 *
 * 我没理解错的话，这里其实就是象征性读一下，清空对应事件的缓冲区（是这样的，2023-10-22）
 *
 * 实际意义是让“LOOP”继续往下执行回调函数，对应doPendingFunctors()
 */
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

// 用来唤醒loop所在的线程的  向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法 =》 Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}