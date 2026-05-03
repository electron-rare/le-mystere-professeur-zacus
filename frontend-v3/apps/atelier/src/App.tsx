import { Layout } from './components/Layout.js';
import { useLiveDiff } from './lib/useLiveDiff.js';

export function App() {
  useLiveDiff();
  return <Layout />;
}
