# Fiche technique AudioManager

## Interface
- Méthodes principales : init(), play(), stop(), setVolume(), getStatus()
- Gestion des flux audio (lecture, enregistrement)

## Flux de données
- Entrée : fichiers audio, flux PCM
- Sortie : DAC, I2S, logs

## Scénarios d’utilisation
- Lecture de fichier audio
- Contrôle du volume
- Gestion des erreurs

## Exemple d’intégration
```cpp
AudioManager audio;
audio.init();
audio.play("test.wav");
```
