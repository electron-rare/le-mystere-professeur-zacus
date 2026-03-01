export type ScenarioCatalogEntry = {
  title: string
  summary: string
  highlights: string[]
  acts: string[]
  stations: string[]
  puzzles: {
    id: string
    label: string
    description: string
  }[]
  printables: string[]
}

export const scenarioCatalog: Record<string, ScenarioCatalogEntry> = {
  zacus_v2: {
    title: 'Le mystère du Professeur Zacus — Version réelle',
    summary:
      'U-SON dérive : Acte 1 stabilise le LA 440 Hz, Acte 2 active Zone 4 (piano LEFOU) puis le QR WIN aux Archives.',
    highlights: ['Acte 1 — LA 440 Hz', 'Acte 2 — piano LEFOU', 'Finale QR : WIN'],
    acts: [
      'Acte 1 (≈45 min) — Atelier des Ondes : stabiliser la référence scientifique LA 440.',
      'Acte 2 (≈60 min) — Zone 4 + Archives : jouer LEFOU puis scanner QR WIN.',
    ],
    stations: [
      'Atelier des Ondes (LA 440 Hz)',
      'Zone 4 — Studio de Résonance — code musical LEFOU',
      'Salle des Archives — QR WIN derrière le portrait',
    ],
    puzzles: [
      {
        id: 'PUZZLE_LA_440',
        label: 'LA 440 Hz',
        description: 'Stabiliser la note et garder le retour sonore pendant plusieurs secondes pour débloquer l’étape 1.',
      },
      {
        id: 'PUZZLE_PIANO_ALPHABET_5',
        label: 'Piano LEFOU',
        description: 'Trouver L‑E‑F‑O‑U sur les touches blanches (A sur LA 440) et valider la séquence.',
      },
      {
        id: 'PUZZLE_QR_WIN',
        label: 'QR “WIN”',
        description: 'Scanner le QR caché derrière le portrait dans la Salle des Archives (QR_TIMEOUT 30 s).',
      },
    ],
    printables: [
      'Invitation A6 recto/verso',
      'Cartes personnages, lieux et objets',
      'Fiche enquête A4, règles A4 et badge détective',
      'Affiches zones Z1→Z6 et hotline Professeur Zacus',
    ],
  },
}

const scenarioAliases: Record<string, keyof typeof scenarioCatalog> = {
  default: 'zacus_v2',
  zacus: 'zacus_v2',
  mystery_zacus: 'zacus_v2',
}

const normalizeScenarioKey = (value: string) =>
  value
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '')

export const resolveScenarioCatalogEntry = (scenarioId: string): ScenarioCatalogEntry | undefined => {
  const normalized = normalizeScenarioKey(scenarioId)
  if (!normalized) {
    return undefined
  }

  if (scenarioCatalog[normalized]) {
    return scenarioCatalog[normalized]
  }

  const aliasKey = scenarioAliases[normalized]
  if (aliasKey && scenarioCatalog[aliasKey]) {
    return scenarioCatalog[aliasKey]
  }

  const directMatch = Object.entries(scenarioCatalog).find(([key]) => normalized.includes(key) || key.includes(normalized))
  return directMatch?.[1]
}
