---
# Zacus Firmware – Fonts TTF

---

## 📝 Description

Ce dossier contient les polices TTF nécessaires à la génération des polices LVGL pour l’UI Zacus.

---

## 📦 Fichiers requis

- `Inter-Regular.ttf`
- `Orbitron-Bold.ttf`
- `IBMPlexMono-Regular.ttf`
- `PressStart2P-Regular.ttf` (optionnel si `UI_FONT_PIXEL_ENABLE=1`)

Ces fichiers ne sont pas versionnés dans le dépôt pour éviter d’inclure des binaires upstream.

---

## 🚀 Installation & usage

Placer les fichiers TTF dans ce dossier, puis lancer le script :
```sh
tools/fonts/scripts/generate_lvgl_fonts.sh
```

---

## 🤝 Contribuer

Merci de ne pas ajouter de polices propriétaires sans licence libre.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
