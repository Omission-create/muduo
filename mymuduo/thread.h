#pragma once

#include "noncopyable.h"
#include <functional>
#include <thread>
#include <memory>
#include <atomic>
#include <string>
#include <unistd.h>

class Thread : noncopyable
{
public:
    // treadfunc需要参数可以通过bind绑定器实现
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = "");
    ~Thread();

    void start();
    void join();

    bool started() { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }
    static int numCreated() { return numCreated_; }

private:
    void set_DefaultName();

    bool started_;
    bool joined_;
    // std::thread thread_;//创建thread对象的时候线程就会启动，但是我们需要控制线程启动
    std::shared_ptr<std::thread> thread_; // tread智能指针
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
};