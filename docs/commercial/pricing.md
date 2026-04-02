# Grille Tarifaire — Le Mystère du Professeur Zacus

*Tarifs HT. TVA 20% applicable selon statut client.*
*Mis à jour : 2026-04-02*

---

## Les 3 Tiers

### Tier 1 — Animation

> **Le Professeur vient chez vous.** Clé en main : installation, animation, débrief.

| Formule | Prix HT | Détail |
|---------|---------|--------|
| Demi-journée (2–3h session) | **800 €** | 1 session de jeu, 4–15 joueurs |
| Journée complète (2 sessions) | **1 500 €** | 2 sessions back-to-back (matin + après-midi) |
| Soirée d'entreprise (1 session + cocktail) | **1 200 €** | Session de jeu + 30 min de débrief convivial |

**Inclus :**
- Déplacement dans un rayon de 50 km (Île-de-France base)
- Installation et rangement du kit (non compté dans la durée payante)
- Animation complète par un formateur certifié
- Diplôme numérique PDF pour chaque joueur
- Compte-rendu statistique post-session (temps, score, indices utilisés)
- Adaptation TECH / NON_TECH automatique (profiling IA)

**Frais additionnels :**
- Déplacement > 50 km : +0,45 €/km aller-retour + hébergement si nécessaire
- 3e session dans la journée : +500 €
- Week-end / jours fériés : +20 %
- Remise corporate (>3 sessions/an) : -10 %

---

### Tier 2 — Location

> **Le kit chez vous, le week-end.** Pour associations, anniversaires, séminaires résidentiels.

| Formule | Prix HT | Durée |
|---------|---------|-------|
| Week-end | **400 €** | Vendredi 18h → Lundi 10h |
| Semaine | **650 €** | 7 jours |

**Inclus :**
- Kit V3 complet (3 valises, 7 puzzles, hub, ambiance)
- Guide de déploiement 15 minutes (papier + vidéo QR)
- Support téléphonique pendant la location (heures ouvrées)
- Formation vidéo game master (30 min, lien envoyé à J-3)
- 3 scénarios chargés : `zacus_v3_complete` + 2 variantes de durée (30 min et 90 min)

**Caution :** 1 000 € (chèque ou CB pré-autorisé, restitué sans frais si kit rendu intact)

**Frais additionnels :**
- Livraison/récupération > 50 km : +0,45 €/km A/R
- Extension location : +60 €/jour supplémentaire
- Remplacement pièce endommagée : coût pièce + 30 € de gestion

**Non inclus :** Animation humaine (prévoir une personne formée via la vidéo game master)

---

### Tier 3 — Kit (Achat définitif)

> **Votre propre Professeur Zacus.** Pour escape rooms professionnelles, musées, centres de loisirs.

| Référence | Prix HT | Contenu |
|-----------|---------|---------|
| **ZACUS-KIT-STD** | **3 500 €** | Kit Standard |
| **ZACUS-KIT-PRO** | **4 500 €** | Kit Pro (+ support premium 1 an) |
| **ZACUS-KIT-ULTIMATE** | **5 000 €** | Kit Ultimate (+ 5 scénarios + formation 4h) |

#### Kit Standard — 3 500 € HT

- 3 valises déployées et testées (7 puzzles + hub + ambiance)
- Firmware open source — licence MIT
- 3 scénarios YAML inclus (`zacus_v3_complete` + 2 variantes)
- 1h de formation à distance (visio, prise en main complète)
- Accès aux mises à jour firmware pendant **1 an** (OTA ou USB-C)
- Documentation complète : DEPLOYMENT_RUNBOOK.md, code source commenté
- Pool TTS pré-généré (>200 phrases, Piper TTS + XTTS-v2)

#### Kit Pro — 4 500 € HT

Tout le Standard +
- Support téléphonique prioritaire pendant **1 an** (réponse < 4h en semaine)
- 2 scénarios supplémentaires au choix (sur devis ou catalogue)
- Session de formation présentielle 2h (ou 4h à distance)
- Kit de pièces de rechange : 2× ESP32-S3 dev kit, 10× NTAG213, 2× SG90

#### Kit Ultimate — 5 000 € HT

Tout le Pro +
- 5 scénarios YAML personnalisables
- Formation approfondie 4h présentielle (game design + firmware de base)
- Branding personnalisé : nom du professeur, voix personnalisée (enregistrement 20 min requis)
- 1 session d'animation offerte pour inauguration

---

## Options et add-ons

| Référence | Prix HT | Détail |
|-----------|---------|--------|
| **ZACUS-SCENARIO** | 200 € | Scénario supplémentaire (YAML livré, compilé, testé) |
| **ZACUS-SUPPORT-AN** | 500 €/an | Support premium annuel (renouvellement après 1 an inclus) |
| **ZACUS-VOICE-CLONE** | 300 € | Clone vocal personnalisé XTTS-v2 (votre propre professeur) |
| **ZACUS-BRANDING** | 150 € | Personnalisation visuelle BOX-3 (logo, couleurs, nom) |
| **ZACUS-TRAINING-2H** | 200 € | Formation game master supplémentaire (2h visio) |

---

## Récapitulatif business

### Analyse de rentabilité (Kit Standard)

| Poste | Valeur |
|-------|--------|
| BOM total | ~642 € |
| Prix de vente Kit Standard | 3 500 € HT |
| **Marge brute** | **~2 858 € (82 %)** |
| Temps de fabrication / formation | ~5h |
| Coût horaire valorisé | ~571 €/h |

### Projections revenus annuels (estimatif)

| Scénario | Sessions Animation | Locations | Kits | CA estimé HT |
|----------|--------------------|-----------|------|--------------|
| **Minimal** | 4/mois × 800 € | 2/mois × 400 € | 0 | ~57 600 € |
| **Réaliste** | 6/mois × 1 000 € | 4/mois × 400 € | 2/trimestre × 3 500 € | ~103 200 € |
| **Ambitieux** | 10/mois × 1 200 € | 8/mois × 400 € | 2/mois × 4 000 € | ~241 600 € |

### Revenus récurrents (SaaS-like)

| Client | Montant annuel HT |
|--------|-------------------|
| Support premium (par client Kit) | 500 € |
| 1 scénario/an par client Kit | 200 € |
| Renouvellement location régulier (asso) | ~1 200 € |

---

## Conditions générales

- **Devis valable 30 jours** à compter de la date d'émission
- **Acompte :** 30 % à la commande, solde à la livraison/prestation
- **Paiement :** Virement bancaire, chèque, CB (via Stripe)
- **Délai de livraison Kit :** 3 semaines (assemblage + tests + expédition)
- **Annulation Animation :** remboursement 100 % si > 7 jours, 50 % si 2–7 jours, 0 % si < 48h
- **Annulation Location :** remboursement 100 % si > 5 jours, 50 % si 2–5 jours

---

## Contact commercial

**Clément Saillant — L'Electron Rare**
clement@lelectronrare.fr | lelectronrare.fr/zacus | +33 6 XX XX XX XX

*SIRET : XX XXX XXX XXXXX — TVA intracommunautaire : FR XX XXXXXXXXX*
*Auto-entrepreneur / Micro-entreprise (seuil de franchise TVA selon chiffre d'affaires)*
