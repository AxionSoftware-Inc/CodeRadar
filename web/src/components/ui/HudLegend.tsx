'use client';

import React from 'react';

export const HudLegend: React.FC = () => {
  return (
    <div className="absolute bottom-4 left-1/2 -translate-x-1/2 bg-panel/90 backdrop-blur-md border border-border rounded-full px-4 py-1.5 flex items-center gap-4 z-10 shadow-2xl pointer-events-none select-none">
      <div className="flex items-center gap-1.5 text-[11px] text-muted">
        <div className="w-2.5 h-2.5 rounded-full bg-teal-400" />
        <span>Dart/UI</span>
      </div>
      <div className="flex items-center gap-1.5 text-[11px] text-muted">
        <div className="w-2.5 h-2.5 rounded-full bg-blue-400" />
        <span>C++ Core</span>
      </div>
      <div className="flex items-center gap-1.5 text-[11px] text-muted">
        <div className="w-2.5 h-2.5 rounded-full bg-purple-400" />
        <span>C# Plugin</span>
      </div>
      <div className="flex items-center gap-1.5 text-[11px] text-muted">
        <div className="w-2.5 h-2.5 rounded-full bg-rose-500" />
        <span className="text-rose-400 font-medium">Sikl / Xavf</span>
      </div>
      <div className="flex items-center gap-1.5 text-[11px] text-muted">
        <div className="w-2.5 h-2.5 rounded-full bg-amber-400" />
        <span className="text-amber-400 font-medium">God Object</span>
      </div>
      <div className="flex items-center gap-1.5 text-[11px] text-amber-300 font-medium">
        <div className="w-2.5 h-2.5 rounded-full bg-amber-300 animate-pulse" />
        <span>Sprawl Klaster</span>
      </div>
    </div>
  );
};
