// storage_manager.h - Interface gestion LittleFS
#pragma once

class StorageManager {
 public:
  void begin();
  bool exists(const char* path);
};
