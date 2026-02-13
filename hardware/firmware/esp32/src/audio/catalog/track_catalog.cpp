#include "track_catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

uint32_t parseSynchsafe32(const uint8_t* in) {
  if (in == nullptr) {
    return 0U;
  }
  return (static_cast<uint32_t>(in[0] & 0x7FU) << 21U) |
         (static_cast<uint32_t>(in[1] & 0x7FU) << 14U) |
         (static_cast<uint32_t>(in[2] & 0x7FU) << 7U) |
         static_cast<uint32_t>(in[3] & 0x7FU);
}

uint32_t parseBigEndian32(const uint8_t* in) {
  if (in == nullptr) {
    return 0U;
  }
  return (static_cast<uint32_t>(in[0]) << 24U) |
         (static_cast<uint32_t>(in[1]) << 16U) |
         (static_cast<uint32_t>(in[2]) << 8U) |
         static_cast<uint32_t>(in[3]);
}

bool readBounded(fs::File& file, uint8_t* out, size_t len, uint32_t timeoutMs) {
  if (out == nullptr || len == 0U) {
    return false;
  }
  const uint32_t startMs = millis();
  size_t pos = 0U;
  while (pos < len) {
    const int v = file.read();
    if (v < 0) {
      if (static_cast<uint32_t>(millis() - startMs) >= timeoutMs) {
        break;
      }
      // Keep metadata probing cooperative without forcing a full 1ms stall.
      delay(0);
      continue;
    }
    out[pos++] = static_cast<uint8_t>(v);
  }
  return pos == len;
}

void trimTrailing(char* text) {
  if (text == nullptr) {
    return;
  }
  size_t len = strlen(text);
  while (len > 0U) {
    const char c = text[len - 1U];
    if (c == '\0' || c == '\r' || c == '\n' || c == '\t' || c == ' ') {
      text[len - 1U] = '\0';
      --len;
      continue;
    }
    break;
  }
}

void trimLeading(char* text) {
  if (text == nullptr) {
    return;
  }
  size_t start = 0U;
  while (text[start] != '\0') {
    const char c = text[start];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++start;
      continue;
    }
    break;
  }
  if (start == 0U) {
    return;
  }
  size_t dst = 0U;
  while (text[start] != '\0') {
    text[dst++] = text[start++];
  }
  text[dst] = '\0';
}

void normalizePath(char* path, size_t len) {
  if (path == nullptr || len == 0U) {
    return;
  }
  for (size_t i = 0U; i < len && path[i] != '\0'; ++i) {
    if (path[i] == '\\') {
      path[i] = '/';
    }
  }
  if (path[0] == '\0') {
    snprintf(path, len, "/");
    return;
  }
  if (path[0] != '/') {
    char tmp[120] = {};
    snprintf(tmp, sizeof(tmp), "/%s", path);
    snprintf(path, len, "%s", tmp);
  }
}

const char* basenamePtr(const char* path) {
  if (path == nullptr) {
    return "";
  }
  const char* slash = strrchr(path, '/');
  return (slash == nullptr) ? path : (slash + 1);
}

int compareSegment(const char* a, const char* b, size_t* usedA, size_t* usedB) {
  size_t ia = 0U;
  size_t ib = 0U;
  if (isdigit(static_cast<unsigned char>(a[0])) != 0 &&
      isdigit(static_cast<unsigned char>(b[0])) != 0) {
    unsigned long va = 0UL;
    unsigned long vb = 0UL;
    while (isdigit(static_cast<unsigned char>(a[ia])) != 0) {
      va = (va * 10UL) + static_cast<unsigned long>(a[ia] - '0');
      ++ia;
    }
    while (isdigit(static_cast<unsigned char>(b[ib])) != 0) {
      vb = (vb * 10UL) + static_cast<unsigned long>(b[ib] - '0');
      ++ib;
    }
    *usedA = ia;
    *usedB = ib;
    if (va < vb) {
      return -1;
    }
    if (va > vb) {
      return 1;
    }
    return 0;
  }

  const unsigned char ca = static_cast<unsigned char>(tolower(a[0]));
  const unsigned char cb = static_cast<unsigned char>(tolower(b[0]));
  *usedA = 1U;
  *usedB = 1U;
  if (ca < cb) {
    return -1;
  }
  if (ca > cb) {
    return 1;
  }
  return 0;
}

}  // namespace

