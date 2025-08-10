#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲器类型
class Buffer 
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
                : buffer_(kCheapPrepend + initialSize)
                , readerIndex_(kCheapPrepend)
                , writerIndex_(kCheapPrepend)
                {}
    
     // 可读空间字节
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    // 可写空间字节
    size_t writeableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    // 预留空间字节
    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区可读数据的起始地址
    const char* peek() const
    {
        return &*buffer_.begin() + readerIndex_;
    } 

    // OnMessage触发时，用户读取一部分数据len长度，需要将readerIndex_复位到正确的位置
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {// 值读取了一部分，就是len，还剩下readerIndex_ += len -> writerIndex_
            readerIndex_ += len;
        }
        else// len == readableBytes() 读取了所有可读的数据
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(),len);
        retrieve(len);   // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }
    // 确保有len长度的空间是可以写入buffer的   buffer_.size() - writerIndex 与len的关系主要判断  
    void ensureWriteableBytes(size_t len)
    {
        if(writeableBytes() < len)
        {
            // 进行扩容
            makeSpace(len);
        }
    }

    void append(const char* data,size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd,int* saveErrno);
    // 向fd上写数据
    ssize_t writeFd(int fd,int* saveErrno);
    
private:
    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    // 扩容函数
    void makeSpace(size_t len)
    {
        if(prependableBytes() - kCheapPrepend + writeableBytes() < len)
        {
            // 所有可用于写操作的空间都不足以写下len长度的数据,直接扩容len长度
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            // 1.移动可读数据，2.移动可写下标 
            // char*是天然的随机访问迭代器，符合++ -- 的操作，可以使用std::copy函数
            std::copy(begin()+readerIndex_,
                        begin() + writerIndex_,
                        begin() + kCheapPrepend);
            readerIndex_ =  kCheapPrepend;
            writerIndex_ = kCheapPrepend + readableBytes();
        }
    }
    std::vector<char> buffer_;   // 存储数据
    size_t readerIndex_;        // 可读空间起始值   
    size_t writerIndex_;       // 可写空间起始值
};