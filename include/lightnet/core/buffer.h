#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lightnet {

/// @brief 动态可扩容读写缓冲区（类似 muduo Buffer）
/// 用于 TCP 连接的输入/输出缓冲，支持增量读写与 CRLF 查找
class Buffer {
public:
    static constexpr size_t kInitialSize = 1024;       ///< 默认初始可写区域大小（字节）
    static constexpr size_t kCheapPrepend = 8;         ///< 预留头部空间，便于 prepend 操作
    static constexpr size_t kMaxCapacity = 64 * 1024 * 1024;  ///< 缓冲区最大容量上限

    /// @brief 使用默认初始大小构造缓冲区
    Buffer();
    /// @brief 指定初始可写区域大小构造缓冲区
    explicit Buffer(size_t initial_size);

    /// @brief 可读字节数（尚未被 retrieve 的数据）
    size_t readable_bytes() const { return writer_index_ - reader_index_; }
    /// @brief 可写字节数（vector 末尾剩余空间）
    size_t writable_bytes() const { return buffer_.size() - writer_index_; }
    /// @brief 可前置空间（reader 之前可复用的区域）
    size_t prependable_bytes() const { return reader_index_; }

    /// @brief 返回当前可读数据的起始指针（只读）
    const char* peek() const { return begin() + reader_index_; }
    /// @brief 返回当前可写位置的起始指针
    char* begin_write() { return begin() + writer_index_; }

    /// @brief 消费 len 字节已读数据，移动 reader 指针
    void retrieve(size_t len);
    /// @brief 清空所有可读数据，重置读写指针
    void retrieve_all();
    /// @brief 消费从 peek() 到 end 之间的数据
    void retrieve_until(const char* end);

    /// @brief 追加数据到缓冲区末尾
    void append(const char* data, size_t len);
    /// @brief 追加 std::string 到缓冲区末尾
    void append(const std::string& str) { append(str.data(), str.size()); }

    /// @brief 取出 len 字节并转为 string，同时消费这些数据
    std::string retrieve_as_string(size_t len);
    /// @brief 取出全部可读数据并转为 string
    std::string retrieve_all_as_string();

    /// @brief 确保至少有 len 字节可写空间，不足则扩容或整理
    void ensure_writable(size_t len);
    /// @brief 标记已写入 len 字节，推进 writer 指针
    void has_written(size_t len) { writer_index_ += len; }

    /// @brief 从 fd 读取数据到缓冲区（使用 readv 减少系统调用）
    /// @param fd 文件描述符
    /// @param saved_errno 失败时保存 errno
    /// @return 读取字节数，0 表示 EOF，-1 表示错误
    ssize_t read_fd(int fd, int* saved_errno);

    /// @brief 在可读区域查找 \r\n
    const char* find_crlf() const;
    /// @brief 从 start 开始在可读区域查找 \r\n
    const char* find_crlf(const char* start) const;

private:
    /// @brief 返回底层 vector 数据区起始指针（可写）
    char* begin() { return buffer_.data(); }
    /// @brief 返回底层 vector 数据区起始指针（只读）
    const char* begin() const { return buffer_.data(); }
    /// @brief 扩容或内部整理以腾出 len 字节可写空间
    void make_space(size_t len);

    std::vector<char> buffer_;  ///< 底层存储
    size_t reader_index_;       ///< 读指针：已读数据的起始位置
    size_t writer_index_;       ///< 写指针：已写数据的末尾位置
};

}  // namespace lightnet
