interface YamlPreviewProps {
  yaml: string;
  className?: string;
}

/**
 * Read-only syntax-highlighted YAML preview panel.
 * Uses a <pre> with CSS-based token coloring — no Monaco dependency.
 */
export function YamlPreview({ yaml, className = '' }: YamlPreviewProps) {
  if (!yaml) {
    return (
      <div className={`yaml-preview-empty ${className}`} style={styles.empty}>
        <span style={{ color: '#636366', fontSize: 12 }}>
          Le YAML apparaît ici après validation.
        </span>
      </div>
    );
  }

  // Minimal token-based syntax highlighting
  const highlighted = yaml
    .split('\n')
    .map((line, i) => <YamlLine key={i} line={line} />);

  return (
    <pre className={`yaml-preview ${className}`} style={styles.pre}>
      {highlighted}
    </pre>
  );
}

function YamlLine({ line }: { line: string }) {
  // Key: color #7dd3fc (blue), value: #a3e635 (green), comment: #6b7280 (gray), string: #fbbf24 (amber)
  const parts: Array<{ text: string; color: string }> = [];

  const commentMatch = /^(\s*)(#.*)$/.exec(line);
  if (commentMatch) {
    const [, indent, comment] = commentMatch;
    parts.push({ text: indent, color: '#fff' });
    parts.push({ text: comment, color: '#6b7280' });
    return <span style={{ display: 'block' }}>{parts.map((p, i) => <span key={i} style={{ color: p.color }}>{p.text}</span>)}{'\n'}</span>;
  }

  const kvMatch = /^(\s*)([^:]+)(:)(\s*.*)$/.exec(line);
  if (kvMatch) {
    const [, indent, key, colon, rest] = kvMatch;
    const valueColor = rest.trim().startsWith('"') ? '#fbbf24' : '#a3e635';
    return (
      <span style={{ display: 'block' }}>
        <span style={{ color: '#fff' }}>{indent}</span>
        <span style={{ color: '#7dd3fc' }}>{key}</span>
        <span style={{ color: '#fff' }}>{colon}</span>
        <span style={{ color: valueColor }}>{rest}</span>
        {'\n'}
      </span>
    );
  }

  return <span style={{ display: 'block', color: '#d1d5db' }}>{line}{'\n'}</span>;
}

const styles = {
  pre: {
    flex: 1,
    overflow: 'auto',
    padding: '12px',
    margin: 0,
    fontSize: 11,
    lineHeight: 1.5,
    fontFamily: "'SF Mono', 'Fira Code', 'Fira Mono', monospace",
    backgroundColor: 'transparent',
    whiteSpace: 'pre' as const,
  },
  empty: {
    flex: 1,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 12,
  },
};
