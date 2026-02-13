#pragma once

#include <Arduino.h>
#include <FS.h>

enum class CatalogCodec : uint8_t {
  kUnknown = 0,
  kMp3,
  kWav,
  kAac,
  kFlac,
  kOpus,
};

const char* catalogCodecLabel(CatalogCodec codec);
CatalogCodec catalogCodecFromPath(const char* path);

struct TrackEntry {
  char path[120] = {};
  char title[40] = {};
  char artist[32] = {};
  char album[32] = {};
  char codec[8] = {};
  uint32_t durationMs = 0;
  uint32_t sizeBytes = 0;
};

struct CatalogStats {
  uint16_t tracks = 0;
  uint16_t folders = 0;
  uint32_t scanMs = 0;
  bool indexed = false;
  bool metadataBestEffort = true;
};

class TrackCatalog {
 public:
  static constexpr uint16_t kMaxTracks = 250;
  static constexpr uint8_t kDefaultMaxDepth = 4;

  void clear();

  bool scan(fs::FS& storage,
            const char* rootPath,
            uint8_t maxDepth,
            uint32_t metadataTimeoutMs,
            CatalogStats* outStats = nullptr);
  bool loadIndex(fs::FS& storage, const char* path, CatalogStats* outStats = nullptr);
  bool saveIndex(fs::FS& storage, const char* path) const;

  bool appendFallbackPath(const char* path, uint32_t sizeBytes);

  uint16_t size() const;
  const TrackEntry* entry(uint16_t index) const;
  int16_t indexOfPath(const char* path) const;

  uint16_t listByPrefix(const char* prefix,
                        uint16_t offset,
                        uint16_t limit,
                        Print& out) const;
  uint16_t countByPrefix(const char* prefix) const;

 private:
  static bool isSupportedPath(const char* path, CatalogCodec* outCodec = nullptr);
  static int compareNatural(const char* lhs, const char* rhs);
  static void sanitizeText(char* text, size_t len);
  static bool copyStr(char* out, size_t outLen, const char* in);
  static bool startsWithPathPrefix(const char* path, const char* prefix);

  void parseMetadata(fs::FS& storage, TrackEntry* entry, uint32_t timeoutMs);
  void parseId3v2(fs::File& file, TrackEntry* entry, uint32_t timeoutMs);
  void parseId3v1(fs::File& file, TrackEntry* entry);
  bool addTrackEntry(const TrackEntry& entry);
  void sortEntries();

  bool scanDirRecursive(fs::FS& storage,
                        const char* dirPath,
                        uint8_t depth,
                        uint8_t maxDepth,
                        uint32_t metadataTimeoutMs,
                        CatalogStats* stats);

  TrackEntry entries_[kMaxTracks];
  uint16_t count_ = 0;
};
