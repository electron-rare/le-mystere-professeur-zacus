import { useEffect, useRef, forwardRef, useImperativeHandle } from 'react';
import * as Blockly from 'blockly';
import { TOOLBOX_XML } from './toolbox.js';

export interface BlocklyWorkspaceHandle {
  getWorkspace: () => Blockly.WorkspaceSvg | null;
}

interface BlocklyWorkspaceProps {
  className?: string;
  onWorkspaceChange?: (workspace: Blockly.WorkspaceSvg) => void;
}

/**
 * React wrapper around the Blockly SVG workspace.
 * Exposes the workspace instance via an imperative ref handle.
 */
export const BlocklyWorkspace = forwardRef<BlocklyWorkspaceHandle, BlocklyWorkspaceProps>(
  ({ className = '', onWorkspaceChange }, ref) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null);

    useImperativeHandle(ref, () => ({
      getWorkspace: () => workspaceRef.current,
    }));

    useEffect(() => {
      if (!containerRef.current) return;

      const ws = Blockly.inject(containerRef.current, {
        toolbox: TOOLBOX_XML,
        grid: { spacing: 20, length: 3, colour: '#2c2c2e', snap: true },
        zoom: { controls: true, wheel: true, startScale: 0.9 },
        theme: Blockly.Themes.Dark,
        renderer: 'zelos',
        trashcan: true,
        move: { scrollbars: true, drag: true, wheel: false },
      });

      workspaceRef.current = ws;

      if (onWorkspaceChange) {
        ws.addChangeListener(() => onWorkspaceChange(ws));
      }

      const handleResize = () => Blockly.svgResize(ws);
      window.addEventListener('resize', handleResize);

      return () => {
        window.removeEventListener('resize', handleResize);
        ws.dispose();
        workspaceRef.current = null;
      };
    }, []); // eslint-disable-line react-hooks/exhaustive-deps

    return <div ref={containerRef} className={`blockly-container ${className}`} style={{ height: '100%', width: '100%' }} />;
  },
);

BlocklyWorkspace.displayName = 'BlocklyWorkspace';
