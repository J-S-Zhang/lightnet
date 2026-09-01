#include "lightnet/rpc/rpc_codec.h"

#include <cstring>

namespace lightnet {

Buffer RpcCodec::encode(const RpcMessage& msg) {
    Buffer buf;
    buf.ensure_writable(4 + 1 + 1 + 8 + 2 + msg.method.size() + 4 + msg.payload.size());
    // 固定头：magic + version + type + request_id
    uint32_t magic = kMagic;
    buf.append(reinterpret_cast<const char*>(&magic), 4);
    uint8_t version = kVersion;
    buf.append(reinterpret_cast<const char*>(&version), 1);
    uint8_t type = static_cast<uint8_t>(msg.type);
    buf.append(reinterpret_cast<const char*>(&type), 1);
    uint64_t req_id = msg.request_id;
    buf.append(reinterpret_cast<const char*>(&req_id), 8);
    // 变长：method + payload
    uint16_t method_len = static_cast<uint16_t>(msg.method.size());
    buf.append(reinterpret_cast<const char*>(&method_len), 2);
    buf.append(msg.method.data(), msg.method.size());
    uint32_t body_len = static_cast<uint32_t>(msg.payload.size());
    buf.append(reinterpret_cast<const char*>(&body_len), 4);
    buf.append(msg.payload.data(), msg.payload.size());
    return buf;
}

bool RpcCodec::decode(Buffer* buf, RpcMessage* out) {
    if (buf->readable_bytes() < 16) return false;
    const char* p = buf->peek();
    uint32_t magic;
    std::memcpy(&magic, p, 4);
    if (magic != kMagic) return false;
    uint8_t version = static_cast<uint8_t>(p[4]);
    if (version != kVersion) return false;
    uint16_t method_len;
    std::memcpy(&method_len, p + 14, 2);
    const size_t header_size = 16 + method_len + 4;
    if (buf->readable_bytes() < header_size) return false;
    uint32_t body_len;
    std::memcpy(&body_len, p + 16 + method_len, 4);
    const size_t total = header_size + body_len;
    if (buf->readable_bytes() < total) return false;
    out->type = static_cast<RpcMessageType>(static_cast<uint8_t>(p[5]));
    std::memcpy(&out->request_id, p + 6, 8);
    out->method.assign(p + 16, method_len);
    out->payload.assign(p + header_size, body_len);
    buf->retrieve(total);
    return true;
}

}  // namespace lightnet
