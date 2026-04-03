import { useState } from 'react';
import { BOX3_MDNS_HOST, BOX3_WS_PORT } from '@zacus/shared';

interface Props { onConnect: (url: string) => void; }

export function ConnectionSetup({ onConnect }: Props) {
  const defaultUrl = `ws://${BOX3_MDNS_HOST}:${BOX3_WS_PORT}`;
  const [url, setUrl] = useState(defaultUrl);

  return (
    <div className="flex flex-col items-center justify-center h-screen bg-[#1c1c1e] text-white gap-6">
      <h1 className="text-2xl font-bold">Dashboard Zacus V3</h1>
      <p className="text-white/60">Connecter au BOX-3</p>
      <div className="flex gap-2">
        <input
          type="text"
          value={url}
          onChange={(e) => setUrl(e.target.value)}
          className="w-80 px-4 py-2 rounded-xl bg-[#2c2c2e] border border-white/10 focus:border-[#0071e3] outline-none"
        />
        <button
          onClick={() => onConnect(url)}
          className="px-6 py-2 rounded-xl bg-[#0071e3] hover:bg-[#0077ed] font-medium"
        >
          Connecter
        </button>
      </div>
      <button
        onClick={() => onConnect(defaultUrl)}
        className="text-sm text-white/40 hover:text-white/80"
      >
        Utiliser mDNS par défaut ({BOX3_MDNS_HOST})
      </button>
    </div>
  );
}
