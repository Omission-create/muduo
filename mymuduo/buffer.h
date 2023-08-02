#pragma once
#include <vector>
#include <unistd.h>
#include <string>

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readIndex_(kCheapPrepend),
          writeIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const
    {
        return writeIndex_ - readIndex_;
    }

    size_t writeableBytes() const
    {
        return buffer_.size() - writeIndex_;
    }

    size_t prependableBytes() const
    {
        return readIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const
    {
        return begin() + readIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readIndex_ += len; // 应用只读取了刻度缓冲区数据的一部分，就是len，还剩下readerIndex_+=len
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }

    // 复位
    void retrieveAll()
    {
        readIndex_ = writeIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的的bbuffer数据转成string类型数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句把缓冲区的中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size() - writeIndex_ len
    void ensureWriteableBytes(size_t len)
    {
        if (writeableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
    }

    // 把data内存上的数据添加到可写缓冲区上
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writeIndex_ += len;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);

    // 向fd写数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // socket编程需要vector底层数组的裸指针
    char *begin()
    {
        return &*buffer_.begin();
    }

    char *beginWrite()
    {
        return begin() + writeIndex_;
    }

    const char *begin() const // 仅有返回值不同的函数无法重载
    {
        return &*buffer_.begin();
    }

    const char *beginWrite() const // 仅有返回值不同的函数无法重载
    {
        return begin() + writeIndex_;
    }

    /*
        kCheapPrepend |  have read | reader | writer |
                                   |
                                readindex_
    */
    void makeSpace(size_t len)
    {
        if (writeableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writeIndex_ + len);
        }
        else
        {
            // move readable data to the front , make space inside buffer
            size_t readable = readableBytes();
            std::copy(begin() + readIndex_, begin() + writeIndex_, begin() + kCheapPrepend);
            readIndex_ = kCheapPrepend;
            writeIndex_ = readIndex_ + readable;
        }
    }

    std::vector<char> buffer_; // 类对象释放时，vector自动销毁
    size_t readIndex_;
    size_t writeIndex_;
};