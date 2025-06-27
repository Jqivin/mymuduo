#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
* 从fd上读取数据，Poller是工作在LT模式下的
* Buffer缓冲区是由大小的，但是从fd上读取数据的时候，不知道tcp数据的大小
*/
ssize_t Buffer::readFd(int fd,int* saveErrno)
{
    char extraBuf[65536] = {0}; // 从栈内存开辟64K大小的数据  为什么是64K数据？参考：
    struct iovec vec[2];
    const size_t writeable = writeableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writeable;

    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof extraBuf;

    // 如果缓冲区剩余空间大于64K，就不需要使用extraBuf
    const int iovcnt = (writeable < sizeof extraBuf ? 2 : 1);
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n < 0)
    {
        *saveErrno = errno;
    }
    else if(n <= writeable)
    {
        // Buffer中剩余空间足够
        writerIndex_ += n;
    }
    else
    {
        // 写入了两块空间
        writerIndex_ = buffer_.size();  // Buffer中已经写满了
        append(extraBuf,n - writeable); // writerIndex_开始写 n - writable大小的数据,其中append中有扩容操作
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd,peek(),readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}