const char* catalogCodecLabel(CatalogCodec codec) {
  switch (codec) {
    case CatalogCodec::kMp3:
      return "MP3";
    case CatalogCodec::kWav:
      return "WAV";
    case CatalogCodec::kAac:
      return "AAC";
    case CatalogCodec::kFlac:
      return "FLAC";
    case CatalogCodec::kOpus:
      return "OPUS";
    case CatalogCodec::kUnknown:
    default:
      return "UNKNOWN";
  }
}

CatalogCodec catalogCodecFromPath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return CatalogCodec::kUnknown;
  }
  String lower(path);
  lower.toLowerCase();
  if (lower.endsWith(".mp3")) {
    return CatalogCodec::kMp3;
  }
  if (lower.endsWith(".wav")) {
    return CatalogCodec::kWav;
  }
  if (lower.endsWith(".aac") || lower.endsWith(".m4a")) {
    return CatalogCodec::kAac;
  }
  if (lower.endsWith(".flac")) {
    return CatalogCodec::kFlac;
  }
  if (lower.endsWith(".opus") || lower.endsWith(".ogg")) {
    return CatalogCodec::kOpus;
  }
  return CatalogCodec::kUnknown;
}

void TrackCatalog::clear() {
  count_ = 0U;
}

bool TrackCatalog::scan(fs::FS& storage,
                        const char* rootPath,
                        uint8_t maxDepth,
                        uint32_t metadataTimeoutMs,
                        CatalogStats* outStats) {
  clear();
  CatalogStats stats;
  const uint32_t beginMs = millis();
  const char* root = (rootPath != nullptr && rootPath[0] != '\0') ? rootPath : "/";
  const bool ok = scanDirRecursive(storage, root, 0U, maxDepth, metadataTimeoutMs, &stats);
  if (ok) {
    sortEntries();
    stats.tracks = count_;
    stats.scanMs = millis() - beginMs;
    stats.indexed = true;
  }
  if (outStats != nullptr) {
    *outStats = stats;
  }
  return ok;
}

bool TrackCatalog::loadIndex(fs::FS& storage, const char* path, CatalogStats* outStats) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  fs::File file = storage.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    return false;
  }

  clear();
  CatalogStats stats;
  stats.indexed = true;

  while (file.available() > 0) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) {
      continue;
    }

    TrackEntry entry;
    memset(&entry, 0, sizeof(entry));

    const int p1 = line.indexOf('\t');
    if (p1 < 0) {
      continue;
    }
    const int p2 = line.indexOf('\t', p1 + 1);
    const int p3 = line.indexOf('\t', (p2 >= 0) ? p2 + 1 : -1);
    const int p4 = line.indexOf('\t', (p3 >= 0) ? p3 + 1 : -1);
    const int p5 = line.indexOf('\t', (p4 >= 0) ? p4 + 1 : -1);
    const int p6 = line.indexOf('\t', (p5 >= 0) ? p5 + 1 : -1);

    if (p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0) {
      continue;
    }

    const String pathValue = line.substring(0, p1);
    const String codecValue = line.substring(p1 + 1, p2);
    const String sizeValue = line.substring(p2 + 1, p3);
    const String titleValue = line.substring(p3 + 1, p4);
    const String artistValue = line.substring(p4 + 1, p5);
    const String albumValue = line.substring(p5 + 1, p6);
    const String durationValue = line.substring(p6 + 1);

    copyStr(entry.path, sizeof(entry.path), pathValue.c_str());
    copyStr(entry.codec, sizeof(entry.codec), codecValue.c_str());
    copyStr(entry.title, sizeof(entry.title), titleValue.c_str());
    copyStr(entry.artist, sizeof(entry.artist), artistValue.c_str());
    copyStr(entry.album, sizeof(entry.album), albumValue.c_str());
    entry.sizeBytes = static_cast<uint32_t>(sizeValue.toInt());
    entry.durationMs = static_cast<uint32_t>(durationValue.toInt());
    normalizePath(entry.path, sizeof(entry.path));
    sanitizeText(entry.title, sizeof(entry.title));
    sanitizeText(entry.artist, sizeof(entry.artist));
    sanitizeText(entry.album, sizeof(entry.album));

    if (!addTrackEntry(entry)) {
      break;
    }
  }
  file.close();

  stats.tracks = count_;
  if (outStats != nullptr) {
    *outStats = stats;
  }
  return count_ > 0U;
}

