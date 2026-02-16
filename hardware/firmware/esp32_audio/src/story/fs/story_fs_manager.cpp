#include "story_fs_manager.h"

#include <cstring>
#include <LittleFS.h>

namespace {

void copyText(char* out, size_t outLen, const char* text) {
	if (out == nullptr || outLen == 0U) {
		return;
	}
	out[0] = '\0';
	if (text == nullptr || text[0] == '\0') {
		return;
	}
	snprintf(out, outLen, "%s", text);
}

}  // namespace

StoryFsManager::StoryFsManager(const char* story_root) {
	copyText(storyRoot_, sizeof(storyRoot_), story_root != nullptr ? story_root : "/story");
}

bool StoryFsManager::init() {
	initialized_ = ensureStoryDirs();
	return initialized_;
}

void StoryFsManager::cleanup() {
	resetScenarioData();
	initialized_ = false;
}

bool StoryFsManager::loadScenario(const char* scenario_id) {
	(void)scenario_id;
	resetScenarioData();
	return false;
}

bool StoryFsManager::listScenarios(StoryScenarioInfo* out, size_t maxCount, size_t* outCount) const {
	if (outCount != nullptr) {
		*outCount = 0U;
	}
	if (out == nullptr || maxCount == 0U) {
		return false;
	}

	fs::File root = LittleFS.open(storyRoot_);
	if (!root || !root.isDirectory()) {
		return false;
	}

	size_t count = 0U;
	fs::File entry = root.openNextFile();
	while (entry && count < maxCount) {
		if (entry.isDirectory()) {
			char path[96] = {};
			snprintf(path, sizeof(path), "%s/%s/scenario.json", storyRoot_, entry.name());
			StoryScenarioInfo info = {};
			if (loadScenarioInfoFromFile(path, &info)) {
				out[count++] = info;
			}
		}
		entry = root.openNextFile();
	}

	if (outCount != nullptr) {
		*outCount = count;
	}
	return true;
}

bool StoryFsManager::fsInfo(uint32_t* totalBytes, uint32_t* usedBytes, uint16_t* scenarioCount) const {
	if (totalBytes != nullptr) {
		*totalBytes = LittleFS.totalBytes();
	}
	if (usedBytes != nullptr) {
		*usedBytes = LittleFS.usedBytes();
	}
	if (scenarioCount != nullptr) {
		StoryScenarioInfo infos[16];
		size_t count = 0U;
		if (listScenarios(infos, 16U, &count)) {
			*scenarioCount = static_cast<uint16_t>(count);
		} else {
			*scenarioCount = 0U;
		}
	}
	return true;
}

const StepDef* StoryFsManager::getStep(const char* step_id) const {
	(void)step_id;
	return nullptr;
}

const ResourceBindings* StoryFsManager::getResources(const char* step_id) const {
	(void)step_id;
	return nullptr;
}

const AppConfig* StoryFsManager::getAppConfig(const char* app_id) {
	(void)app_id;
	return nullptr;
}

bool StoryFsManager::validateChecksum(const char* resource_type, const char* resource_id) {
	(void)resource_type;
	(void)resource_id;
	return false;
}

void StoryFsManager::listResources(const char* resource_type) {
	(void)resource_type;
}

const ScenarioDef* StoryFsManager::scenario() const {
	return initialized_ ? &scenario_ : nullptr;
}

bool StoryFsManager::loadJson(const char* path, DynamicJsonDocument& doc) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}
	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		return false;
	}
	const DeserializationError err = deserializeJson(doc, file);
	file.close();
	return !err;
}

bool StoryFsManager::verifyChecksum(const char* resource_path) {
	(void)resource_path;
	return true;
}

bool StoryFsManager::ensureStoryDirs() {
	if (!LittleFS.exists(storyRoot_)) {
		return LittleFS.mkdir(storyRoot_);
	}
	return true;
}

