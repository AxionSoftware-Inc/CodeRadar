'use client';

import React from 'react';
import { ViewMode, SmellFilter } from '@/lib/types';
import { 
  Building2, 
  Layers, 
  Orbit, 
  Cpu, 
  Grid3X3,
  Sparkles, 
  RefreshCw, 
  Workflow, 
  Maximize2, 
  UploadCloud, 
  Waves,
  Zap,
  Flame,
  FileCode2,
  Sliders
} from 'lucide-react';

interface HeaderProps {
  healthScore: number;
  currentMode: ViewMode;
  onModeChange: (mode: ViewMode) => void;
  flowActive: boolean;
  onToggleFlow: () => void;
  blastSimActive: boolean;
  onToggleBlastSim: () => void;
  smellFilter: SmellFilter;
  onSmellFilterChange: (filter: SmellFilter) => void;
  onNextSprawl: () => void;
  onNextCycle: () => void;
  onNextClone: () => void;
  onNextGod: () => void;
  onResetCam: () => void;
  onOpenSettings: () => void;
  onFileUpload: (file: File) => void;
  datasetName: string;
  onSelectDataset: (name: 'bim' | 'demo') => void;
}

export const Header: React.FC<HeaderProps> = ({
  healthScore,
  currentMode,
  onModeChange,
  flowActive,
  onToggleFlow,
  blastSimActive,
  onToggleBlastSim,
  smellFilter,
  onSmellFilterChange,
  onNextSprawl,
  onNextCycle,
  onNextClone,
  onNextGod,
  onResetCam,
  onOpenSettings,
  onFileUpload,
  datasetName,
  onSelectDataset,
}) => {
  const getGradeInfo = (score: number) => {
    if (score >= 90) return { grade: 'A', text: "A'lo", color: 'text-emerald-400 border-emerald-500/40 bg-emerald-500/10' };
    if (score >= 75) return { grade: 'B', text: 'Yaxshi', color: 'text-sky-400 border-sky-500/40 bg-sky-500/10' };
    if (score >= 60) return { grade: 'C', text: "O'rtacha", color: 'text-amber-400 border-amber-500/40 bg-amber-500/10' };
    if (score >= 40) return { grade: 'D', text: 'Zaif', color: 'text-orange-400 border-orange-500/40 bg-orange-500/10' };
    return { grade: 'F', text: 'Xavfli', color: 'text-rose-400 border-rose-500/40 bg-rose-500/10' };
  };

  const gradeInfo = getGradeInfo(healthScore);

  return (
    <header className="absolute top-3 left-3 right-3 h-[54px] bg-panel/90 backdrop-blur-md border border-border rounded-xl flex items-center justify-between px-3 z-30 shadow-2xl">
      {/* Brand & Score */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-2">
          <div className="w-7 h-7 rounded-lg bg-primary/20 border border-primary/50 flex items-center justify-center text-primary font-bold shadow-[0_0_12px_rgba(56,189,248,0.3)]">
            ⬡
          </div>
          <div>
            <div className="text-xs font-bold tracking-tight text-white flex items-center gap-1.5 leading-none">
              ArchRadar 3D
              <span className="text-[9px] font-mono px-1.5 py-0.2 rounded bg-white/10 text-primary">v2.0 Core</span>
            </div>
            <div className="text-[10px] text-muted leading-tight mt-0.5">Spatial Topology & Architecture Linter</div>
          </div>
        </div>

        {/* Health Score Pill */}
        <div className={`px-2.5 py-1 rounded-lg text-xs font-semibold border flex items-center gap-1.5 ${gradeInfo.color}`}>
          <span className="font-bold">{Math.round(healthScore)}</span>
          <span className="text-[10px] opacity-70">/ 100</span>
          <span className="font-mono text-[10px] px-1 py-0.2 rounded bg-black/30">Baho: {gradeInfo.grade}</span>
        </div>

        {/* Dataset Switcher Pill */}
        <div className="hidden lg:flex items-center bg-black/40 border border-border rounded-lg p-0.5 text-[11px]">
          <button
            onClick={() => onSelectDataset('bim')}
            className={`px-2 py-0.5 rounded font-medium transition-all ${
              datasetName === 'BIM-Mobile'
                ? 'bg-primary/20 text-primary border border-primary/40 font-bold'
                : 'text-muted hover:text-white'
            }`}
          >
            BIM-Mobile (805 fayl)
          </button>
          <button
            onClick={() => onSelectDataset('demo')}
            className={`px-2 py-0.5 rounded font-medium transition-all ${
              datasetName === 'Demo Project'
                ? 'bg-primary/20 text-primary border border-primary/40 font-bold'
                : 'text-muted hover:text-white'
            }`}
          >
            Demo Tizim
          </button>
        </div>
      </div>

      {/* Mode Switcher Tabs */}
      <div className="flex items-center gap-1 bg-black/40 p-1 rounded-lg border border-border/60">
        <button
          onClick={() => onModeChange('CITY')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded-md transition-all ${
            currentMode === 'CITY'
              ? 'bg-primary/20 text-primary border border-primary/50 font-semibold shadow-[0_0_10px_rgba(56,189,248,0.25)]'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
          title="3D Kod Shahri (Software City)"
        >
          <Building2 className="w-3.5 h-3.5" />
          <span className="hidden md:inline">Kod Shahri</span>
        </button>

        <button
          onClick={() => onModeChange('LAYER')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded-md transition-all ${
            currentMode === 'LAYER'
              ? 'bg-primary/20 text-primary border border-primary/50 font-semibold shadow-[0_0_10px_rgba(56,189,248,0.25)]'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
          title="Qatlamlar oqimi (Core -> Domain -> API -> UI)"
        >
          <Layers className="w-3.5 h-3.5" />
          <span className="hidden md:inline">Qatlamlar</span>
        </button>

        <button
          onClick={() => onModeChange('GALAXY')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded-md transition-all ${
            currentMode === 'GALAXY'
              ? 'bg-primary/20 text-primary border border-primary/50 font-semibold shadow-[0_0_10px_rgba(56,189,248,0.25)]'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
          title="PageRank Galaktikasi"
        >
          <Orbit className="w-3.5 h-3.5" />
          <span className="hidden md:inline">Galaktika</span>
        </button>

        <button
          onClick={() => onModeChange('MICRO')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded-md transition-all ${
            currentMode === 'MICRO'
              ? 'bg-primary/20 text-primary border border-primary/50 font-semibold shadow-[0_0_10px_rgba(56,189,248,0.25)]'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
          title="Funksiyalar mikro-topologiyasi"
        >
          <Cpu className="w-3.5 h-3.5" />
          <span className="hidden md:inline">Funksiyalar</span>
        </button>

        <button
          onClick={() => onModeChange('DSM')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded-md transition-all ${
            currentMode === 'DSM'
              ? 'bg-emerald-500/20 text-emerald-400 border border-emerald-500/50 font-semibold shadow-[0_0_10px_rgba(16,185,129,0.25)]'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
          title="Dependency Structure Matrix (DSM) 2D Tahlil"
        >
          <Grid3X3 className="w-3.5 h-3.5" />
          <span className="hidden md:inline">DSM Matritsa</span>
        </button>
      </div>

      {/* Quick Actions & Toggles */}
      <div className="flex items-center gap-1.5">
        {/* Smell Spotlight Filter */}
        <div className="hidden 2xl:flex items-center gap-0.5 bg-black/40 border border-border/60 p-0.5 rounded-lg text-[10px]">
          {(
            [
              { id: 'ALL', label: 'Barchasi', active: 'bg-white/15 text-white' },
              { id: 'CYCLES', label: 'Sikllar', active: 'bg-rose-500/25 text-rose-300 border border-rose-500/40' },
              { id: 'CLONES', label: 'Klonlar', active: 'bg-purple-500/25 text-purple-300 border border-purple-500/40' },
              { id: 'HOTSPOTS', label: 'Hotspot', active: 'bg-orange-500/25 text-orange-300 border border-orange-500/40' },
            ] as const
          ).map((f) => (
            <button
              key={f.id}
              onClick={() => onSmellFilterChange(f.id)}
              className={`px-2 py-0.5 rounded font-medium transition-all ${
                smellFilter === f.id ? f.active : 'text-muted hover:text-white'
              }`}
              title={`Spotlight: ${f.label}`}
            >
              {f.label}
            </button>
          ))}
        </div>

        {/* Blast Radius Impact Simulator Toggle */}
        <button
          onClick={onToggleBlastSim}
          title="Blast Radius ta'sir zonasi simulyatori"
          className={`px-2.5 py-1 text-xs rounded-lg border transition-all flex items-center gap-1.5 ${
            blastSimActive
              ? 'bg-rose-500/20 border-rose-500 text-rose-400 font-bold shadow-[0_0_12px_rgba(244,63,94,0.3)] animate-pulse'
              : 'bg-black/30 border-border text-muted hover:border-white/30 hover:text-white'
          }`}
        >
          <Flame className="w-3.5 h-3.5" />
          <span className="hidden xl:inline">Blast Sim</span>
        </button>

        {/* Particles Flow Toggle */}
        <button
          onClick={onToggleFlow}
          title="Oqim zarralari (Code Flow Particles)"
          className={`px-2.5 py-1 text-xs rounded-lg border transition-all flex items-center gap-1.5 ${
            flowActive
              ? 'bg-primary/20 border-primary text-primary font-medium'
              : 'bg-black/30 border-border text-muted hover:border-white/30 hover:text-white'
          }`}
        >
          <Waves className="w-3.5 h-3.5" />
          <span className="hidden xl:inline">Oqim</span>
        </button>

        {/* Quick Issue Jumpers */}
        <div className="hidden sm:flex items-center gap-1 bg-black/40 border border-border/50 p-0.5 rounded-lg">
          <button
            onClick={onNextSprawl}
            title="Navbatdagi Abstraksiya Sprawl klasteriga uchish"
            className="p-1 rounded text-amber-400 hover:bg-amber-500/20 transition-colors"
          >
            <Sparkles className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={onNextCycle}
            title="Navbatdagi Aylanma Bog'liqlik halqasiga uchish"
            className="p-1 rounded text-rose-400 hover:bg-rose-500/20 transition-colors"
          >
            <RefreshCw className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={onNextClone}
            title="Navbatdagi Semantik Kod Dublikatiga uchish"
            className="p-1 rounded text-purple-400 hover:bg-purple-500/20 transition-colors"
          >
            <Workflow className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={onNextGod}
            title="Navbatdagi Markaziy Tugunga (Hub) uchish"
            className="p-1 rounded text-yellow-300 hover:bg-yellow-500/20 transition-colors"
          >
            <Zap className="w-3.5 h-3.5" />
          </button>
        </div>

        {/* Camera Reset */}
        <button
          onClick={onResetCam}
          title="Kamerani boshlang'ich holatga qaytarish"
          className="p-1.5 rounded-lg border border-border bg-black/30 text-muted hover:border-white/40 hover:text-white transition-all"
        >
          <Maximize2 className="w-3.5 h-3.5" />
        </button>

        {/* Viewport & Grouping Settings */}
        <button
          onClick={onOpenSettings}
          title="Viewport, Modul va Tahlil Sozlamalari"
          className="p-1.5 rounded-lg border border-border bg-black/30 text-muted hover:border-primary/60 hover:text-primary transition-all"
        >
          <Sliders className="w-3.5 h-3.5" />
        </button>

        {/* Upload Custom .bin / .json */}
        <label
          title="Binar (.bin) yoki JSON hisobot yuklash"
          className="px-2.5 py-1 text-xs rounded-lg border border-border bg-black/30 text-muted hover:border-white/40 hover:text-white transition-all cursor-pointer flex items-center gap-1.5"
        >
          <UploadCloud className="w-3.5 h-3.5" />
          <span className="hidden md:inline">Yuklash</span>
          <input
            type="file"
            accept=".bin,.json"
            className="hidden"
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) onFileUpload(file);
            }}
          />
        </label>
      </div>
    </header>
  );
};
