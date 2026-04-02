export function encodeScenarioToUrl(workspaceXml: string): string {
  const compressed = btoa(encodeURIComponent(workspaceXml));
  return `${window.location.origin}${window.location.pathname}#scenario=${compressed}`;
}

export function decodeScenarioFromUrl(hash: string): string | null {
  const match = hash.match(/#scenario=(.+)/);
  if (!match) return null;
  try {
    return decodeURIComponent(atob(match[1]));
  } catch {
    return null;
  }
}