bool StoryFsManager::loadScenarioInfoFromFile(const char* path, StoryScenarioInfo* out) const {
	if (path == nullptr || out == nullptr) {
		return false;
	}
	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		return false;
	}
	const bool ok = parseScenarioJson(file, out);
	file.close();
	return ok;
}

bool StoryFsManager::parseScenarioJson(fs::File& file, StoryScenarioInfo* out) const {
	if (!file || out == nullptr) {
		return false;
	}
	StaticJsonDocument<512> doc;
	const DeserializationError err = deserializeJson(doc, file);
	if (err) {
		return false;
	}
	const char* id = doc["id"] | "";
	if (id[0] == '\0') {
		return false;
	}
	copyText(out->id, sizeof(out->id), id);
	out->version = static_cast<uint16_t>(doc["version"] | 0U);
	out->estimatedDurationS = static_cast<uint32_t>(doc["estimated_duration_s"] | 0U);
	return true;
}

const char* StoryFsManager::buildResourcePath(const char* resource_type,
																							const char* resource_id,
																							const char* extension,
																							char* out,
																							size_t out_len) const {
	if (out == nullptr || out_len == 0U) {
		return nullptr;
	}
	if (resource_type == nullptr || resource_id == nullptr) {
		out[0] = '\0';
		return out;
	}
	const char* ext = (extension != nullptr) ? extension : "";
	snprintf(out, out_len, "%s/%s/%s%s", storyRoot_, resource_type, resource_id, ext);
	return out;
}

const char* StoryFsManager::storeString(const char* value) {
	if (value == nullptr || value[0] == '\0') {
		return nullptr;
	}
	const size_t len = strlen(value) + 1U;
	if (stringOffset_ + len >= sizeof(stringPool_)) {
		return nullptr;
	}
	char* dst = &stringPool_[stringOffset_];
	memcpy(dst, value, len);
	stringOffset_ += len;
	return dst;
}

void StoryFsManager::resetScenarioData() {
	scenario_ = {};
	stepCount_ = 0U;
	stringOffset_ = 0U;
	for (auto& cache : appConfigs_) {
		cache.valid = false;
		cache.params = JsonObject();
	}
}
#include "story_fs_manager.h"

#include <LittleFS.h>
#include <mbedtls/sha256.h>

#include <cstring>

namespace {

constexpr size_t kSha256HexLen = 64U;

bool textEquals(const char* lhs, const char* rhs) {
	if (lhs == nullptr || rhs == nullptr) {
		return false;
	}
	return strcmp(lhs, rhs) == 0;
}

StoryTransitionTrigger parseTrigger(const char* value) {
	if (value == nullptr) {
		return StoryTransitionTrigger::kOnEvent;
	}
	if (strcmp(value, "after_ms") == 0) {
		return StoryTransitionTrigger::kAfterMs;
	}
	if (strcmp(value, "immediate") == 0) {
		return StoryTransitionTrigger::kImmediate;
	}
	return StoryTransitionTrigger::kOnEvent;
}

StoryEventType parseEventType(const char* value) {
	if (value == nullptr) {
		return StoryEventType::kNone;
	}
	if (strcmp(value, "unlock") == 0) {
		return StoryEventType::kUnlock;
	}
	if (strcmp(value, "audio_done") == 0) {
		return StoryEventType::kAudioDone;
	}
	if (strcmp(value, "timer") == 0) {
		return StoryEventType::kTimer;
	}
	if (strcmp(value, "serial") == 0) {
		return StoryEventType::kSerial;
	}
	if (strcmp(value, "action") == 0) {
		return StoryEventType::kAction;
	}
	return StoryEventType::kNone;
}

bool hexCharToNibble(char value, uint8_t* out) {
	if (out == nullptr) {
		return false;
	}
	if (value >= '0' && value <= '9') {
		*out = static_cast<uint8_t>(value - '0');
		return true;
	}
	if (value >= 'a' && value <= 'f') {
		*out = static_cast<uint8_t>(10 + value - 'a');
		return true;
	}
	if (value >= 'A' && value <= 'F') {
		*out = static_cast<uint8_t>(10 + value - 'A');
		return true;
	}
	return false;
}

bool trimHexString(char* text) {
	if (text == nullptr) {
		return false;
	}
	size_t len = strlen(text);
	while (len > 0U && (text[len - 1U] == '\n' || text[len - 1U] == '\r' || text[len - 1U] == ' ')) {
		text[len - 1U] = '\0';
		--len;
	}
	return len >= kSha256HexLen;
}

bool computeFileSha256(fs::File& file, char* outHex, size_t outLen) {
	if (outHex == nullptr || outLen <= kSha256HexLen) {
		return false;
	}

	mbedtls_sha256_context ctx;
	mbedtls_sha256_init(&ctx);
	if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) {
		mbedtls_sha256_free(&ctx);
		return false;
	}

