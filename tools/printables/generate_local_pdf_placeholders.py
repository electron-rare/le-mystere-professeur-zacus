#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'printables' / 'export' / 'pdf' / 'zacus_v1'
OUT.mkdir(parents=True, exist_ok=True)

# Minimal PDF writer (no external dependency)
def pdf_escape(s: str) -> str:
    return s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

def write_simple_pdf(path: Path, title: str, lines: list[str]):
    content_lines = [
        'BT',
        '/F1 18 Tf',
        '50 790 Td',
        f'({pdf_escape(title)}) Tj',
        '/F1 11 Tf',
    ]
    y = 760
    for ln in lines:
        content_lines.append(f'1 0 0 1 50 {y} Tm ({pdf_escape(ln)}) Tj')
        y -= 16
        if y < 60:
            break
    content_lines.append('ET')
    stream = '\n'.join(content_lines).encode('latin-1', errors='replace')

    objs = []
    objs.append(b'1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n')
    objs.append(b'2 0 obj << /Type /Pages /Count 1 /Kids [3 0 R] >> endobj\n')
    objs.append(b'3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >> endobj\n')
    objs.append(b'4 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n')
    objs.append(f'5 0 obj << /Length {len(stream)} >> stream\n'.encode() + stream + b'\nendstream endobj\n')

    header = b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n'
    xref_positions = [0]
    body = b''
    offset = len(header)
    for obj in objs:
        xref_positions.append(offset)
        body += obj
        offset += len(obj)
    xref = f'xref\n0 {len(xref_positions)}\n'.encode()
    xref += b'0000000000 65535 f \n'
    for pos in xref_positions[1:]:
        xref += f'{pos:010d} 00000 n \n'.encode()
    trailer = f'trailer << /Size {len(xref_positions)} /Root 1 0 R >>\nstartxref\n{offset}\n%%EOF\n'.encode()
    path.write_bytes(header + body + xref + trailer)

cards = [
    ('zacus_v1_regles_a4_v01.pdf', 'Regles Zacus v1', [
        'Duree 60-90 min | Age 9-11 ans | 6-14 enfants',
        '1) Enquete en equipes 2) Hotline Brigade Z 3) Resolution finale',
        'Lisibilite N&B prioritaire, sans logos externes.'
    ]),
    ('zacus_v1_hotline_a4_v01.pdf', 'Hotline Brigade Z', [
        'Reponses autorisees: Indice / Validation / Relance',
        'Tokens telephones: L - AFO - EFO - LE - U',
        'Solution finale unique: Professeur Electron'
    ]),
    ('zacus_v1_feuille_enquete_a4_v01.pdf', 'Feuille Enquete QUI/OU/COMMENT', [
        'QUI : ______________________',
        'OU : _______________________',
        'COMMENT : __________________',
        'PREUVE : ___________________'
    ]),
]

for filename, title, lines in cards:
    write_simple_pdf(OUT / filename, title, lines)
    print('generated', (OUT / filename).relative_to(ROOT))
