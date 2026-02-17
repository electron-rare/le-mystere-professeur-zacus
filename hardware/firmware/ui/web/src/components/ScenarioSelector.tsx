import React, { useEffect, useState } from 'react';

interface Scenario {
  id: string;
  version: number;
  estimated_duration_s: number;
}

const ScenarioSelector: React.FC = () => {
  const [scenarios, setScenarios] = useState<Scenario[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selected, setSelected] = useState<string | null>(null);
  const [playing, setPlaying] = useState<string | null>(null);

  useEffect(() => {
    setLoading(true);
    fetch('/api/story/list')
      .then((res) => {
        if (!res.ok) throw new Error('Erreur API');
        return res.json();
      })
      .then((data) => {
        setScenarios(data.scenarios || []);
        setError(null);
      })
      .catch(() => setError('Impossible de charger les scénarios'))
      .finally(() => setLoading(false));
  }, []);

  const handlePlay = async (id: string) => {
    setPlaying(id);
    try {
      const res1 = await fetch(`/api/story/select/${id}`, { method: 'POST' });
      if (!res1.ok) throw new Error();
      const res2 = await fetch('/api/story/start', { method: 'POST' });
      if (!res2.ok) throw new Error();
      setSelected(id);
    } catch {
      setError('Erreur lors du démarrage du scénario');
    } finally {
      setPlaying(null);
    }
  };

  return (
    <section aria-labelledby="scenarios-title">
      <h2 id="scenarios-title">Scénarios disponibles</h2>
      {loading && <div role="status">Chargement…</div>}
      {error && <div role="alert" style={{ color: 'red' }}>{error}</div>}
      <ul style={{ display: 'flex', flexWrap: 'wrap', gap: 16, padding: 0, listStyle: 'none' }}>
        {scenarios.map((s) => (
          <li key={s.id} style={{ border: selected === s.id ? '2px solid #007bff' : '1px solid #ccc', borderRadius: 8, padding: 16, minWidth: 180 }}>
            <div><strong>{s.id}</strong> (v{s.version})</div>
            <div>Durée : {s.estimated_duration_s}s</div>
            <button
              onClick={() => handlePlay(s.id)}
              disabled={playing === s.id}
              aria-pressed={selected === s.id}
              style={{ marginTop: 8, width: '100%' }}
            >
              {playing === s.id ? 'Démarrage…' : 'Play'}
            </button>
          </li>
        ))}
      </ul>
    </section>
  );
};

export default ScenarioSelector;