	uint8_t buffer[256];
	while (file.available()) {
		const size_t read = file.read(buffer, sizeof(buffer));
		if (read == 0U) {
			break;
		}
		if (mbedtls_sha256_update_ret(&ctx, buffer, read) != 0) {
			mbedtls_sha256_free(&ctx);
			return false;
		}
	}

	uint8_t hash[32] = {};
	if (mbedtls_sha256_finish_ret(&ctx, hash) != 0) {
		mbedtls_sha256_free(&ctx);
		return false;
	}
	mbedtls_sha256_free(&ctx);

	static const char* kHex = "0123456789abcdef";
	for (size_t i = 0; i < sizeof(hash); ++i) {
		outHex[i * 2U] = kHex[(hash[i] >> 4U) & 0x0FU];
		outHex[i * 2U + 1U] = kHex[hash[i] & 0x0FU];
	}
	outHex[kSha256HexLen] = '\0';
	return true;
}

}  // namespace

StoryFsManager::StoryFsManager(const char* story_root) {
	if (story_root != nullptr) {
		snprintf(storyRoot_, sizeof(storyRoot_), "%s", story_root);
	} else {
		snprintf(storyRoot_, sizeof(storyRoot_), "/story");
	}
}

bool StoryFsManager::init() {
	resetScenarioData();
	if (!LittleFS.begin(false)) {
		Serial.println("[STORY_FS] LittleFS not mounted.");
		return false;
	}
	if (!ensureStoryDirs()) {
		Serial.println("[STORY_FS] story dirs missing.");
		return false;
	}

	initialized_ = true;
	return true;
}

void StoryFsManager::cleanup() {
	resetScenarioData();
	initialized_ = false;
}

