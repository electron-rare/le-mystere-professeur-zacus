## Plan : Roadmap hardware/firmware priorisée par plateforme

### 1. ESP32
- **Build/Flash** : Scripts et gates déjà factorisés, structure conforme. Priorité : maintenir la reproductibilité, surveiller les évolutions PlatformIO.
- **Tests/Smoke** : Vérifier la couverture des tests hardware (smoke, reboot, panic). Ajouter des tests de drivers spécifiques si besoin (UART, I2C, SPI, GPIO).
- **Drivers** : S’assurer que tous les capteurs/actuateurs nécessaires sont intégrés et documentés. Priorité aux drivers critiques (communication, sécurité, alimentation).
- **CI/CD** : Maintenir la compatibilité avec la CI, artefacts reproductibles, logs clairs.

### 2. ESP8266
- **Build/Flash** : Scripts harmonisés, mais attention à la compatibilité avec les nouveaux firmwares. Priorité : stabilité du flash et du lien SoftwareSerial.
- **Tests/Smoke** : Ajouter ou renforcer les tests de communication avec l’ESP32 (SoftwareSerial, UI link).
- **Drivers** : Vérifier la présence de tous les drivers nécessaires (afficheur OLED, relais, etc.).
- **Logs** : S’assurer que les logs d’erreur sont bien capturés et exploitables.

### 3. RP2040 (TFT/OLED)
- **Build/Flash** : Vérifier la reproductibilité des builds pour les variantes ILI9488/ILI9486.
- **Tests/Smoke** : Ajouter des tests de rendu écran, de réactivité UI, et de communication avec l’ESP32/ESP8266.
- **Drivers** : Priorité à la stabilité des drivers d’affichage et à la gestion mémoire.
- **Documentation** : Compléter la doc sur le wiring, les dépendances, et les limitations connues.

### 4. Codex/Auto-fix
- **Prompts** : Centraliser et documenter tous les prompts dans codex_prompts/.
- **Intégration** : S’assurer que l’auto-fix fonctionne sur toutes les plateformes, et que les logs/artefacts sont bien générés.
- **Tests** : Ajouter des scénarios de test auto-fix pour chaque plateforme.

### 5. Commun (logs, artefacts, onboarding)
- **Logs/Artefacts** : Maintenir la centralisation, la rotation, et la clarté des logs/artefacts pour chaque plateforme.
- **Onboarding** : Adapter les instructions pour chaque cible (esp32, esp8266, rp2040).
- **CI** : Vérifier que chaque plateforme est bien couverte par la CI (build, smoke, artefacts, logs).

---

**Décisions**
- Priorité à la robustesse ESP32 (build, drivers, tests), puis ESP8266 (communication, logs), puis RP2040 (affichage, tests UI).
- Harmonisation et documentation continue pour chaque plateforme.
- Audit et automatisation réguliers pour garantir la conformité AGENTS.md.

Souhaitez-vous démarrer par l’audit/optimisation ESP32, ESP8266, ou RP2040 ?
