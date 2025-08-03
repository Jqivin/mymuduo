#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Buffer.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }

    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
    :loop_(CheckLoopNotNull(loop))
    , name_(name)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop,sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)  // 64M
{
    // 给chnannel设置回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleClose,this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
    
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            // 放在loop的线程中执行
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size())
            );
        }
    }
}
/**
 * 发送数据  应用写的快， 而内核发送数据慢， 需要把待发送数据写入缓冲区， 而且设置了水位回调（防止发送太快）
 */ 
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0; // 已发送的数据
    ssize_t remaining = len; // 未发送的数据，初始值就是len（所有的都没发送呢）
    bool faultError = false; // 记录是否产生错误

    if(state_ == kDisconnected)
    { // 之前调用过该connection的shutdown，不能再进行发送了
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {// 表示channel_第一次开始写数据（没有注册epollout事件，因为默认都是注册的epollin事件），而且缓冲区没有待发送数据
        // 这个时候直接发送数据就可以了
        nwrote = ::write(channel_->fd(),data,len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 说明已经发送了len长度的大小数据，已经将data中数据全都发送出去了
                // 所以就不需要再给channel设置epollout事件了，也就是不需要执行TcpConnection::handleWrite()函数了（这个函数在Channel::handleEventWithGuard执行的回调）
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this())
                );
            }
        }
        else
        {
            // 发送失败了
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {// 非阻塞没有数据，正常的返回
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }

        // 并没有发生错误,是数据没有发送完，剩余的数据需要保存到缓冲区当中，然后给channel注册epollout事件，
        // poller发现当前的tcp发送缓冲区有剩余空间，会通知相应的socket-channel，调用writeCallback_回调方法（TcpConnection::handleWrite方法)，
        // 最终把发送缓冲区中的数据全部发送完成
        if(!faultError && remaining > 0)
        {
            ssize_t oldlen = outputBuffer_.readableBytes();  // 之前遗留的未发送的数据大小
            if(oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_
             && highWaterMarkCallback_)
            {
            // 之前遗留的未发送的数据大小比水位线小，加上这次未发送的比水位线高，回调给用户高水位回调函数
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_,shared_from_this(),oldlen + remaining)
            );
            }

            // 开始往outputBuffer中追加数据
            outputBuffer_.append((char*)data + nwrote,remaining);
           if(!channel_->isWriting())
           {
                channel_->enableWriting(); // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
                // 真正发送剩余数据是在TcpConnection::handleWrite()中，这里不发送
           }
        }
    }


}
// 关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdownWrite(); // 关闭写端，会在channle中触发EPOLLHUP（默认都会注册），会回调closeCallback（最终执行到hanleClose）
    }
    // 数据如果没发送完，会在handleWrite发送完后执行此函数
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 默认只注册读事件 向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所有感兴趣的事件，从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n >0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage，也就是给用户通知读数据
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n == 0)
    {
        // 连接已经关闭
         handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())  // 判断是否可写，也就是是否注册了写事件
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);  // 这里面已经写到fd中了
        if(n > 0)
        {
            // 已经发送到网络中了n个数据了，需要清理一下已经发送的n个数据
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {// 已经发送完成了
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

// Poller->Channel->TcpConnection::hanleClose->TcpServer::removeConnection->TcpConnection::connectDestroyed
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  // 执行连接关闭的回调
    closeCallback_(connPtr); // 关闭连接的回调  执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}