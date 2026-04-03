import { WebSocketServer } from 'ws';
import type { GameEvent } from '@zacus/shared';

const wss = new WebSocketServer({ port: 81 });

const EVENTS: GameEvent[] = [
  { type: 'game_start', timestamp: Date.now(), data: { session_id: 'test-session-001' } },
  { type: 'profile_detected', timestamp: Date.now(), data: { profile: 'TECH' } },
  { type: 'puzzle_solved', timestamp: Date.now(), data: { puzzle_id: 'P1_SON' } },
  { type: 'hint_given', timestamp: Date.now(), data: { puzzle_id: 'P2_CIRCUIT' } },
  { type: 'npc_spoke', timestamp: Date.now(), data: { phrase: 'Excellent travail ! Continuez !' } },
];

wss.on('connection', (ws) => {
  console.log('[mock] Dashboard connected');
  let i = 0;
  const interval = setInterval(() => {
    if (i >= EVENTS.length) { clearInterval(interval); return; }
    const event = { ...EVENTS[i++], timestamp: Date.now() };
    ws.send(JSON.stringify(event));
    // Also send a timer update every interval
    ws.send(JSON.stringify({ type: 'timer_update', timestamp: Date.now(), data: { elapsed_ms: i * 2000 } }));
  }, 2000);

  ws.on('message', (data) => {
    console.log('[mock] Command received:', data.toString());
  });

  ws.on('close', () => clearInterval(interval));
});

console.log('[mock] BOX-3 mock WS server on ws://localhost:81');
