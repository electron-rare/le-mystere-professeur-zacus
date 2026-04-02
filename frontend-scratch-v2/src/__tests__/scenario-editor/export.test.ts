import { describe, it, expect } from 'vitest';
import { decodeScenarioFromUrl } from '../../components/ScenarioEditor/export/share';

// ─── Share URL encode/decode tests ───
// We test the core encode/decode logic without requiring window.location.
// encodeScenarioToUrl() uses window.location at runtime, so we test the
// underlying btoa/atob round-trip directly via decodeScenarioFromUrl.

function encodeForTest(xml: string): string {
  const compressed = btoa(encodeURIComponent(xml));
  return `#scenario=${compressed}`;
}

describe('share URL encode/decode round-trip', () => {
  it('round-trips a simple XML string', () => {
    const xml = '<xml><block type="scenario_scene"></block></xml>';
    const hash = encodeForTest(xml);

    expect(hash).toContain('#scenario=');
    const decoded = decodeScenarioFromUrl(hash);
    expect(decoded).toBe(xml);
  });

  it('round-trips XML with special characters (French, entities)', () => {
    const xml = '<xml><block name="scène éàü &amp; test"></block></xml>';
    const hash = encodeForTest(xml);
    const decoded = decodeScenarioFromUrl(hash);
    expect(decoded).toBe(xml);
  });

  it('round-trips an empty workspace', () => {
    const xml = '<xml></xml>';
    const hash = encodeForTest(xml);
    const decoded = decodeScenarioFromUrl(hash);
    expect(decoded).toBe(xml);
  });

  it('returns null for invalid hash formats', () => {
    expect(decodeScenarioFromUrl('')).toBeNull();
    expect(decodeScenarioFromUrl('#other=abc')).toBeNull();
    expect(decodeScenarioFromUrl('#noscenario')).toBeNull();
    expect(decodeScenarioFromUrl('no-hash-at-all')).toBeNull();
  });

  it('returns null for corrupted base64 payload', () => {
    expect(decodeScenarioFromUrl('#scenario=!!!invalid-base64!!!')).toBeNull();
  });
});

// ─── Download utility tests ───
// downloadYaml and downloadJson require document/URL which are browser-only.
// We verify the module exports exist and the function signatures are correct.
// Actual DOM interaction is covered by integration/E2E tests.

describe('download module exports', () => {
  it('exports downloadYaml function', async () => {
    const mod = await import('../../components/ScenarioEditor/export/download');
    expect(typeof mod.downloadYaml).toBe('function');
    expect(mod.downloadYaml.length).toBe(2); // (yaml, filename)
  });

  it('exports downloadJson function', async () => {
    const mod = await import('../../components/ScenarioEditor/export/download');
    expect(typeof mod.downloadJson).toBe('function');
    expect(mod.downloadJson.length).toBe(2); // (data, filename)
  });
});

describe('share module exports', () => {
  it('exports encodeScenarioToUrl function', async () => {
    const mod = await import('../../components/ScenarioEditor/export/share');
    expect(typeof mod.encodeScenarioToUrl).toBe('function');
    expect(mod.encodeScenarioToUrl.length).toBe(1); // (workspaceXml)
  });

  it('exports decodeScenarioFromUrl function', async () => {
    const mod = await import('../../components/ScenarioEditor/export/share');
    expect(typeof mod.decodeScenarioFromUrl).toBe('function');
    expect(mod.decodeScenarioFromUrl.length).toBe(1); // (hash)
  });
});
