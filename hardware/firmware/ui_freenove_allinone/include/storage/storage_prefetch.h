#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace storage {

struct StoragePrefetchRequest {
  char path[96] = {0};
  uint32_t offset = 0U;
  uint16_t bytes = 0U;
};

struct StoragePrefetchChunk {
  uint16_t bytes = 0U;
  bool eof = false;
  uint8_t data[1536] = {0};
};

class StoragePrefetch {
 public:
  static constexpr uint8_t kRequestDepth = 4U;

  bool pushRequest(const StoragePrefetchRequest& request) {
    if (request_count_ >= kRequestDepth) {
      return false;
    }
    request_queue_[request_write_] = request;
    request_write_ = static_cast<uint8_t>((request_write_ + 1U) % kRequestDepth);
    ++request_count_;
    return true;
  }

  bool popRequest(StoragePrefetchRequest* out_request) {
    if (out_request == nullptr || request_count_ == 0U) {
      return false;
    }
    *out_request = request_queue_[request_read_];
    request_read_ = static_cast<uint8_t>((request_read_ + 1U) % kRequestDepth);
    --request_count_;
    return true;
  }

  uint8_t pendingRequests() const {
    return request_count_;
  }

 private:
  StoragePrefetchRequest request_queue_[kRequestDepth] = {};
  uint8_t request_read_ = 0U;
  uint8_t request_write_ = 0U;
  uint8_t request_count_ = 0U;
};

}  // namespace storage