bool StoryFsManager::loadScenario(const char* scenario_id) {
	if (!initialized_) {
		if (!init()) {
			return false;
		}
	}

	if (scenario_id == nullptr || scenario_id[0] == '\0') {
		Serial.println("[STORY_FS] loadScenario missing id.");
		return false;
	}

	char path[128] = {};
	buildResourcePath("scenarios", scenario_id, ".json", path, sizeof(path));
	if (!LittleFS.exists(path)) {
		Serial.printf("[STORY_FS] scenario missing: %s\n", path);
		return false;
	}
	if (!verifyChecksum(path)) {
		Serial.printf("[STORY_FS] checksum failed: %s\n", path);
		return false;
	}

	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		Serial.printf("[STORY_FS] open failed: %s\n", path);
		return false;
	}

	DynamicJsonDocument doc(8192);
	DeserializationError err = deserializeJson(doc, file);
	file.close();
	if (err) {
		Serial.printf("[STORY_FS] parse error: %s\n", err.c_str());
		return false;
	}

	resetScenarioData();
	JsonObject root = doc.as<JsonObject>();
	const char* id = root["id"] | "";
	scenario_.id = storeString(id);
	scenario_.version = static_cast<uint16_t>(root["version"] | 0);
	scenario_.initialStepId = storeString(root["initial_step"] | "");

	JsonArray steps = root["steps"].as<JsonArray>();
	if (steps.isNull()) {
		Serial.println("[STORY_FS] scenario missing steps array.");
		return false;
	}

	JsonArray bindings = root["app_bindings"].as<JsonArray>();
	if (!bindings.isNull()) {
		for (JsonVariant bindingVal : bindings) {
			const char* appId = bindingVal["id"] | "";
			if (appId[0] != '\0' && !validateChecksum("apps", appId)) {
				Serial.printf("[STORY_FS] app checksum mismatch: %s\n", appId);
				return false;
			}
		}
	}

	for (JsonVariant stepVal : steps) {
		if (stepCount_ >= static_cast<uint8_t>(sizeof(steps_) / sizeof(steps_[0]))) {
			Serial.println("[STORY_FS] too many steps.");
			break;
		}
		JsonObject stepObj = stepVal.as<JsonObject>();
		if (stepObj.isNull()) {
			continue;
		}

		const char* screenId = stepObj["screen_scene_id"] | "";
		const char* audioId = stepObj["audio_pack_id"] | "";
		if (screenId[0] != '\0' && !validateChecksum("screens", screenId)) {
			Serial.printf("[STORY_FS] screen checksum mismatch: %s\n", screenId);
			return false;
		}
		if (audioId[0] != '\0' && !validateChecksum("audio", audioId)) {
			Serial.printf("[STORY_FS] audio checksum mismatch: %s\n", audioId);
			return false;
		}

		StepRuntime& runtime = stepRuntime_[stepCount_];
		StepDef& step = steps_[stepCount_];
		step.id = storeString(stepObj["step_id"] | "");
		step.resources.screenSceneId = storeString(stepObj["screen_scene_id"] | "");
		step.resources.audioPackId = storeString(stepObj["audio_pack_id"] | "");

		JsonArray actions = stepObj["actions"].as<JsonArray>();
		uint8_t actionCount = 0U;
		if (!actions.isNull()) {
			for (JsonVariant actionVal : actions) {
				if (actionCount >= static_cast<uint8_t>(sizeof(runtime.actionIds) / sizeof(runtime.actionIds[0]))) {
					break;
				}
				const char* actionId = actionVal.as<const char*>();
				if (actionId != nullptr && actionId[0] != '\0' && !validateChecksum("actions", actionId)) {
					Serial.printf("[STORY_FS] action checksum mismatch: %s\n", actionId);
					return false;
				}
				runtime.actionIds[actionCount++] = storeString(actionId);
			}
		}
		step.resources.actionIds = runtime.actionIds;
		step.resources.actionCount = actionCount;

		JsonArray apps = stepObj["apps"].as<JsonArray>();
		uint8_t appCount = 0U;
		if (!apps.isNull()) {
			for (JsonVariant appVal : apps) {
				if (appCount >= static_cast<uint8_t>(sizeof(runtime.appIds) / sizeof(runtime.appIds[0]))) {
					break;
				}
				const char* appId = appVal.as<const char*>();
				runtime.appIds[appCount++] = storeString(appId);
			}
		}
		step.resources.appIds = runtime.appIds;
		step.resources.appCount = appCount;

		step.mp3GateOpen = static_cast<bool>(stepObj["mp3_gate_open"] | false);

		JsonArray transitions = stepObj["transitions"].as<JsonArray>();
		uint8_t transitionCount = 0U;
		if (!transitions.isNull()) {
			for (JsonVariant trVal : transitions) {
				if (transitionCount >= static_cast<uint8_t>(sizeof(runtime.transitions) / sizeof(runtime.transitions[0]))) {
					break;
				}
				JsonObject trObj = trVal.as<JsonObject>();
				if (trObj.isNull()) {
					continue;
				}
				TransitionDef& tr = runtime.transitions[transitionCount++];
				tr.id = storeString(trObj["id"] | "");
				tr.trigger = parseTrigger(trObj["trigger"] | "");
				tr.eventType = parseEventType(trObj["event_type"] | "none");
				tr.eventName = storeString(trObj["event_name"] | "");
				tr.afterMs = static_cast<uint32_t>(trObj["after_ms"] | 0);
				tr.targetStepId = storeString(trObj["target_step_id"] | "");
				tr.priority = static_cast<uint8_t>(trObj["priority"] | 0);
			}
		}
		step.transitions = runtime.transitions;
		step.transitionCount = transitionCount;

		++stepCount_;
	}

	scenario_.steps = steps_;
	scenario_.stepCount = stepCount_;
	Serial.printf("[STORY_FS] scenario loaded id=%s steps=%u\n", scenario_.id, stepCount_);
	return true;
}

