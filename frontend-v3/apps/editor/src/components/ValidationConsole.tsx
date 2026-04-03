import type { ValidationResult } from '../lib/validator.js';

interface ValidationConsoleProps {
  result: ValidationResult | null;
  className?: string;
}

/**
 * Displays validation results: summary line, errors list, warnings list.
 * Shown in the footer bar of the editor.
 */
export function ValidationConsole({ result, className = '' }: ValidationConsoleProps) {
  if (!result) {
    return (
      <div className={className} style={styles.idle}>
        Cliquer sur Valider pour analyser le scénario
      </div>
    );
  }

  return (
    <div className={className} style={styles.container}>
      <span style={{ color: result.valid ? '#34c759' : '#ff453a', fontWeight: 600 }}>
        {result.summary}
      </span>

      {result.errors.length > 0 && (
        <ul style={styles.list}>
          {result.errors.map((err, i) => (
            <li key={i} style={{ color: '#ff453a' }}>
              • {err}
            </li>
          ))}
        </ul>
      )}

      {result.warnings.length > 0 && (
        <ul style={styles.list}>
          {result.warnings.map((warn, i) => (
            <li key={i} style={{ color: '#ff9500' }}>
              ⚠ {warn}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

const styles = {
  idle: {
    color: 'rgba(255,255,255,0.4)',
    fontSize: 12,
    display: 'flex',
    alignItems: 'center',
    height: '100%',
  },
  container: {
    display: 'flex',
    alignItems: 'center',
    gap: 16,
    fontSize: 12,
    height: '100%',
    flexWrap: 'wrap' as const,
    overflow: 'hidden',
  },
  list: {
    display: 'flex',
    gap: 12,
    listStyle: 'none',
    padding: 0,
    margin: 0,
    flexWrap: 'wrap' as const,
  },
};
