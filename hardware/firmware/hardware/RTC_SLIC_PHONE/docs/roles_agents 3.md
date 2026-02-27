# Répartition des rôles — RTC_BL_PHONE

Ce document propose une structure de rôles inspirée du kit Zacus, adaptée à l’utilisation du téléphone RTC expérimental.

## Rôles principaux

- **Opérateur principal** : gère la prise d’appel, le décrochage/raccrochage, et la navigation dans les commandes série.
- **Chronométreur** : surveille la durée des appels/tests, annonce les transitions (ex : passage à l’appel suivant).
- **Analyste technique** : supervise la connexion Bluetooth, vérifie l’état HFP, et diagnostique les problèmes matériels.
- **Archiviste** : note les résultats des tests, consigne les logs et les adresses MAC utilisées.
- **Témoin narrateur** : explique à voix haute chaque étape, relit les consignes de sécurité et d’utilisation.
- **Gardien du combiné** : veille à la manipulation correcte du combiné RTC et à la sécurité du matériel.

## Rôles optionnels

- **Ambassadeur** : communique avec d’autres équipes ou utilisateurs pour valider la réussite des tests.
- **Cartographe** : documente le câblage, les ports utilisés et la configuration matérielle.
- **Journaliste** : rédige un compte-rendu synthétique de la session.

## Utilisation

- Affecter un rôle à chaque participant lors des tests collaboratifs ou des démonstrations.
- Adapter la liste selon la taille de l’équipe et le contexte (atelier, démo, QA).
- Utiliser ce document comme support pour l’assistance IA ou la génération de checklists personnalisées.
