#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false),        // 刚创建Thread对象时仅仅是初始化了一些成员变量
      joined_(false),         // 待补充（detech和join是互斥的要记得）
      tid_(0),                // 线程的id，还没正式创建所以暂时设为0
      func_(std::move(func)), // 线程启动后执行的内容
      name_(name)             // 线程的名字
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_) // detech和join是互斥的要记得
    {
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}

void Thread::start() // 一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;                // 信号量
    sem_init(&sem, false, 0); // 初始化信号量

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread(
        // 给std::thread传入了一个不带参数的lambda表达式，&表示对外部变量进行引用
        [&]()
        {
            tid_ = CurrentThread::tid(); // 获取线程的tid值

            sem_post(&sem); // 发出信号，说明创建线程成功咯

            func_(); // 线程的工作函数
        }));

    sem_wait(&sem); // 这里必须等待获取上面新创建的线程的tid值
}

void Thread::join()
{
    joined_ = true;  // joined后就不能detached了
    thread_->join(); // 这是std::thread::join，不要理解成循环调用了
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}