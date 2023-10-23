#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

/**
 * @brief 线程类 - 封装了线程的各项操作
 *
 */
class Thread : noncopyable
{
public:
    // 那如果要定义有多个参数的函数怎么办？使用bind函数
    // 线程函数返回值为空即可，这个返回值没啥意义，所以统一为void
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start(); // 正式开始执行线程
    void join();  // 待补充（detech和join是互斥的要记得）

    bool started() const { return started_; } // 获取线程的状态，是否启动

    pid_t tid() const { return tid_; } // 获取线程的tid

    const std::string &name() const { return name_; } // 获取线程的名字
    static int numCreated() { return numCreated_; }   // 获取创建的线程的数量

private:
    void setDefaultName(); // 设置线程的默认名字

    bool started_; // 线程是否启动
    bool joined_;  // 待补充（detech和join是互斥的要记得）

    std::shared_ptr<std::thread> thread_; // 该成员指向堆内存，所以需要用智能指针帮忙自动释放内存

    pid_t tid_;        // 线程的tid
    ThreadFunc func_;  // 线程的执行函数
    std::string name_; // 线程的名字

    static std::atomic_int numCreated_; // static变量，所有Thread类共享，记录创建了多少个线程
};