bool TrackCatalog::saveIndex(fs::FS& storage, const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }

  if (storage.exists(path)) {
    storage.remove(path);
  }

  fs::File file = storage.open(path, FILE_WRITE);
  if (!file || file.isDirectory()) {
    return false;
  }

  for (uint16_t i = 0U; i < count_; ++i) {
    const TrackEntry& e = entries_[i];
    file.printf("%s\t%s\t%lu\t%s\t%s\t%s\t%lu\n",
                e.path,
                e.codec,
                static_cast<unsigned long>(e.sizeBytes),
                e.title,
                e.artist,
                e.album,
                static_cast<unsigned long>(e.durationMs));
  }
  file.close();
  return true;
}

bool TrackCatalog::appendFallbackPath(const char* path, uint32_t sizeBytes) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  TrackEntry entry;
  memset(&entry, 0, sizeof(entry));
  copyStr(entry.path, sizeof(entry.path), path);
  normalizePath(entry.path, sizeof(entry.path));
  const CatalogCodec codec = catalogCodecFromPath(entry.path);
  if (codec == CatalogCodec::kUnknown) {
    return false;
  }
  copyStr(entry.codec, sizeof(entry.codec), catalogCodecLabel(codec));
  entry.sizeBytes = sizeBytes;
  return addTrackEntry(entry);
}

void TrackCatalog::sort() {
  sortEntries();
}

uint16_t TrackCatalog::size() const {
  return count_;
}

const TrackEntry* TrackCatalog::entry(uint16_t index) const {
  if (index >= count_) {
    return nullptr;
  }
  return &entries_[index];
}

int16_t TrackCatalog::indexOfPath(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return -1;
  }
  char normalized[120] = {};
  copyStr(normalized, sizeof(normalized), path);
  normalizePath(normalized, sizeof(normalized));
  for (uint16_t i = 0U; i < count_; ++i) {
    if (strcmp(entries_[i].path, normalized) == 0) {
      return static_cast<int16_t>(i);
    }
  }
  return -1;
}

uint16_t TrackCatalog::listByPrefix(const char* prefix,
                                    uint16_t offset,
                                    uint16_t limit,
                                    Print& out) const {
  const char* safePrefix = (prefix == nullptr) ? "/" : prefix;
  uint16_t total = 0U;
  uint16_t emitted = 0U;
  for (uint16_t i = 0U; i < count_; ++i) {
    const TrackEntry& e = entries_[i];
    if (!startsWithPathPrefix(e.path, safePrefix)) {
      continue;
    }
    ++total;
    if (total <= offset) {
      continue;
    }
    if (emitted >= limit) {
      continue;
    }
    ++emitted;
    const char* title = (e.title[0] != '\0') ? e.title : basenamePtr(e.path);
    out.printf("[%u] %s | %s | %s | %s\n",
               static_cast<unsigned int>(i + 1U),
               title,
               e.artist[0] != '\0' ? e.artist : "-",
               e.codec,
               e.path);
  }
  return total;
}

uint16_t TrackCatalog::countByPrefix(const char* prefix) const {
  const char* safePrefix = (prefix == nullptr) ? "/" : prefix;
  uint16_t total = 0U;
  for (uint16_t i = 0U; i < count_; ++i) {
    if (startsWithPathPrefix(entries_[i].path, safePrefix)) {
      ++total;
    }
  }
  return total;
}

bool TrackCatalog::isSupportedPath(const char* path, CatalogCodec* outCodec) {
  const CatalogCodec codec = catalogCodecFromPath(path);
  if (outCodec != nullptr) {
    *outCodec = codec;
  }
  return codec != CatalogCodec::kUnknown;
}

int TrackCatalog::compareNatural(const char* lhs, const char* rhs) {
  if (lhs == nullptr && rhs == nullptr) {
    return 0;
  }
  if (lhs == nullptr) {
    return -1;
  }
  if (rhs == nullptr) {
    return 1;
  }

  size_t il = 0U;
  size_t ir = 0U;
  while (lhs[il] != '\0' && rhs[ir] != '\0') {
    size_t usedL = 0U;
    size_t usedR = 0U;
    const int cmp = compareSegment(lhs + il, rhs + ir, &usedL, &usedR);
    if (cmp != 0) {
      return cmp;
    }
    il += usedL;
    ir += usedR;
  }
  if (lhs[il] == '\0' && rhs[ir] == '\0') {
    return 0;
  }
  return (lhs[il] == '\0') ? -1 : 1;
}

