#pragma once

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

/**
 * 用户使用muduo编写服务器程序
 */ 
// 对外的服务器编程使用的类
class TcpServer
{
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
public:
    TcpServer(EventLoop* loop,
        const InetAddress &listenAddr,
        const std::string &nameArg,
        Option option = kNoReusePort);
    ~TcpServer();

    void setThreadNums(int threadNums);     // 设置底层subloop的个数

    // 设置回调
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    
    EventLoop* loop_;       // baseLoop 用户定义的loop
    std::string name_;      
    std::string ipPort_;

    std::unique_ptr<Acceptor> acceptor_;    //运行在mainloop，任务监听新用户的连接
    std::shared_ptr<EventLoopThreadPool> threadPool_;   // one loop peer thread

    ConnectionCallback connectionCallback_; // 有新连接时的回调
    MessageCallback messageCallback_; // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调

    std::atomic<int> started_;
    int nextConnId_;                // 用于分配subloop
    ConnectionMap connections_;     // 保存所有的连接

};