bool StoryFsManager::listScenarios(StoryScenarioInfo* out, size_t maxCount, size_t* outCount) const {
	if (outCount != nullptr) {
		*outCount = 0U;
	}
	if (!initialized_) {
		return false;
	}
	if (out == nullptr || maxCount == 0U) {
		return false;
	}
	char dirPath[96] = {};
	buildResourcePath("scenarios", "", "", dirPath, sizeof(dirPath));
	fs::File dir = LittleFS.open(dirPath);
	if (!dir || !dir.isDirectory()) {
		return false;
	}

	size_t count = 0U;
	fs::File entry = dir.openNextFile();
	while (entry && count < maxCount) {
		if (!entry.isDirectory()) {
			StoryScenarioInfo info = {};
			if (parseScenarioJson(entry, &info)) {
				out[count++] = info;
			}
		}
		entry.close();
		entry = dir.openNextFile();
	}
	if (outCount != nullptr) {
		*outCount = count;
	}
	return true;
}

bool StoryFsManager::fsInfo(uint32_t* totalBytes, uint32_t* usedBytes, uint16_t* scenarioCount) const {
	if (!initialized_) {
		return false;
	}
	if (totalBytes != nullptr) {
		*totalBytes = static_cast<uint32_t>(LittleFS.totalBytes());
	}
	if (usedBytes != nullptr) {
		*usedBytes = static_cast<uint32_t>(LittleFS.usedBytes());
	}
	if (scenarioCount != nullptr) {
		*scenarioCount = 0U;
		char dirPath[96] = {};
		buildResourcePath("scenarios", "", "", dirPath, sizeof(dirPath));
		fs::File dir = LittleFS.open(dirPath);
		if (dir && dir.isDirectory()) {
			fs::File entry = dir.openNextFile();
			while (entry) {
				if (!entry.isDirectory()) {
					++(*scenarioCount);
				}
				entry.close();
				entry = dir.openNextFile();
			}
		}
	}
	return true;
}

const StepDef* StoryFsManager::getStep(const char* step_id) const {
	if (step_id == nullptr || step_id[0] == '\0') {
		return nullptr;
	}
	for (uint8_t i = 0U; i < stepCount_; ++i) {
		const StepDef& step = steps_[i];
		if (step.id != nullptr && strcmp(step.id, step_id) == 0) {
			return &step;
		}
	}
	return nullptr;
}

const ResourceBindings* StoryFsManager::getResources(const char* step_id) const {
	const StepDef* step = getStep(step_id);
	return (step != nullptr) ? &step->resources : nullptr;
}

const AppConfig* StoryFsManager::getAppConfig(const char* app_id) {
	if (app_id == nullptr || app_id[0] == '\0') {
		return nullptr;
	}
	for (auto& cache : appConfigs_) {
		if (cache.valid && textEquals(cache.appId, app_id)) {
			return &cache.appConfig;
		}
	}

	AppConfigCache* slot = nullptr;
	for (auto& cache : appConfigs_) {
		if (!cache.valid) {
			slot = &cache;
			break;
		}
	}
	if (slot == nullptr) {
		slot = &appConfigs_[0];
	}

	char path[128] = {};
	buildResourcePath("apps", app_id, ".json", path, sizeof(path));
	if (!LittleFS.exists(path)) {
		Serial.printf("[STORY_FS] app missing: %s\n", path);
		return nullptr;
	}
	if (!verifyChecksum(path)) {
		Serial.printf("[STORY_FS] app checksum failed: %s\n", path);
		return nullptr;
	}

	slot->doc.clear();
	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		return nullptr;
	}
	DeserializationError err = deserializeJson(slot->doc, file);
	file.close();
	if (err) {
		Serial.printf("[STORY_FS] app parse error: %s\n", err.c_str());
		return nullptr;
	}

	const char* type = slot->doc["app"] | "";
	snprintf(slot->appId, sizeof(slot->appId), "%s", app_id);
	snprintf(slot->appType, sizeof(slot->appType), "%s", type);
	slot->params = slot->doc["config"].as<JsonObject>();
	if (slot->params.isNull()) {
		slot->params = slot->doc["params"].as<JsonObject>();
	}
	slot->appConfig = {};
	snprintf(slot->appConfig.appId, sizeof(slot->appConfig.appId), "%s", slot->appId);
	snprintf(slot->appConfig.appType, sizeof(slot->appConfig.appType), "%s", slot->appType);
	slot->appConfig.params = slot->params;
	slot->valid = true;
	return &slot->appConfig;
}

