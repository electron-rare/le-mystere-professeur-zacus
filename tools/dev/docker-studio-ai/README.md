# Studio IA locale (Docker)

Ce setup lance :
- **Ollama** (moteur LLM local),
- **stable-diffusion-webui** (optionnel, profil `with-sdxl`) pour génération visuelle,
- **local_studio_ai_gateway.py** (pont HTTP attendu par le frontend :
  `story_generate`, `printables_plan`, `visual_generate`).

## Lancement

```bash
cd "tools/dev/docker-studio-ai"
cp .env.example .env
docker compose up -d --build
```
Pour activer SDXL :

```bash
docker compose --profile with-sdxl up -d --build
```

### One-liner de démarrage rapide

```bash
export LLM_MODEL="qwen2.5-coder:14b" \
&& cd "tools/dev/docker-studio-ai" \
&& [ -f .env ] || cp .env.example .env \
&& docker compose up -d --build \
&& docker exec -it zacus-ollama ollama pull "${LLM_MODEL}" \
&& docker compose --profile with-sdxl up -d --build
```

### Changer le LLM sans redémarrage complet

Pour basculer vers un autre provider local (ex: Mistral, Gemma, DeepSeek), change uniquement `LLM_URL` et `LLM_MODEL` dans `tools/dev/docker-studio-ai/.env`, puis relance la stack :

```bash
cd "tools/dev/docker-studio-ai"

# Exemple 1: Ollama local
python3 - <<'PY'
from pathlib import Path
p = Path(".env")
lines = p.read_text().splitlines()
updated = []
for line in lines:
    if line.startswith("LLM_URL="):
        updated.append("LLM_URL=http://127.0.0.1:11434/v1/chat/completions")
    elif line.startswith("LLM_MODEL="):
        updated.append("LLM_MODEL=mistral:7b-instruct")
    else:
        updated.append(line)
p.write_text("\n".join(updated) + "\n")
PY
docker compose up -d --build --force-recreate studio-ai-gateway

# Exemple 2: vLLM / LM Studio (local OpenAI-compatible)
python3 - <<'PY'
from pathlib import Path
p = Path(".env")
lines = p.read_text().splitlines()
updated = []
for line in lines:
    if line.startswith("LLM_URL="):
        updated.append("LLM_URL=http://127.0.0.1:1234/v1/chat/completions")
    elif line.startswith("LLM_MODEL="):
        updated.append("LLM_MODEL=mistral-7b-instruct")
    else:
        updated.append(line)
p.write_text("\n".join(updated) + "\n")
PY
docker compose up -d --build --force-recreate studio-ai-gateway

# Exemple 3: OpenAI API
python3 - <<'PY'
from pathlib import Path
p = Path(".env")
lines = p.read_text().splitlines()
updated = []
for line in lines:
    if line.startswith("LLM_URL="):
        updated.append("LLM_URL=https://api.openai.com/v1/chat/completions")
    elif line.startswith("LLM_MODEL="):
        updated.append("LLM_MODEL=gpt-4o-mini")
    else:
        updated.append(line)
p.write_text("\n".join(updated) + "\n")
PY
# Ajouter votre token dans votre environnement au moment de lancer docker (ex: OPENAI_API_KEY)
docker compose up -d --build --force-recreate studio-ai-gateway
```

Télécharge un modèle (1er lancement) :

```bash
docker exec -it zacus-ollama ollama pull qwen2.5-coder:14b
```

Vérifie le gateway :

```bash
curl http://127.0.0.1:8787/health
```

## Utilisation avec le frontend

Dans `fronted dev web UI/.env.local` :

```bash
VITE_ZACUS_STUDIO_AI_URL=http://127.0.0.1:8787/story_generate
```

Le frontend tentera l’endpoint IA puis basculera automatiquement sur le générateur local si indisponible.

### Génération d’images (optionnelle)

POST minimal :

```bash
curl -X POST http://127.0.0.1:8787 \
  -H "Content-Type: application/json" \
  -d '{
    "mode":"visual_generate",
    "prompt":"une affiche de concert rétro, style affiche de cirque",
    "width":1024,
    "height":1024,
    "steps":25,
    "cfgScale":7.5,
    "count":1
  }'
```

## Arrêt

```bash
docker compose down
```

Supprime le modèle Ollama si besoin :

```bash
docker exec -it zacus-ollama ollama rm qwen2.5-coder:14b
```
