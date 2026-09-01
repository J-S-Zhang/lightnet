#include "lightnet/metrics/metrics.h"

#include <sstream>

namespace lightnet {

namespace {

constexpr size_t kHistogramBuckets = 12;
constexpr std::array<uint64_t, kHistogramBuckets> kBucketBounds = {
    100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000, UINT64_MAX
};

}  // namespace

Metrics& Metrics::instance() {
    static Metrics m;
    return m;
}

void Metrics::Histogram::observe(uint64_t value) {
    sum.fetch_add(value, std::memory_order_relaxed);
    count.fetch_add(1, std::memory_order_relaxed);
    // 找到第一个 le 边界，累加对应 bucket（Prometheus 累积桶）
    for (size_t i = 0; i < kBuckets; ++i) {
        if (value <= kBucketBounds[i]) {
            buckets[i].fetch_add(1, std::memory_order_relaxed);
            break;
        }
    }
}

void Metrics::Histogram::export_to(std::string& out, const std::string& name) const {
    out += "# TYPE " + name + " histogram\n";
    uint64_t cumulative = 0;
    for (size_t i = 0; i < kBuckets; ++i) {
        cumulative += buckets[i].load(std::memory_order_relaxed);
        out += name + "_bucket{le=\"" + std::to_string(kBucketBounds[i]) + "\"} "
               + std::to_string(cumulative) + "\n";
    }
    out += name + "_sum " + std::to_string(sum.load(std::memory_order_relaxed)) + "\n";
    out += name + "_count " + std::to_string(count.load(std::memory_order_relaxed)) + "\n";
}

void Metrics::record_request_latency_us(uint64_t us) {
    request_latency_.observe(us);
}

void Metrics::record_event_loop_lag_us(uint64_t us) {
    event_loop_lag_.observe(us);
}

std::string Metrics::export_prometheus() const {
    std::ostringstream oss;
    oss << "# TYPE lightnet_connections_total counter\n"
        << "lightnet_connections_total "
        << connections_total_.load(std::memory_order_relaxed) << "\n"
        << "# TYPE lightnet_active_connections gauge\n"
        << "lightnet_active_connections "
        << active_connections_.load(std::memory_order_relaxed) << "\n"
        << "# TYPE lightnet_bytes_read_total counter\n"
        << "lightnet_bytes_read_total "
        << bytes_read_.load(std::memory_order_relaxed) << "\n"
        << "# TYPE lightnet_bytes_written_total counter\n"
        << "lightnet_bytes_written_total "
        << bytes_written_.load(std::memory_order_relaxed) << "\n"
        << "# TYPE lightnet_requests_total counter\n"
        << "lightnet_requests_total "
        << requests_total_.load(std::memory_order_relaxed) << "\n";
    std::string out = oss.str();
    request_latency_.export_to(out, "lightnet_request_latency_us");
    event_loop_lag_.export_to(out, "lightnet_event_loop_lag_us");
    return out;
}

}  // namespace lightnet
