import React, { useState, useEffect, useRef, useCallback } from 'react';

interface MeshNode {
  id: string;
  name: string;
  rssi: number;
  latencyMs: number;
  packetLoss: number;
  lastSeen: number;
  online: boolean;
}

interface MeshTopologyMessage {
  type: string;
  nodes: Array<{
    name: string;
    rssi: number;
    latency_ms: number;
    packet_loss: number;
  }>;
}

const PUZZLE_NODES = ['BOX-3', 'P1 Son', 'P2', 'P4 Radio', 'P5 Morse', 'P6 NFC', 'P7 Coffre'];

function rssiColor(rssi: number): string {
  if (rssi > -60) return '#22c55e'; // strong — green
  if (rssi > -75) return '#f59e0b'; // medium — amber
  return '#ef4444';                  // weak — red
}

export function MeshDiagnostic(): React.JSX.Element {
  const [nodes, setNodes] = useState<MeshNode[]>(
    PUZZLE_NODES.map((name, i) => ({
      id: `node-${i}`,
      name,
      rssi: 0,
      latencyMs: 0,
      packetLoss: 0,
      lastSeen: 0,
      online: false,
    }))
  );
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // Listen for mesh topology messages from BOX-3 WebSocket
  useEffect(() => {
    window.zacus.wifi.onWsMessage(rawData => {
      try {
        const msg = JSON.parse(rawData) as MeshTopologyMessage;
        if (msg.type === 'mesh_topology') {
          setNodes(prev => prev.map(node => {
            const update = msg.nodes.find(n => n.name === node.name);
            if (!update) return node;
            return {
              ...node,
              rssi: update.rssi,
              latencyMs: update.latency_ms,
              packetLoss: update.packet_loss,
              lastSeen: Date.now(),
              online: true,
            };
          }));
        }
      } catch { /* not a JSON message */ }
    });
  }, []);

  // Draw mesh topology on canvas
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const W = canvas.width;
    const H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    // BOX-3 at center top
    const masterX = W / 2;
    const masterY = 60;

    // Puzzle nodes in a row below (skip index 0 = BOX-3)
    const puzzleNodes = nodes.slice(1);
    const spacing = W / (puzzleNodes.length + 1);

    ctx.font = '11px monospace';
    ctx.textAlign = 'center';

    puzzleNodes.forEach((node, i) => {
      const x = spacing * (i + 1);
      const y = H - 70;

      // Connection line
      ctx.beginPath();
      ctx.moveTo(masterX, masterY + 24);
      ctx.lineTo(x, y - 18);
      ctx.strokeStyle = node.online ? rssiColor(node.rssi) : '#333';
      ctx.lineWidth = node.online ? 2 : 1;
      ctx.stroke();

      // RSSI label at line midpoint
      if (node.online) {
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(
          `${node.rssi}dB`,
          (masterX + x) / 2,
          (masterY + 24 + y - 18) / 2 - 6
        );
      }

      // Node circle
      ctx.beginPath();
      ctx.arc(x, y, 18, 0, Math.PI * 2);
      ctx.fillStyle = node.online ? rssiColor(node.rssi) : '#374151';
      ctx.fill();
      ctx.strokeStyle = '#1f2937';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Node name label
      ctx.fillStyle = '#e2e8f0';
      ctx.font = '10px monospace';
      ctx.fillText(node.name.slice(0, 6), x, y + 34);

      if (node.online) {
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(`${node.latencyMs}ms`, x, y + 46);
      }
    });

    // BOX-3 master node
    ctx.beginPath();
    ctx.arc(masterX, masterY, 24, 0, Math.PI * 2);
    ctx.fillStyle = '#7c3aed';
    ctx.fill();
    ctx.strokeStyle = '#4c1d95';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fillStyle = '#e2e8f0';
    ctx.font = 'bold 11px monospace';
    ctx.fillText('BOX-3', masterX, masterY + 4);
  }, [nodes]);

  useEffect(() => { draw(); }, [draw]);

  const onlineCount = nodes.filter(n => n.online).length;

  return (
    <div className="mesh-diagnostic">
      <div className="panel-header">
        <h2>Mesh Diagnostic</h2>
        <span className="text-dim">
          ESP-NOW via BOX-3 WebSocket — {onlineCount}/{nodes.length} online
        </span>
      </div>

      <canvas
        ref={canvasRef}
        width={700}
        height={280}
        className="mesh-canvas"
      />

      <table className="mesh-table">
        <thead>
          <tr>
            <th>Device</th>
            <th>Status</th>
            <th>RSSI</th>
            <th>Latency</th>
            <th>Loss %</th>
            <th>Last seen</th>
          </tr>
        </thead>
        <tbody>
          {nodes.map(node => (
            <tr key={node.id}>
              <td>{node.name}</td>
              <td>
                <span className={`status-dot ${node.online ? 'online' : 'offline'}`} />
                {node.online ? 'Online' : 'Offline'}
              </td>
              <td
                className="mono"
                style={{ color: node.online ? rssiColor(node.rssi) : undefined }}
              >
                {node.online ? `${node.rssi} dBm` : '—'}
              </td>
              <td className="mono">
                {node.online ? `${node.latencyMs} ms` : '—'}
              </td>
              <td className={node.online && node.packetLoss > 5 ? 'warn mono' : 'mono'}>
                {node.online ? `${node.packetLoss.toFixed(1)}%` : '—'}
              </td>
              <td className="text-dim">
                {node.lastSeen
                  ? new Date(node.lastSeen).toLocaleTimeString()
                  : '—'}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
