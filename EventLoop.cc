#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

/**
 * @brief 防止一个线程创建多个EventLoop
 *
 * __thread修饰的变量的作用域仅在当前作用域内
 */
__thread EventLoop *t_loopInThisThread = nullptr;

/**
 * @brief Poller IO复用接口的超时时间
 *
 * 在超时前如果监听的fd上没有事件发生那么就阻塞着
 */
const int kPollTimeMs = 10000;

/**
 * @brief Create a Eventfd object
 *
 * 每个EventLoop都会有一个Eventfd，对应 wakeupFd_ 成员变量。
 *
 * 它的作用是让EventLoop里的“Loop”不要继续阻塞在epoll_wait里，继续往下执行，后续还有回调函数需要执行（对应 doPendingFunctors()）
 *
 * 它是如何起到这种通知作用的呢？其实本质上也是让epoll监听文件描述符
 *
 * @return int
 */
int createEventfd()
{
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
      callingPendingFunctors_(false),              // 标识当前loop是否有需要执行的回调操作
      threadId_(CurrentThread::tid()),             // 获取当前线程的thread id，存着，以防止该线程继续创建EventLoop对象(One Loop Per Thread)
      poller_(Poller::newDefaultPoller(this)),     // 初始化poller_，其实就是epoll，因为我们只实现了epoll，没有实现select和poll
      wakeupFd_(createEventfd()),                  // 创建eventfd初始化wakeupFd_
      wakeupChannel_(new Channel(this, wakeupFd_)) // 为wakeupFd_创建对应的channel，每一个fd都有对应的channel
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

    /**
     * @brief 设置wakeupfd的事件类型以及发生事件后的回调操作
     *
     * 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
     *
     * 这里bind的this是EventLoop的this哦，要想清楚，不要误以为是wakeupChannel_的
     *
     * enableReading虽然是Channel对象封装的方法，但实际上是这么一个过程：
     *
     * Channel.enableReading -> Channel.update -> EventLoop.updateChannel -> EpollPoller.updateChannel -> EPollPoller.update
     *
     * 为啥要绕一圈呢？因为Poller才能操作fd的属性，所以要借助EventLoop调用epoll的方法
     */
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
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
        activeChannels_.clear(); // 要先清空
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // 监听两类fd：一种是client的fd，一种是wakeupfd
        for (Channel *channel : activeChannels_) // 挨个取出有活跃事件的channel，进行相应处理
        {
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd《=channel subloop
         * mainLoop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */
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

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中，执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb , 就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程 （这其实是this->wakeup())
    }
}

/**
 * @brief 处理wakeupFd_上的EPOLLIN读事件
 *
 * 我没理解错的话，这里其实就是象征性读一下，清空对应事件的缓冲区
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