void TrackCatalog::sanitizeText(char* text, size_t len) {
  if (text == nullptr || len == 0U) {
    return;
  }
  for (size_t i = 0U; i < len && text[i] != '\0'; ++i) {
    if (text[i] == '\t' || text[i] == '\r' || text[i] == '\n') {
      text[i] = ' ';
    }
  }
  trimLeading(text);
  trimTrailing(text);
}

bool TrackCatalog::copyStr(char* out, size_t outLen, const char* in) {
  if (out == nullptr || outLen == 0U) {
    return false;
  }
  out[0] = '\0';
  if (in == nullptr) {
    return false;
  }
  snprintf(out, outLen, "%s", in);
  return out[0] != '\0';
}

bool TrackCatalog::startsWithPathPrefix(const char* path, const char* prefix) {
  if (path == nullptr || prefix == nullptr) {
    return false;
  }
  if (strcmp(prefix, "/") == 0 || prefix[0] == '\0') {
    return true;
  }
  const size_t prefixLen = strlen(prefix);
  if (strncmp(path, prefix, prefixLen) != 0) {
    return false;
  }
  const char next = path[prefixLen];
  return next == '\0' || next == '/';
}

void TrackCatalog::parseMetadata(fs::FS& storage, TrackEntry* entry, uint32_t timeoutMs) {
  if (entry == nullptr || entry->path[0] == '\0') {
    return;
  }
  fs::File file = storage.open(entry->path, FILE_READ);
  if (!file || file.isDirectory()) {
    return;
  }
  parseId3v2(file, entry, timeoutMs);
  parseId3v1(file, entry);
  sanitizeText(entry->title, sizeof(entry->title));
  sanitizeText(entry->artist, sizeof(entry->artist));
  sanitizeText(entry->album, sizeof(entry->album));
  file.close();
}

void TrackCatalog::parseId3v2(fs::File& file, TrackEntry* entry, uint32_t timeoutMs) {
  if (!file || entry == nullptr) {
    return;
  }
  if (file.seek(0) == false) {
    return;
  }
  uint8_t header[10] = {};
  if (!readBounded(file, header, sizeof(header), timeoutMs)) {
    return;
  }
  if (!(header[0] == 'I' && header[1] == 'D' && header[2] == '3')) {
    return;
  }

  const uint8_t version = header[3];
  const uint32_t tagSize = parseSynchsafe32(&header[6]);
  if (tagSize == 0U || tagSize > 64U * 1024U) {
    return;
  }

  uint32_t consumed = 0U;
  while (consumed + 10U <= tagSize) {
    uint8_t fh[10] = {};
    if (!readBounded(file, fh, sizeof(fh), timeoutMs)) {
      return;
    }
    consumed += 10U;

    if (fh[0] == 0U || fh[1] == 0U || fh[2] == 0U || fh[3] == 0U) {
      return;
    }
    const char fid[5] = {static_cast<char>(fh[0]),
                         static_cast<char>(fh[1]),
                         static_cast<char>(fh[2]),
                         static_cast<char>(fh[3]),
                         '\0'};
    uint32_t frameSize = 0U;
    if (version >= 4U) {
      frameSize = parseSynchsafe32(&fh[4]);
    } else {
      frameSize = parseBigEndian32(&fh[4]);
    }
    if (frameSize == 0U || frameSize > (tagSize - consumed)) {
      return;
    }

    const bool wanted = (strcmp(fid, "TIT2") == 0) ||
                        (strcmp(fid, "TPE1") == 0) ||
                        (strcmp(fid, "TALB") == 0);

    if (!wanted) {
      if (file.seek(file.position() + frameSize) == false) {
        return;
      }
      consumed += frameSize;
      continue;
    }

    char local[96] = {};
    const size_t readLen = (frameSize < (sizeof(local) - 1U)) ? frameSize : (sizeof(local) - 1U);
    if (!readBounded(file, reinterpret_cast<uint8_t*>(local), readLen, timeoutMs)) {
      return;
    }
    local[readLen] = '\0';
    consumed += frameSize;
    if (frameSize > readLen) {
      if (file.seek(file.position() + (frameSize - readLen)) == false) {
        return;
      }
    }

    const char* textStart = local;
    if (readLen > 0U) {
      // encoding byte
      textStart = local + 1;
    }

    if (strcmp(fid, "TIT2") == 0 && entry->title[0] == '\0') {
      copyStr(entry->title, sizeof(entry->title), textStart);
    } else if (strcmp(fid, "TPE1") == 0 && entry->artist[0] == '\0') {
      copyStr(entry->artist, sizeof(entry->artist), textStart);
    } else if (strcmp(fid, "TALB") == 0 && entry->album[0] == '\0') {
      copyStr(entry->album, sizeof(entry->album), textStart);
    }
  }
}

