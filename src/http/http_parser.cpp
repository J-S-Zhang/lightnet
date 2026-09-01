#include "lightnet/http/http_parser.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace lightnet {

HttpParser::HttpParser()
    : state_(HttpParseState::kStartLine),
      content_length_(0),
      keep_alive_(true) {}

HttpParseState HttpParser::parse(Buffer* buf) {
    while (state_ != HttpParseState::kComplete && state_ != HttpParseState::kError) {
        if (state_ == HttpParseState::kStartLine || state_ == HttpParseState::kHeaders) {
            const char* crlf = buf->find_crlf();
            if (!crlf) return state_;  // 半包：等待更多数据
            const char* line_start = buf->peek();
            const char* line_end = crlf;
            if (state_ == HttpParseState::kStartLine) {
                if (!parse_start_line(line_start, line_end)) {
                    state_ = HttpParseState::kError;
                    return state_;
                }
                state_ = HttpParseState::kHeaders;
            } else {
                if (line_start == line_end) {
                    // 空行：header 段结束
                    buf->retrieve(2);
                    const auto cl = request_.get_header("Content-Length");
                    if (!cl.empty()) {
                        content_length_ = static_cast<size_t>(std::stoul(cl));
                        state_ = HttpParseState::kBody;
                    } else {
                        state_ = HttpParseState::kComplete;
                    }
                    continue;
                }
                if (!parse_header_line(line_start, line_end)) {
                    state_ = HttpParseState::kError;
                    return state_;
                }
            }
            buf->retrieve(static_cast<size_t>(crlf - line_start + 2));
        } else if (state_ == HttpParseState::kBody) {
            if (buf->readable_bytes() < content_length_) {
                return state_;  // body 未收齐
            }
            request_.body = buf->retrieve_as_string(content_length_);
            state_ = HttpParseState::kComplete;
        }
    }
    return state_;
}

void HttpParser::reset() {
    state_ = HttpParseState::kStartLine;
    request_ = {};
    content_length_ = 0;
    keep_alive_ = true;
}

bool HttpParser::parse_start_line(const char* begin, const char* end) {
    const char* sp1 = std::find(begin, end, ' ');
    if (sp1 == end) return false;
    const char* sp2 = std::find(sp1 + 1, end, ' ');
    if (sp2 == end) return false;
    request_.method.assign(begin, sp1);
    request_.path.assign(sp1 + 1, sp2);
    request_.version.assign(sp2 + 1, end);
    if (request_.version.size() < 8 || request_.version.compare(0, 5, "HTTP/") != 0) {
        return false;
    }
    if (request_.path.empty() || request_.path[0] != '/') {
        return false;
    }
    return true;
}

bool HttpParser::parse_header_line(const char* begin, const char* end) {
    const char* colon = std::find(begin, end, ':');
    if (colon == end) return false;
    std::string key(begin, colon);
    const char* val_start = colon + 1;
    while (val_start < end && std::isspace(static_cast<unsigned char>(*val_start))) ++val_start;
    std::string value(val_start, end);
    request_.headers[key] = value;
    if (key == "Connection" || key == "connection") {
        keep_alive_ = (value != "close");
    }
    return true;
}

}  // namespace lightnet
