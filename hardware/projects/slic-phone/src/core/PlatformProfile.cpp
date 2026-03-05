#include "core/PlatformProfile.h"

BoardProfile detectBoardProfile() {
#if defined(BOARD_PROFILE_ESP32_S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    return BoardProfile::ESP32_S3;
#else
    return BoardProfile::ESP32_A252;
#endif
}

FeatureMatrix getFeatureMatrix(BoardProfile profile) {
    switch (profile) {
        case BoardProfile::ESP32_A252:
            return FeatureMatrix{
                .has_full_duplex_i2s = true,
            };
        case BoardProfile::ESP32_S3:
            return FeatureMatrix{
                .has_full_duplex_i2s = false,
            };
        default:
            return FeatureMatrix{
                .has_full_duplex_i2s = false,
            };
    }
}

const char* boardProfileToString(BoardProfile profile) {
    switch (profile) {
        case BoardProfile::ESP32_A252:
            return "ESP32_A252";
        case BoardProfile::ESP32_S3:
            return "ESP32_S3";
        default:
            return "UNKNOWN";
    }
}
