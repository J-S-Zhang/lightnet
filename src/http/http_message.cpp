#include "lightnet/http/http_message.h"

#include <cctype>
#include <sstream>

namespace lightnet {

std::string HttpRequest::get_header(const std::string& key, const std::string& default_val) const {
    auto it = headers.find(key);
    if (it != headers.end()) return it->second;
    // 精确匹配失败，逐条做大小写不敏感比较
    for (const auto& [k, v] : headers) {
        if (k.size() == key.size()) {
            bool match = true;
            for (size_t i = 0; i < k.size(); ++i) {
                if (std::tolower(k[i]) != std::tolower(key[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return v;
        }
    }
    return default_val;
}

bool HttpRequest::keep_alive() const {
    const auto conn = get_header("Connection");
    if (!conn.empty()) {
        return conn != "close";
    }
    return version == "HTTP/1.1";
}

void HttpResponse::set_content_type(const std::string& type) {
    headers["Content-Type"] = type;
}

void HttpResponse::set_body(const std::string& content) {
    body = content;
    headers["Content-Length"] = std::to_string(body.size());
}

std::string HttpResponse::serialize() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    for (const auto& [k, v] : headers) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n" << body;
    return oss.str();
}

}  // namespace lightnet
