import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'story-editor': ['@uiw/react-codemirror', '@codemirror/lang-yaml'],
          'story-flow': ['@xyflow/react', 'dagre'],
          'story-yaml': ['yaml'],
        },
      },
    },
  },
})
