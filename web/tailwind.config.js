/** @type {import('tailwindcss').Config} */
export default {
  content: [
    './src/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        bg: '#07090e',
        panel: 'rgba(13, 17, 23, 0.88)',
        border: 'rgba(48, 54, 61, 0.65)',
        primary: '#38bdf8',
        critical: '#f85149',
        warning: '#d29922',
        info: '#a371f7',
        sprawl: '#e3b341',
        success: '#3fb950',
        cpp: '#60a5fa',
        dart: '#00d2b8',
        csharp: '#a78bfa',
        python: '#fbbf24',
        js: '#f43f5e',
      },
      fontFamily: {
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'monospace'],
      },
    },
  },
  plugins: [],
};
