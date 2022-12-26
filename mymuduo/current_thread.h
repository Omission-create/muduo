#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    //__thread 相当于c++ local_thread 表示对于每个线程都有该变量的一个拷贝
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
            cacheTid();
        return t_cachedTid;
    }
}