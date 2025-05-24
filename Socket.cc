#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if(0 != bind(sockfd_,(sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in)))
    {
        // handle error
        LOG_FATAL("bind socket:%d failed",sockfd_);
    }
   
}

void Socket::listen()
{
    // 等待连接队列最长1024
    if(0 != ::listen(sockfd_, 1024))
    {
        // handle error
        LOG_FATAL("listen socket:%d failed",sockfd_);
    }
}

int  Socket::accept(InetAddress* peeraddr)
{
    sockaddr_in addr;
    socklen_t addrlen;
    bzero(&addr,sizeof addr);
    int connfd = ::accept(sockfd_, (sockaddr*)&addr,&addrlen);
    if(connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void  Socket::shutdownWrite()
{
    // 关闭写端
   if(::shutdown(sockfd_, SHUT_WR))
   {
        LOG_ERROR("shutdown socket:%d failed",sockfd_);
   }
   
}

// 禁用Nagle算法，减少延迟（适用于需要低延迟的TCP连接）
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    if(setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval) < 0)
    {
        LOG_ERROR("setTcpNoDelay failed");
    }
}

// 设置SO_REUSEADDR，允许端口复用（通常用于服务器重启时端口能立即重用）
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0)
    {
        LOG_ERROR("setReuseAddr failed");
    }
}

// 设置SO_REUSEPORT，允许多个socket绑定同一个端口（多进程/多线程高并发服务器常用）
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval) < 0)
    {
        LOG_ERROR("setReusePort failed");
    }
}

// 设置SO_KEEPALIVE，开启TCP保活机制，检测死连接
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval) < 0)
    {
        LOG_ERROR("setKeepAlive failed");
    }
}