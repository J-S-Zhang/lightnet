#include "lightnet/core/buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

namespace lightnet {

Buffer::Buffer() : Buffer(kInitialSize) {}

Buffer::Buffer(size_t initial_size)
    : buffer_(kCheapPrepend + initial_size),
      reader_index_(kCheapPrepend),
      writer_index_(kCheapPrepend) {}

void Buffer::retrieve(size_t len) {
    if (len < readable_bytes()) {
        reader_index_ += len;
    } else {
        retrieve_all();
    }
}

void Buffer::retrieve_all() {
    reader_index_ = kCheapPrepend;
    writer_index_ = kCheapPrepend;
}

void Buffer::retrieve_until(const char* end) {
    retrieve(static_cast<size_t>(end - peek()));
}

void Buffer::append(const char* data, size_t len) {
    ensure_writable(len);
    std::memcpy(begin_write(), data, len);
    has_written(len);
}

std::string Buffer::retrieve_as_string(size_t len) {
    len = std::min(len, readable_bytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieve_all_as_string() {
    return retrieve_as_string(readable_bytes());
}

void Buffer::ensure_writable(size_t len) {
    if (writable_bytes() < len) {
        make_space(len);
    }
}

void Buffer::make_space(size_t len) {
    if (writable_bytes() + prependable_bytes() < len + kCheapPrepend) {
        // 前置空间 + 尾部空间都不够，直接扩容
        size_t new_size = writer_index_ + len;
        if (new_size > kMaxCapacity) {
            new_size = kMaxCapacity;
        }
        buffer_.resize(new_size);
    } else {
        // 将未读数据前移，复用 reader 前的 prependable 区域
        size_t readable = readable_bytes();
        std::memmove(begin() + kCheapPrepend, peek(), readable);
        reader_index_ = kCheapPrepend;
        writer_index_ = reader_index_ + readable;
    }
}

ssize_t Buffer::read_fd(int fd, int* saved_errno) {
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writable_bytes();
    // 第一块：写入 buffer 内部可写区
    vec[0].iov_base = begin_write();
    vec[0].iov_len = writable;
    // 第二块：栈上备用区，防止单次 read 超过 buffer 容量
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writer_index_ += n;
    } else {
        // readv 溢出部分落在 extrabuf，再 append 进 buffer
        writer_index_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

const char* Buffer::find_crlf() const {
    return find_crlf(peek());
}
//crlf查找实现，从start开始查找，到写指针位置结束，返回第一个\r\n的位置，如果找不到返回nullptr
const char* Buffer::find_crlf(const char* start) const {
    const char* crlf = std::search(start, begin() + writer_index_, "\r\n", "\r\n" + 2);
    return crlf == begin() + writer_index_ ? nullptr : crlf;
}

}  // namespace lightnet
