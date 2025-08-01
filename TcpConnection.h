#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TimeStamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/*
* TcpServer => Acceptor(负责监听新连接)=>有一个用户连接，通过accept函数拿到connfd => 创建TcpConnection并设置回调
* => channel => Poller中 => 有事件发生时调用channel的回调（其实这个回调就是Tcpserver中设置的，也就是用户设置的）
*/
class TcpConnection : noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    
    ~TcpConnection();

    // 返回当前connection所在的事件循环loop
    EventLoop* getLoop() const {return loop_;}
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const {return state_ == kConnected;}

   // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();
   void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }


    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();


private:

    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state) { state_ = state; }

    EventLoop* loop_; // 这里不是baseLoop，因为TcpConnnection都是再subloop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;         // 有新连接时的回调函数
    MessageCallback messageCallback_;       // 有读写消息时的回调函数
    WriteCompleteCallback writeCompleteCallback_;    // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;  // 接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区
};