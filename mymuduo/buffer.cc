#include "buffer.h"
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

/**
 * @brief 从fd上读取数据（Poller工作LT模式）
 *
 * @param fd
 * @param saveErrno
 * @return ssize_t
 */
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536]; // 栈上空间 64KB
    struct iovec vec[2];
    const size_t writable = writeableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // when there is enough space in this buffer, don't read into extrabuf
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // 一次读写至少保证64K
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        writeIndex_ += n;
    }
    else // extrabuf里面也写入数据
    {
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writeIndex_开始写n-writeable的数据
    }

    return n;
}

// 向fd写数据
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}