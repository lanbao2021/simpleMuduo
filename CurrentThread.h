#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    // __thread修饰的变量是线程局部存储的，每个线程有一份独立实体，各个线程的值互不干扰
    // t_cachedTid全局变量是线程局部存储的tid
    extern __thread int t_cachedTid;

    void cacheTid(); // 缓存线程的tid

    inline int tid() // 获取线程的tid
    {
        // __builtin_expect是GCC内建函数，是为了分支优化而使用的
        // t_cachedTid == 0为真的概率大，告诉编译器这个分支是更有可能执行的
        // __builtin_expect(t_cachedTid == 0, 0)表示t_cachedTid == 0的概率很小
        // 后面的0表示t_cachedTid == 0的概率很小，如果是1表示t_cachedTid == 0的概率很大
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid(); // 没有缓存，则缓存线程的tid
        }
        return t_cachedTid; // 返回线程的tid
    }
}