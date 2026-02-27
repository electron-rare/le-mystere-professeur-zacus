// Interface générique pour codec audio (I2S)
// Permet d'abstraire ES8388, PCM5102, et une interface générique


#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <Arduino.h>

enum AudioRoute {
    ROUTE_RTC,
    ROUTE_BLUETOOTH,
    ROUTE_NONE
};

class AudioCodec {
public:
    virtual bool init() = 0;
    virtual bool setVolume(uint8_t volume) = 0;
    virtual bool mute(bool state) = 0;
    virtual bool setRoute(AudioRoute route) = 0;
    virtual ~AudioCodec() {}
};

class ES8388Codec : public AudioCodec {
public:
    bool init() override;
    bool setVolume(uint8_t volume) override;
    bool mute(bool state) override;
    bool setRoute(AudioRoute route) override;
};

class PCM5102Codec : public AudioCodec {
public:
    bool init() override;
    bool setVolume(uint8_t volume) override;
    bool mute(bool state) override;
    bool setRoute(AudioRoute route) override;
};

class GenericCodec : public AudioCodec {
public:
    bool init() override { return true; }
    bool setVolume(uint8_t) override { return true; }
    bool mute(bool) override { return true; }
    bool setRoute(AudioRoute) override { return true; }
};

#endif // AUDIO_CODEC_H
