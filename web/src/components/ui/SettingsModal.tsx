'use client';

import React from 'react';
import { ViewportSettings, GroupByMode, HeightMetric } from '@/lib/types';
import { X, Sliders, Layers, BarChart3, Eye, CloudFog } from 'lucide-react';

interface SettingsModalProps {
  isOpen: boolean;
  settings: ViewportSettings;
  onChange: (next: ViewportSettings) => void;
  onClose: () => void;
}

export const SettingsModal: React.FC<SettingsModalProps> = ({
  isOpen,
  settings,
  onChange,
  onClose,
}) => {
  if (!isOpen) return null;

  const update = <K extends keyof ViewportSettings>(key: K, val: ViewportSettings[K]) => {
    onChange({ ...settings, [key]: val });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
      <div className="w-full max-w-md bg-panel border border-border rounded-2xl shadow-2xl overflow-hidden">
        <div className="flex items-center justify-between px-4 py-3 border-b border-border bg-black/30">
          <div className="flex items-center gap-2 text-sm font-bold text-white">
            <Sliders className="w-4 h-4 text-primary" />
            <span>Viewport va Tahlil Sozlamalari</span>
          </div>
          <button onClick={onClose} className="text-muted hover:text-white p-1 rounded-lg hover:bg-white/10">
            <X className="w-4 h-4" />
          </button>
        </div>

        <div className="p-4 space-y-4 text-xs">
          {/* Grouping Mode */}
          <div>
            <div className="font-semibold text-gray-200 flex items-center gap-1.5 mb-2">
              <Layers className="w-3.5 h-3.5 text-primary" />
              <span>Shaharni Orollarga Ajratish (Grouping)</span>
            </div>
            <div className="grid grid-cols-3 gap-1.5">
              {(
                [
                  { id: 'MODULE', label: 'Modullar', desc: 'Batafsil (feature/pkg)' },
                  { id: 'TOP_LEVEL', label: "Katta Bo'limlar", desc: 'Umumiy (src, apps)' },
                  { id: 'LANGUAGE', label: 'Tillar', desc: 'C++, Dart, Kotlin...' },
                ] as Array<{ id: GroupByMode; label: string; desc: string }>
              ).map((opt) => (
                <button
                  key={opt.id}
                  onClick={() => update('groupBy', opt.id)}
                  className={`p-2 rounded-xl border text-left transition-all ${
                    settings.groupBy === opt.id
                      ? 'bg-primary/20 border-primary text-white'
                      : 'bg-black/30 border-border text-muted hover:text-white hover:border-white/30'
                  }`}
                >
                  <div className="font-bold text-[11px]">{opt.label}</div>
                  <div className="text-[9px] opacity-75 mt-0.5">{opt.desc}</div>
                </button>
              ))}
            </div>
          </div>

          {/* Building Height Metric */}
          <div>
            <div className="font-semibold text-gray-200 flex items-center gap-1.5 mb-2">
              <BarChart3 className="w-3.5 h-3.5 text-amber-400" />
              <span>Bino Balandligi Metrikasi</span>
            </div>
            <div className="grid grid-cols-3 gap-1.5">
              {(
                [
                  { id: 'LOC', label: 'Hajm (LOC)' },
                  { id: 'COMPLEXITY', label: 'Murakkablik' },
                  { id: 'BLAST', label: 'Blast Radius' },
                ] as Array<{ id: HeightMetric; label: string }>
              ).map((opt) => (
                <button
                  key={opt.id}
                  onClick={() => update('heightMetric', opt.id)}
                  className={`py-2 px-2.5 rounded-xl border font-semibold text-[11px] transition-all ${
                    settings.heightMetric === opt.id
                      ? 'bg-amber-500/20 border-amber-400 text-amber-300'
                      : 'bg-black/30 border-border text-muted hover:text-white'
                  }`}
                >
                  {opt.label}
                </button>
              ))}
            </div>
          </div>

          {/* Visual Toggles */}
          <div className="space-y-2 pt-1 border-t border-border/60">
            <label className="flex items-center justify-between p-2.5 rounded-xl bg-black/30 border border-border/60 cursor-pointer hover:border-white/30">
              <span className="flex items-center gap-2 text-gray-200">
                <CloudFog className="w-4 h-4 text-sky-400" />
                <span>Masofa Tumani (Atmosphere Fog)</span>
              </span>
              <input
                type="checkbox"
                checked={settings.fogEnabled}
                onChange={(e) => update('fogEnabled', e.target.checked)}
                className="accent-sky-400 w-4 h-4"
              />
            </label>

            <label className="flex items-center justify-between p-2.5 rounded-xl bg-black/30 border border-border/60 cursor-pointer hover:border-white/30">
              <span className="flex items-center gap-2 text-gray-200">
                <Eye className="w-4 h-4 text-emerald-400" />
                <span>3D Modul Yorliqlari</span>
              </span>
              <input
                type="checkbox"
                checked={settings.showLabels}
                onChange={(e) => update('showLabels', e.target.checked)}
                className="accent-emerald-400 w-4 h-4"
              />
            </label>

            <label className="flex items-center justify-between p-2.5 rounded-xl bg-black/30 border border-border/60 cursor-pointer hover:border-white/30">
              <span className="flex items-center gap-2 text-gray-200">
                <Layers className="w-4 h-4 text-purple-400" />
                <span>Uzoqdan Qirralarni Yashirish (Semantic LOD)</span>
              </span>
              <input
                type="checkbox"
                checked={settings.lodEnabled}
                onChange={(e) => update('lodEnabled', e.target.checked)}
                className="accent-purple-400 w-4 h-4"
              />
            </label>
          </div>
        </div>
      </div>
    </div>
  );
};
