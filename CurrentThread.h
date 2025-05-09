#pragma once

#include <unistd.h>
#include<sys/syscall.h>

namespace CurrentThread
{
    // __thread 保证全局变量t_cachedTid在不同的线程中有不同的值，C++11 中可以使用thread_local 关键字
    extern __thread int t_cachedTid;

    // 由于获取线程的tid需要从内核态切到用户态，所以缓存一下tid，就不用每次获取的时候进行切换
    void cacheTid();

    inline int tid()
    {
        if(__builtin_expect(t_cachedTid == 0,0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}