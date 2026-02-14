# Q&A client - Stack MP3

## Q1 - Pourquoi deux backends audio ?

Reponse:

- pour combiner modernisation et garantie terrain
- backend tools pour la trajectoire moderne
- backend legacy pour la couverture multi-formats et la continite de service

## Q2 - Comment vous prouvez que ca reste fluide ?

Reponse:

- scan incremental avec budget par tick
- commandes serie repondent pendant scan
- navigation UI et ecran restent reactifs

## Q3 - Que se passe-t-il si un codec n'est pas supporte par le backend actif ?

Reponse:

- en `AUTO_FALLBACK`, bascule vers legacy
- `MP3_BACKEND_STATUS` expose `last_fallback_reason` et les compteurs

## Q4 - L'ecran peut se desynchroniser ?

Reponse:

- protocole `STAT` resilient avec `seq` + CRC
- parser backward-compatible
- reprise apres reset croise testee en runbook

## Q5 - Quel est le risque principal en production ?

Reponse:

- variabilite materielle USB/SD/qualite fichiers
- mitigee par scripts QA, checklist live et observabilite runtime

## Q6 - Que faut-il pour ajouter des fonctions MP3 ensuite ?

Reponse:

- conserver le contrat canonique `MP3_*`
- enrichir controller/UI sans casser les commandes existantes
- ajouter evidence QA et smoke avant merge

## Q7 - Quelle est la prochaine etape apres cette demo ?

Reponse:

- finaliser extraction complete logique MP3 hors orchestrateur
- stabiliser UX OLED page-aware en condition chargee
- industrialiser campagne live RC avec rapport automatique

