#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace fx {

// Minimal JSON view interface: implement this adapter with your JSON library of choice.
// The engine core does NOT mandate a specific JSON library.
struct JsonValue {
  virtual ~JsonValue() = default;

  virtual bool isObject() const = 0;
  virtual bool isArray() const = 0;
  virtual bool isString() const = 0;
  virtual bool isNumber() const = 0;
  virtual bool isBool() const = 0;
  virtual bool isNull() const = 0;

  virtual std::string getString(const std::string& def = "") const = 0;
  virtual double getNumber(double def = 0.0) const = 0;
  virtual bool getBool(bool def = false) const = 0;

  virtual const JsonValue* get(const std::string& key) const = 0; // object member
  virtual size_t size() const = 0;                                 // array size
  virtual const JsonValue* at(size_t i) const = 0;                 // array item
};

struct IJsonParser {
  virtual ~IJsonParser() = default;
  virtual const JsonValue* parse(const std::string& jsonText) = 0;
  virtual void free(const JsonValue* root) = 0;
};

// Helper: parse object fields into string map (params/args) in a portable way.
inline std::unordered_map<std::string, std::string> jsonObjectToStringMap(const JsonValue* obj)
{
  // This requires an iterator API which our minimal interface doesn't include.
  // So: keep params "stringly typed" but provide an alternative:
  // - your JSON adapter can fill params directly while parsing.
  return {};
}

} // namespace fx
