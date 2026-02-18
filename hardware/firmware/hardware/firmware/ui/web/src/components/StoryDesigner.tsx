import React, { useEffect, useState } from 'react';

const TEMPLATES = [
  { label: 'Défaut', file: 'default_unlock_win_etape2.yaml' },
  { label: 'Express', file: 'example_unlock_express.yaml' },
  { label: 'Spectre', file: 'spectre_radio_lab.yaml' },
];

const fetchTemplate = async (file: string) => {
  const res = await fetch(`/docs/protocols/story_specs/scenarios/${file}`);
  return res.ok ? res.text() : '';
};

const LOCAL_KEY = 'story_designer_yaml';

const StoryDesigner: React.FC = () => {
  const [yaml, setYaml] = useState('');
  const [result, setResult] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [template, setTemplate] = useState(TEMPLATES[0].file);

  // Load from localStorage
  useEffect(() => {
    const saved = localStorage.getItem(LOCAL_KEY);
    if (saved) setYaml(saved);
    else loadTemplate(TEMPLATES[0].file);
    // eslint-disable-next-line
  }, []);

  // Auto-save
  useEffect(() => {
    localStorage.setItem(LOCAL_KEY, yaml);
  }, [yaml]);

  const loadTemplate = async (file: string) => {
    setLoading(true);
    setError(null);
    setResult(null);
    setTemplate(file);
    try {
      const content = await fetchTemplate(file);
      setYaml(content);
    } catch {
      setError('Erreur chargement template');
    } finally {
      setLoading(false);
    }
  };

  const handleValidate = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await fetch('/api/story/validate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ yaml }),
      });
      const data = await res.json();
      if (res.ok && data.valid) setResult('YAML valide ✔️');
      else setError('YAML invalide');
    } catch {
      setError('Erreur validation');
    } finally {
      setLoading(false);
    }
  };

  const handleDeploy = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await fetch('/api/story/deploy', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-yaml' },
        body: yaml,
      });
      if (res.ok) setResult('Déploiement réussi ✔️');
      else setError('Erreur déploiement');
    } catch {
      setError('Erreur déploiement');
    } finally {
      setLoading(false);
    }
  };

  return (
    <section aria-labelledby="designer-title">
      <h2 id="designer-title">Designer de scénario</h2>
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: 24 }}>
        <div style={{ flex: 1, minWidth: 260 }}>
          <label htmlFor="template-select">Template : </label>
          <select
            id="template-select"
            value={template}
            onChange={(e) => loadTemplate(e.target.value)}
            style={{ marginBottom: 8 }}
          >
            {TEMPLATES.map((tpl) => (
              <option key={tpl.file} value={tpl.file}>{tpl.label}</option>
            ))}
          </select>
          <textarea
            value={yaml}
            onChange={(e) => setYaml(e.target.value)}
            rows={18}
            style={{ width: '100%', fontFamily: 'monospace', fontSize: 14, borderRadius: 4, border: '1px solid #ccc' }}
            aria-label="Éditeur YAML"
            disabled={loading}
          />
          <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
            <button onClick={handleValidate} disabled={loading}>Validate</button>
            <button onClick={handleDeploy} disabled={loading}>Deploy</button>
          </div>
          {loading && <div role="status">Traitement…</div>}
          {result && <div style={{ color: 'green' }}>{result}</div>}
          {error && <div role="alert" style={{ color: 'red' }}>{error}</div>}
        </div>
      </div>
    </section>
  );
};

export default StoryDesigner;
