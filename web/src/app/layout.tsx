import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'ArchRadar 3D — Spatial Architecture Studio',
  description: 'Macro-to-Micro Code Topology, Probabilistic Clone Detection & Abstraction Sprawl Visualizer',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en" className="dark">
      <body className="bg-bg text-text antialiased overflow-hidden select-none">
        {children}
      </body>
    </html>
  );
}
