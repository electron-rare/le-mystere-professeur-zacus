# Workflow printables (prompts -> images -> PDF)

1. Choisir un prompt dans `printables/src/prompts/`.
2. Renseigner les variables (`ZONE_NAME`, `COLOR`, etc.).
3. Générer une image HD (300 DPI min).
4. Vérifier lisibilité N&B (test impression brouillon).
5. Exporter PDF dans `printables/export/pdf/...` et PNG preview dans `printables/export/png/...`.

## Nommage
- `zacus_v1_<type>_<id>_v01.(png|pdf)`
- Ex: `zacus_v1_carte-zone_Z3_v01.pdf`

## Génération locale rapide (placeholder PDF)
- `python3 tools/printables/generate_local_pdf_placeholders.py`
- Sortie: `printables/export/pdf/zacus_v1/`

## Check pré-impression
- Marges sécurité OK
- Contraste N&B OK
- Orthographe OK
- Pas de logos/marques tierces
- Cohérence scénario (noms/indices/solution)
