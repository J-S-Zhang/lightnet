#pragma once

#include <string>
#include <unordered_map>

namespace lightnet {

/// @brief HTTP/1.1 请求对象
struct HttpRequest {
    std::string method;   ///< 请求方法，如 GET、POST
    std::string path;     ///< 请求路径，如 /api/user
    std::string version;  ///< HTTP 版本，如 HTTP/1.1
    std::unordered_map<std::string, std::string> headers;  ///< 请求头
    std::string body;     ///< 请求体

    /// @brief 获取请求头，支持大小写不敏感查找
    std::string get_header(const std::string& key, const std::string& default_val = "") const;
    /// @brief 是否保持长连接（Connection 头或 HTTP/1.1 默认）
    bool keep_alive() const;
};

/// @brief HTTP/1.1 响应对象
struct HttpResponse {
    int status_code = 200;              ///< 状态码
    std::string status_message = "OK";  ///< 状态描述
    std::unordered_map<std::string, std::string> headers;  ///< 响应头
    std::string body;                   ///< 响应体

    /// @brief 设置 Content-Type 头
    void set_content_type(const std::string& type);
    /// @brief 设置 body 并自动更新 Content-Length
    void set_body(const std::string& content);
    /// @brief 序列化为 HTTP/1.1 响应字节流
    std::string serialize() const;
};

}  // namespace lightnet
