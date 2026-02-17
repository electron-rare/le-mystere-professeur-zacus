# Plan d’action template

Chaque fiche `.github/agents/<agent>.md` doit inclure une section `## Plan d’action` inspirée de ce modèle :

1. **Contexte / validation du contrat**  
   - run: <commande de vérification (ex. `git status -sb`)>  
2. **Étapes principales décrites par l’agent**  
   - run: <commande 1>  
   - run: <commande 2>  
3. **Reporting et artefacts**  
   - run: <commande pour documenter (ex. `python3 tools/dev/gen_cockpit_docs.py` ou `cat GIT_WRITE_OPS_FINAL_REPORT.md`)>  

Les lignes `- run:` sont interprétées par `tools/dev/plan_runner.sh` (voir plus bas). Ajoute une phrase descriptive pour chaque étape afin que le plan soit lisible par un humain.
