#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace audio {

enum class AudioCommandType : uint8_t {
  kNone = 0,
  kPlay,
  kStop,
  kSetVolume,
};

struct AudioCommand {
  AudioCommandType type = AudioCommandType::kNone;
  char path[96] = {0};
  uint8_t value = 0U;
};

struct AudioStatus {
  bool playing = false;
  uint32_t underrun_count = 0U;
  uint16_t buffered_chunks = 0U;
};

struct AudioByteRing {
  static constexpr size_t kChunkBytes = 1536U;
  static constexpr size_t kSlotCount = 96U;

  uint8_t* data = nullptr;
  size_t capacity_bytes = 0U;
  size_t chunk_bytes = kChunkBytes;
  uint16_t write_slot = 0U;
  uint16_t read_slot = 0U;
  uint16_t used_slots = 0U;
};

class AudioPipeline {
 public:
  static constexpr uint8_t kCommandQueueDepth = 8U;

  bool begin(AudioByteRing* ring, uint8_t* backing, size_t backing_bytes) {
    if (ring == nullptr || backing == nullptr) {
      return false;
    }
    if (backing_bytes < (AudioByteRing::kChunkBytes * AudioByteRing::kSlotCount)) {
      return false;
    }
    ring_ = ring;
    ring_->data = backing;
    ring_->capacity_bytes = backing_bytes;
    ring_->chunk_bytes = AudioByteRing::kChunkBytes;
    ring_->write_slot = 0U;
    ring_->read_slot = 0U;
    ring_->used_slots = 0U;
    return true;
  }

  bool pushChunk(const uint8_t* chunk, size_t bytes) {
    if (ring_ == nullptr || chunk == nullptr || bytes > ring_->chunk_bytes || ring_->used_slots >= AudioByteRing::kSlotCount) {
      return false;
    }
    uint8_t* dst = ring_->data + static_cast<size_t>(ring_->write_slot) * ring_->chunk_bytes;
    std::memcpy(dst, chunk, bytes);
    if (bytes < ring_->chunk_bytes) {
      std::memset(dst + bytes, 0, ring_->chunk_bytes - bytes);
    }
    ring_->write_slot = static_cast<uint16_t>((ring_->write_slot + 1U) % AudioByteRing::kSlotCount);
    ++ring_->used_slots;
    return true;
  }

  bool popChunk(uint8_t* out_chunk, size_t out_bytes) {
    if (ring_ == nullptr || out_chunk == nullptr || out_bytes < ring_->chunk_bytes || ring_->used_slots == 0U) {
      return false;
    }
    const uint8_t* src = ring_->data + static_cast<size_t>(ring_->read_slot) * ring_->chunk_bytes;
    std::memcpy(out_chunk, src, ring_->chunk_bytes);
    ring_->read_slot = static_cast<uint16_t>((ring_->read_slot + 1U) % AudioByteRing::kSlotCount);
    --ring_->used_slots;
    return true;
  }

  uint16_t bufferedChunks() const {
    return (ring_ != nullptr) ? ring_->used_slots : 0U;
  }

 private:
  AudioByteRing* ring_ = nullptr;
};

}  // namespace audio