bool StoryFsManager::validateChecksum(const char* resource_type, const char* resource_id) {
	if (resource_type == nullptr || resource_id == nullptr || resource_id[0] == '\0') {
		return false;
	}
	char path[128] = {};
	buildResourcePath(resource_type, resource_id, ".json", path, sizeof(path));
	return verifyChecksum(path);
}

void StoryFsManager::listResources(const char* resource_type) {
	if (resource_type == nullptr || resource_type[0] == '\0') {
		Serial.println("[STORY_FS] list missing resource type");
		return;
	}
	char dirPath[96] = {};
	buildResourcePath(resource_type, "", "", dirPath, sizeof(dirPath));
	fs::File dir = LittleFS.open(dirPath);
	if (!dir || !dir.isDirectory()) {
		Serial.printf("[STORY_FS] list failed: %s\n", dirPath);
		return;
	}
	fs::File entry = dir.openNextFile();
	while (entry) {
		if (!entry.isDirectory()) {
			const char* name = entry.name();
			if (name != nullptr) {
				const char* base = strrchr(name, '/');
				base = (base != nullptr) ? (base + 1) : name;
				const char* ext = strrchr(base, '.');
				if (ext == nullptr || strcmp(ext, ".json") != 0) {
					entry.close();
					entry = dir.openNextFile();
					continue;
				}
				char id[80] = {};
				snprintf(id, sizeof(id), "%s", base);
				char* dot = strrchr(id, '.');
				if (dot != nullptr) {
					*dot = '\0';
				}
				Serial.printf("%s\n", id);
			}
		}
		entry.close();
		entry = dir.openNextFile();
	}
}

const ScenarioDef* StoryFsManager::scenario() const {
	return (scenario_.id != nullptr && scenario_.id[0] != '\0') ? &scenario_ : nullptr;
}

bool StoryFsManager::loadJson(const char* path, DynamicJsonDocument& doc) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}
	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		return false;
	}
	DeserializationError err = deserializeJson(doc, file);
	file.close();
	if (err) {
		Serial.printf("[STORY_FS] JSON error %s: %s\n", path, err.c_str());
		return false;
	}
	return true;
}

bool StoryFsManager::verifyChecksum(const char* resource_path) {
	if (resource_path == nullptr || resource_path[0] == '\0') {
		return false;
	}
	char checksumPath[160] = {};
	snprintf(checksumPath, sizeof(checksumPath), "%s.sha256", resource_path);
	if (!LittleFS.exists(resource_path) || !LittleFS.exists(checksumPath)) {
		Serial.printf("[STORY_FS] checksum missing for %s\n", resource_path);
		return false;
	}

	fs::File dataFile = LittleFS.open(resource_path, "r");
	if (!dataFile) {
		return false;
	}
	char computed[kSha256HexLen + 1U] = {};
	if (!computeFileSha256(dataFile, computed, sizeof(computed))) {
		dataFile.close();
		return false;
	}
	dataFile.close();

	fs::File checksumFile = LittleFS.open(checksumPath, "r");
	if (!checksumFile) {
		return false;
	}
	char expected[kSha256HexLen + 8U] = {};
	size_t read = checksumFile.readBytes(expected, sizeof(expected) - 1U);
	checksumFile.close();
	expected[read] = '\0';
	if (!trimHexString(expected)) {
		return false;
	}
	expected[kSha256HexLen] = '\0';

	if (strcmp(computed, expected) != 0) {
		Serial.printf("[STORY_FS] checksum mismatch %s\n", resource_path);
		return false;
	}
	return true;
}

