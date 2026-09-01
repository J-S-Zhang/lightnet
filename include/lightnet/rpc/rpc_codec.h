#pragma once

#include "lightnet/core/buffer.h"

#include <cstdint>
#include <string>

namespace lightnet {

/// @brief RPC 消息类型
enum class RpcMessageType : uint8_t {
    kRequest = 1,   ///< 请求，需要响应
    kResponse = 2,  ///< 响应
    kOneway = 3,    ///< 单向调用，无需响应
};

/// @brief RPC 消息体
struct RpcMessage {
    RpcMessageType type = RpcMessageType::kRequest;  ///< 消息类型
    uint64_t request_id = 0;                         ///< 请求 ID，用于匹配响应
    std::string method;                              ///< 方法名
    std::string payload;                             ///< 业务载荷（字符串）
};

/// @brief 二进制 RPC 编解码（LNRP 协议）
/// 格式: magic(4) | version(1) | type(1) | req_id(8) | method_len(2) | method | body_len(4) | body
class RpcCodec {
public:
    static constexpr uint32_t kMagic = 0x4C4E5250;  ///< 魔数 "LNRP"
    static constexpr uint8_t kVersion = 1;          ///< 协议版本

    /// @brief 将 RpcMessage 编码为 Buffer
    static Buffer encode(const RpcMessage& msg);
    /// @brief 从 Buffer 增量解码一条消息；数据不足返回 false
    static bool decode(Buffer* buf, RpcMessage* out);
};

}  // namespace lightnet
