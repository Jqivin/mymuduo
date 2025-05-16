#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <atomic>

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const {return started_;}
    pid_t tid() const {return tid_;}
    const std::string& name() const {return name_;}

    static int numCreated() {return numCreated_;}

private:

    void setDefaultName();
    
    bool started_;
    bool joined_;

    // 注意这里不要使用成员变量，因为线程类定义就启动了线程，需要使用指针
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;    // 存储线程函数
    std::string name_;      // 线程名称

    static std::atomic_int numCreated_;   //记录创建几个线程对象
};