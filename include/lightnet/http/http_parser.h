#pragma once

#include "lightnet/core/buffer.h"
#include "lightnet/http/http_message.h"

namespace lightnet {

/// @brief HTTP 解析状态
enum class HttpParseState {
    kStartLine,  ///< 解析请求行
    kHeaders,    ///< 解析请求头
    kBody,       ///< 解析请求体
    kComplete,   ///< 完整请求已就绪
    kError,      ///< 解析错误
};

/// @brief HTTP/1.1 增量解析器（有限状态机）
/// 从 Buffer 中逐行解析，支持不完整请求的分片到达
class HttpParser {
public:
    HttpParser();

    /// @brief 增量解析，返回当前状态
    HttpParseState parse(Buffer* buf);
    /// @brief 重置解析器以处理下一个请求（Keep-Alive）
    void reset();

    const HttpRequest& request() const { return request_; }  ///< 已解析的请求
    bool keep_alive() const { return keep_alive_; }          ///< 是否 Keep-Alive

private:
    /// @brief 解析 "METHOD path HTTP/x.x"
    bool parse_start_line(const char* begin, const char* end);
    /// @brief 解析 "Key: Value"
    bool parse_header_line(const char* begin, const char* end);

    HttpParseState state_;       ///< 当前 FSM 状态
    HttpRequest request_;        ///< 解析结果
    size_t content_length_;      ///< Content-Length 值
    bool keep_alive_;            ///< Connection 头解析结果
};

}  // namespace lightnet
