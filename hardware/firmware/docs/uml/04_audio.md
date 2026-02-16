# UML Audio Subsystem

Audio uses ES8388 + I2S, with dual channels (base + overlay).

## Core components

```
AudioService
  - base channel (MP3 or FX)
  - overlay channel (short FX)

AsyncAudioService
  + startFile(fs, path, gain, timeout)
  + stop
  + update

FmRadioScanFx
  + startSweep / startNoise / startBeep
  + stop
  + update

Mp3Player
  + begin(fs)
  + play/pause/stop
  + next/prev
  + setVolume

I2sJinglePlayer
  + playRTTTL
  + stop
  + update

CodecES8388Driver
  + begin
  + setVolume
  + mute/unmute
```

## Notes

- Base channel handles long audio (story tracks, radio).
- Overlay channel handles short FX and does not block base.
- Timeouts are enforced to avoid stuck audio.
