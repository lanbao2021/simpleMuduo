#include "CurrentThread.h"

namespace CurrentThread 
{
    __thread int t_cachedTid = 0;   

    void cacheTid() // 缓存线程的tid
    {
        if (t_cachedTid == 0) // 如果没有缓存
        {
            // 通过linux系统调用，获取当前线程的tid值
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}