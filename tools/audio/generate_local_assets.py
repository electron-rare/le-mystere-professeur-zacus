#!/usr/bin/env python3
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
out = ROOT / 'audio' / 'generated'
out.mkdir(parents=True, exist_ok=True)

source_pool = {
    'uson_boot_corrompu_30s.mp3': ROOT / 'hardware/firmware/esp32/data/uson_boot_corrompu_lowmono.mp3',
    'uson_win_15s.mp3': ROOT / 'hardware/firmware/esp32/data/uson_mode_hero.mp3',
    'zone5_radio_brouille_v1_30s.mp3': ROOT / 'hardware/firmware/esp32/data/uson_etape1_radio_lowmono.mp3',
    'zone5_radio_brouille_v2_45s.mp3': ROOT / 'hardware/firmware/esp32/sonGPT/uson_etape1_radio_lointaine_audible.mp3',
    'hotline_validation_01.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
    'hotline_indice_01.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
    'hotline_relance_01.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
    'hotline_validation_02.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
    'hotline_indice_02.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
    'hotline_relance_02.mp3': ROOT / 'hardware/firmware/esp32/data/uson_unlock_rapport_imprime.mp3',
}

for name, src in source_pool.items():
    if not src.exists():
        raise SystemExit(f"Missing source asset: {src}")
    dst = out / name
    shutil.copyfile(src, dst)
    print(f"generated {dst.relative_to(ROOT)} <- {src.relative_to(ROOT)}")