bool StoryFsManager::ensureStoryDirs() {
	const char* subdirs[] = {"", "scenarios", "apps", "screens", "audio", "actions"};
	char fullPath[96] = {};
	for (const char* sub : subdirs) {
		if (sub == nullptr) {
			continue;
		}
		if (sub[0] == '\0') {
			snprintf(fullPath, sizeof(fullPath), "%s", storyRoot_);
		} else {
			snprintf(fullPath, sizeof(fullPath), "%s/%s", storyRoot_, sub);
		}
		if (!LittleFS.exists(fullPath)) {
			if (!LittleFS.mkdir(fullPath)) {
				Serial.printf("[STORY_FS] mkdir failed: %s\n", fullPath);
				return false;
			}
		}
	}
	return true;
}

bool StoryFsManager::loadScenarioInfoFromFile(const char* path, StoryScenarioInfo* out) const {
	if (path == nullptr || out == nullptr) {
		return false;
	}
	fs::File file = LittleFS.open(path, "r");
	if (!file) {
		return false;
	}
	const bool ok = parseScenarioJson(file, out);
	file.close();
	return ok;
}

bool StoryFsManager::parseScenarioJson(fs::File& file, StoryScenarioInfo* out) const {
	if (!file || out == nullptr) {
		return false;
	}
	StaticJsonDocument<512> doc;
	DeserializationError err = deserializeJson(doc, file);
	if (err) {
		return false;
	}
	const char* id = doc["id"] | "";
	snprintf(out->id, sizeof(out->id), "%s", id);
	out->version = static_cast<uint16_t>(doc["version"] | 0);
	out->estimatedDurationS = static_cast<uint32_t>(doc["estimated_duration_s"] | 0);
	return true;
}

const char* StoryFsManager::buildResourcePath(const char* resource_type,
																							const char* resource_id,
																							const char* extension,
																							char* out,
																							size_t out_len) const {
	if (out == nullptr || out_len == 0U) {
		return nullptr;
	}
	const char* type = (resource_type != nullptr) ? resource_type : "";
	const char* id = (resource_id != nullptr) ? resource_id : "";
	const char* ext = (extension != nullptr) ? extension : "";
	if (id[0] == '\0') {
		snprintf(out, out_len, "%s/%s", storyRoot_, type);
	} else {
		snprintf(out, out_len, "%s/%s/%s%s", storyRoot_, type, id, ext);
	}
	return out;
}

const char* StoryFsManager::storeString(const char* value) {
	if (value == nullptr) {
		return "";
	}
	size_t len = strlen(value);
	if (len == 0U) {
		return "";
	}
	if (stringOffset_ + len + 1U >= sizeof(stringPool_)) {
		return "";
	}
	char* target = &stringPool_[stringOffset_];
	memcpy(target, value, len);
	target[len] = '\0';
	stringOffset_ += len + 1U;
	return target;
}

void StoryFsManager::resetScenarioData() {
	scenario_ = {};
	stepCount_ = 0U;
	stringOffset_ = 0U;
	memset(stringPool_, 0, sizeof(stringPool_));
	for (auto& cache : appConfigs_) {
		cache.valid = false;
		cache.doc.clear();
		cache.params = JsonObject();
		cache.appConfig = {};
		memset(cache.appId, 0, sizeof(cache.appId));
		memset(cache.appType, 0, sizeof(cache.appType));
	}
	for (auto& step : steps_) {
		step = {};
	}
	for (auto& runtime : stepRuntime_) {
		runtime = {};
	}
}
