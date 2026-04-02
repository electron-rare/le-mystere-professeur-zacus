#!/usr/bin/env python3
"""
WhisperX STT server — OpenAI-compatible transcription endpoint.

Runs on KXKM-AI (RTX 4090). Exposes /v1/audio/transcriptions
compatible with the existing mascarade voice bridge.

Usage:
    python3 tools/stt/whisperx_server.py [--port 8901] [--model large-v3]

Requires: pip install whisperx fastapi uvicorn python-multipart
"""

import argparse
import logging
import os
import tempfile
import time

import uvicorn
from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import JSONResponse

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("stt")

app = FastAPI(title="WhisperX STT", version="0.1.0")

# Global model (loaded once at startup)
_model = None
_device = "cuda"
_compute_type = "float16"


def load_model(model_name: str):
    global _model
    import whisperx
    log.info("Loading WhisperX model %s on %s (%s)...", model_name, _device, _compute_type)
    t0 = time.time()
    _model = whisperx.load_model(model_name, device=_device, compute_type=_compute_type, language="fr")
    log.info("Model loaded in %.1fs", time.time() - t0)


@app.get("/health")
def health():
    return {"ok": _model is not None, "device": _device}


@app.post("/v1/audio/transcriptions")
async def transcribe(
    file: UploadFile = File(...),
    model: str = Form("large-v3"),
    language: str = Form("fr"),
):
    if _model is None:
        return JSONResponse(status_code=503, content={"error": "Model not loaded"})

    # Save uploaded file to temp
    suffix = os.path.splitext(file.filename or "audio.wav")[1]
    with tempfile.NamedTemporaryFile(suffix=suffix, delete=False) as tmp:
        content = await file.read()
        tmp.write(content)
        tmp_path = tmp.name

    try:
        t0 = time.time()
        result = _model.transcribe(tmp_path, language=language, batch_size=1)
        text = " ".join(seg["text"].strip() for seg in result.get("segments", []))
        elapsed = time.time() - t0
        log.info("Transcribed %.1fs audio in %.1fs: %s", len(content) / 32000, elapsed, text[:80])
        return {"text": text}
    finally:
        os.unlink(tmp_path)


def main():
    parser = argparse.ArgumentParser(description="WhisperX STT server")
    parser.add_argument("--port", type=int, default=8901)
    parser.add_argument("--model", default="large-v3")
    parser.add_argument("--host", default="0.0.0.0")
    args = parser.parse_args()

    load_model(args.model)
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