void TrackCatalog::parseId3v1(fs::File& file, TrackEntry* entry) {
  if (!file || entry == nullptr) {
    return;
  }
  const size_t total = file.size();
  if (total < 128U) {
    return;
  }
  if (file.seek(total - 128U) == false) {
    return;
  }
  uint8_t buf[128] = {};
  if (file.read(buf, sizeof(buf)) != static_cast<int>(sizeof(buf))) {
    return;
  }
  if (!(buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G')) {
    return;
  }

  if (entry->title[0] == '\0') {
    char tmp[31] = {};
    memcpy(tmp, &buf[3], 30U);
    sanitizeText(tmp, sizeof(tmp));
    copyStr(entry->title, sizeof(entry->title), tmp);
  }
  if (entry->artist[0] == '\0') {
    char tmp[31] = {};
    memcpy(tmp, &buf[33], 30U);
    sanitizeText(tmp, sizeof(tmp));
    copyStr(entry->artist, sizeof(entry->artist), tmp);
  }
  if (entry->album[0] == '\0') {
    char tmp[31] = {};
    memcpy(tmp, &buf[63], 30U);
    sanitizeText(tmp, sizeof(tmp));
    copyStr(entry->album, sizeof(entry->album), tmp);
  }
}

bool TrackCatalog::addTrackEntry(const TrackEntry& entry) {
  if (count_ >= kMaxTracks) {
    return false;
  }
  entries_[count_++] = entry;
  return true;
}

void TrackCatalog::sortEntries() {
  if (count_ < 2U) {
    return;
  }
  for (uint16_t i = 0U; i < count_ - 1U; ++i) {
    for (uint16_t j = i + 1U; j < count_; ++j) {
      if (compareNatural(entries_[j].path, entries_[i].path) < 0) {
        const TrackEntry tmp = entries_[i];
        entries_[i] = entries_[j];
        entries_[j] = tmp;
      }
    }
  }
}

bool TrackCatalog::scanDirRecursive(fs::FS& storage,
                                    const char* dirPath,
                                    uint8_t depth,
                                    uint8_t maxDepth,
                                    uint32_t metadataTimeoutMs,
                                    CatalogStats* stats) {
  fs::File dir = storage.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return false;
  }
  if (stats != nullptr) {
    ++stats->folders;
  }

  fs::File file = dir.openNextFile();
  while (file) {
    if (count_ >= kMaxTracks) {
      file.close();
      break;
    }

    if (file.isDirectory()) {
      if (depth < maxDepth) {
        String next = String(file.name());
        if (!next.startsWith("/")) {
          next = "/" + next;
        }
        file.close();
        scanDirRecursive(storage, next.c_str(), static_cast<uint8_t>(depth + 1U), maxDepth, metadataTimeoutMs, stats);
      } else {
        file.close();
      }
    } else {
      String path = String(file.name());
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
      CatalogCodec codec = CatalogCodec::kUnknown;
      if (isSupportedPath(path.c_str(), &codec)) {
        TrackEntry entry;
        memset(&entry, 0, sizeof(entry));
        copyStr(entry.path, sizeof(entry.path), path.c_str());
        copyStr(entry.codec, sizeof(entry.codec), catalogCodecLabel(codec));
        entry.sizeBytes = static_cast<uint32_t>(file.size());
        parseMetadata(storage, &entry, metadataTimeoutMs);
        if (!addTrackEntry(entry)) {
          file.close();
          break;
        }
      }
      file.close();
    }

    file = dir.openNextFile();
  }
  dir.close();
  return true;
}
