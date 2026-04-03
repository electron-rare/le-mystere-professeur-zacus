import type { Config } from 'tailwindcss';

const config: Config = {
  content: ['./src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        fer: {
          accent: '#0071e3',
          surface: '#1c1c1e',
          card: '#2c2c2e',
          muted: '#636366',
          success: '#34c759',
          warning: '#ff9500',
          danger: '#ff3b30',
          purple: '#af52de',
        },
      },
      borderRadius: {
        xl: '1rem',
        '2xl': '1.5rem',
      },
      fontFamily: {
        sans: ['-apple-system', 'BlinkMacSystemFont', 'SF Pro Display', 'Segoe UI', 'sans-serif'],
        mono: ['SF Mono', 'Fira Code', 'monospace'],
      },
    },
  },
  plugins: [],
};

export default config;
