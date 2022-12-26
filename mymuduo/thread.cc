#include "thread.h"
#include "current_thread.h"
#include <semaphore.h>

using namespace std;

// 原子变量不能调用拷贝构造函数
// 一个新变量被定义一定会调用拷贝构造函数，不可能是拷贝赋值函数
std::atomic_int Thread::numCreated_{0};

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{
    set_DefaultName();
}
Thread::~Thread()
{
    // thread::join 主线程阻塞等待子线程执行结束
    // thread::detach 子线程脱离主线程，成为守护进程
    if (started_ && !joined_)
    {
        thread_->detach(); // thread类提供的设置线程分离
    }
}

// 一个Thread对象对应个一个线程
void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // 开启线程 lambda表达式
    thread_ = make_shared<thread>(thread([&]()
                                         {
        //获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程， 专门执行线程函数
        func_(); }));

    // 这里必须等待上面新创建线程的tid值
    sem_wait(&sem); // 信号量阻塞主线程
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::set_DefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}