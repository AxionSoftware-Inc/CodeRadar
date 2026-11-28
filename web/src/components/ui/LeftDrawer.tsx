'use client';

import React, { useState, useMemo } from 'react';
import { AppData, CodeNode, CodeIssue, getLangColorHex } from '@/lib/types';
import { 
  Folder, 
  FileCode, 
  AlertTriangle, 
  ChevronLeft, 
  ChevronRight, 
  Search, 
  Sparkles, 
  Flame, 
  Zap, 
  Workflow, 
  Activity,
  Layers
} from 'lucide-react';

interface LeftDrawerProps {
  data: AppData | null;
  selectedLang: string;
  onSelectLang: (lang: string) => void;
  onSelectNode: (node: CodeNode) => void;
  onSelectIssue: (issue: CodeIssue) => void;
  selectedNodeId?: string;
}

export const LeftDrawer: React.FC<LeftDrawerProps> = ({
  data,
  selectedLang,
  onSelectLang,
  onSelectNode,
  onSelectIssue,
  selectedNodeId,
}) => {
  const [isOpen, setIsOpen] = useState(true);
  const [activeTab, setActiveTab] = useState<'TREE' | 'ISSUES' | 'RANKINGS'>('TREE');
  const [issueFilter, setIssueFilter] = useState<'ALL' | 'CRITICAL' | 'WARNING' | 'INFO'>('ALL');
  const [search, setSearch] = useState('');

  const q = search.toLowerCase();

  // Group files by module
  const modules: Record<string, CodeNode[]> = useMemo(() => {
    if (!data) return {};
    const mods: Record<string, CodeNode[]> = {};
    data.nodes
      .filter((n) => n.type === 'file')
      .forEach((f) => {
        if (selectedLang !== 'ALL' && f.lang !== selectedLang) return;
        if (q && !f.name.toLowerCase().includes(q) && !f.module.toLowerCase().includes(q)) return;
        const m = f.module || 'root';
        if (!mods[m]) mods[m] = [];
        mods[m].push(f);
      });
    return mods;
  }, [data, selectedLang, q]);

  // Filter issues
  const filteredIssues = useMemo(() => {
    if (!data) return [];
    return data.issues.filter((iss) => {
      if (issueFilter !== 'ALL' && iss.severity !== issueFilter) return false;
      if (!q) return true;
      return (
        iss.title.toLowerCase().includes(q) ||
        iss.description.toLowerCase().includes(q) ||
        iss.type.toLowerCase().includes(q)
      );
    });
  }, [data, issueFilter, q]);

  // Top rankings calculation
  const rankings = useMemo(() => {
    if (!data) return { topHotspots: [], topBlast: [], topComplexity: [], topGods: [] };
    const files = data.nodes.filter((n) => n.type === 'file');
    const topHotspots = [...files].sort((a, b) => (b.hotspot_score || 0) - (a.hotspot_score || 0)).slice(0, 10);
    const topBlast = [...files].sort((a, b) => (b.blast_radius || 0) - (a.blast_radius || 0)).slice(0, 10);
    const topComplexity = [...files].sort((a, b) => (b.complexity || 0) - (a.complexity || 0)).slice(0, 10);
    const topGods = files.filter((n) => n.is_god || (n.pagerank || 0) > 0.05).slice(0, 10);
    return { topHotspots, topBlast, topComplexity, topGods };
  }, [data]);

  if (!data) return null;

  return (
    <>
      {/* Toggle Tab Handle */}
      <button
        onClick={() => setIsOpen(!isOpen)}
        className={`absolute top-[76px] z-30 bg-panel/90 border border-border text-muted hover:text-white p-1 rounded-r-lg shadow-xl backdrop-blur-md transition-all duration-300 ${
          isOpen ? 'left-[346px]' : 'left-0'
        }`}
        title={isOpen ? "Panelni yashirish" : "Panelni ochish"}
      >
        {isOpen ? <ChevronLeft className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
      </button>

      <div
        className={`absolute top-[74px] left-3 bottom-3 w-[340px] bg-panel/95 backdrop-blur-md border border-border rounded-xl z-20 flex flex-col shadow-2xl transition-transform duration-300 ease-in-out ${
          isOpen ? 'translate-x-0' : '-translate-x-[360px]'
        }`}
      >
        {/* Navigation Tabs */}
        <div className="flex border-b border-border p-1 gap-1 bg-black/30">
          <button
            onClick={() => setActiveTab('TREE')}
            className={`flex-1 py-1.5 text-xs font-semibold rounded-lg text-center transition-all ${
              activeTab === 'TREE'
                ? 'bg-white/10 text-white shadow-sm font-bold'
                : 'text-muted hover:text-white hover:bg-white/5'
            }`}
          >
            📁 Ierarxiya
          </button>
          <button
            onClick={() => setActiveTab('ISSUES')}
            className={`flex-1 py-1.5 text-xs font-semibold rounded-lg text-center transition-all flex items-center justify-center gap-1.5 ${
              activeTab === 'ISSUES'
                ? 'bg-white/10 text-white shadow-sm font-bold'
                : 'text-muted hover:text-white hover:bg-white/5'
            }`}
          >
            <AlertTriangle className="w-3.5 h-3.5 text-amber-400" />
            <span>Muammolar ({data.issues.length})</span>
          </button>
          <button
            onClick={() => setActiveTab('RANKINGS')}
            className={`flex-1 py-1.5 text-xs font-semibold rounded-lg text-center transition-all flex items-center justify-center gap-1.5 ${
              activeTab === 'RANKINGS'
                ? 'bg-white/10 text-white shadow-sm font-bold'
                : 'text-muted hover:text-white hover:bg-white/5'
            }`}
          >
            <Activity className="w-3.5 h-3.5 text-primary" />
            <span>Reyting</span>
          </button>
        </div>

        {/* Search Input */}
        <div className="p-2 border-b border-border/50">
          <div className="relative flex items-center">
            <Search className="w-3.5 h-3.5 text-muted absolute left-2.5 pointer-events-none" />
            <input
              type="text"
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              placeholder="Fayl, modul yoki muammo qidirish..."
              className="w-full bg-black/40 border border-border rounded-lg pl-8 pr-2.5 py-1.5 text-xs text-white placeholder-muted focus:outline-none focus:border-primary transition-colors"
            />
          </div>
        </div>

        {/* Language Filter Chips */}
        <div className="flex gap-1 px-2.5 py-1.5 overflow-x-auto border-b border-border/40 scrollbar-none">
          {[
            { id: 'ALL', label: 'Barchasi', color: 'text-muted' },
            { id: 'cpp', label: 'C/C++', color: 'text-blue-400' },
            { id: 'dart', label: 'Dart/UI', color: 'text-teal-400' },
            { id: 'kotlin', label: 'Kotlin', color: 'text-orange-400' },
            { id: 'cs', label: 'C#', color: 'text-purple-400' },
            { id: 'python', label: 'Python', color: 'text-amber-400' },
            { id: 'js', label: 'JS/TS', color: 'text-rose-400' },
          ].map((l) => (
            <button
              key={l.id}
              onClick={() => onSelectLang(l.id)}
              className={`text-[10px] px-2 py-0.5 rounded-md border whitespace-nowrap transition-all ${
                selectedLang === l.id
                  ? 'bg-primary/20 border-primary text-white font-bold'
                  : 'bg-black/30 border-transparent text-muted hover:text-white hover:border-border'
              }`}
            >
              <span className={l.color}>●</span> {l.label}
            </button>
          ))}
        </div>

        {/* Tab 1: Module Tree */}
        {activeTab === 'TREE' && (
          <div className="flex-1 overflow-y-auto p-2 space-y-1">
            {Object.keys(modules).length === 0 ? (
              <div className="text-center text-xs text-muted py-8">Fayllar topilmadi</div>
            ) : (
              Object.keys(modules)
                .sort()
                .map((mName) => {
                  const fList = modules[mName];
                  return (
                    <div key={mName} className="mb-2">
                      <div className="text-xs font-bold text-white/90 bg-white/5 px-2 py-1 rounded-lg flex justify-between items-center mb-1">
                        <span className="flex items-center gap-1.5 truncate">
                          <Folder className="w-3.5 h-3.5 text-primary flex-shrink-0" />
                          <span className="truncate">{mName}</span>
                        </span>
                        <span className="text-[10px] text-muted font-mono">{fList.length} fayl</span>
                      </div>

                      <div className="space-y-0.5 pl-1.5">
                        {fList.map((f) => {
                          const isSelected = selectedNodeId === f.id;
                          let statusDot = 'text-muted';
                          if (f.has_cycle || f.is_layer_violation) statusDot = 'text-rose-500';
                          else if (f.is_god) statusDot = 'text-amber-400';
                          else if (f.is_sprawl) statusDot = 'text-yellow-400';
                          else if (f.is_clone) statusDot = 'text-purple-400';

                          return (
                            <button
                              key={f.id}
                              onClick={() => onSelectNode(f)}
                              className={`w-full text-left px-2 py-1 rounded-md text-xs flex items-center justify-between gap-2 transition-all ${
                                isSelected
                                  ? 'bg-primary/20 text-primary border border-primary/40 font-semibold'
                                  : 'text-gray-300 hover:bg-white/5 hover:text-white'
                              }`}
                            >
                              <span className="flex items-center gap-1.5 truncate">
                                <span className={`text-[10px] ${statusDot}`}>●</span>
                                <span className="truncate">{f.name}</span>
                              </span>
                              <div className="flex items-center gap-1.5 flex-shrink-0">
                                <span className="text-[9px] text-muted font-mono">{f.loc}L</span>
                                <span
                                  className="text-[9px] px-1.5 py-0.2 rounded font-bold uppercase"
                                  style={{
                                    backgroundColor: 'rgba(255,255,255,0.06)',
                                    color: '#' + getLangColorHex(f.name).toString(16).padStart(6, '0'),
                                  }}
                                >
                                  {f.lang}
                                </span>
                              </div>
                            </button>
                          );
                        })}
                      </div>
                    </div>
                  );
                })
            )}
          </div>
        )}

        {/* Tab 2: Issues Center */}
        {activeTab === 'ISSUES' && (
          <div className="flex-1 flex flex-col overflow-hidden">
            {/* Issue Severity Filter */}
            <div className="flex gap-1 p-2 border-b border-border/40 text-[10px]">
              {(['ALL', 'CRITICAL', 'WARNING', 'INFO'] as const).map((sev) => (
                <button
                  key={sev}
                  onClick={() => setIssueFilter(sev)}
                  className={`flex-1 py-1 rounded font-semibold transition-all ${
                    issueFilter === sev
                      ? sev === 'CRITICAL'
                        ? 'bg-rose-500/20 text-rose-400 border border-rose-500/50'
                        : sev === 'WARNING'
                        ? 'bg-amber-500/20 text-amber-400 border border-amber-500/50'
                        : sev === 'INFO'
                        ? 'bg-sky-500/20 text-sky-400 border border-sky-500/50'
                        : 'bg-white/10 text-white border border-white/20'
                      : 'text-muted hover:text-white hover:bg-white/5'
                  }`}
                >
                  {sev}
                </button>
              ))}
            </div>

            <div className="flex-1 overflow-y-auto p-2 space-y-1.5">
              {filteredIssues.length === 0 ? (
                <div className="text-center text-xs text-muted py-8">Mos muammolar topilmadi</div>
              ) : (
                filteredIssues.map((iss) => {
                  let badgeCol = 'bg-sky-500/10 text-sky-400 border-sky-500/30';
                  if (iss.severity === 'CRITICAL') badgeCol = 'bg-rose-500/10 text-rose-400 border-rose-500/30';
                  else if (iss.severity === 'WARNING') badgeCol = 'bg-amber-500/10 text-amber-400 border-amber-500/30';

                  return (
                    <button
                      key={iss.id}
                      onClick={() => onSelectIssue(iss)}
                      className="w-full text-left p-2.5 rounded-lg border border-border/60 bg-black/30 hover:bg-black/50 hover:border-primary/50 transition-all group"
                    >
                      <div className="flex items-center justify-between gap-1 mb-1">
                        <span className={`text-[9px] font-mono font-bold px-1.5 py-0.2 rounded border ${badgeCol}`}>
                          {iss.type}
                        </span>
                        <span className="text-[10px] text-muted font-mono">{iss.involved_nodes.length} tugun</span>
                      </div>
                      <div className="text-xs font-bold text-white group-hover:text-primary transition-colors truncate">
                        {iss.title}
                      </div>
                      <div className="text-[11px] text-muted line-clamp-2 mt-1 leading-relaxed">
                        {iss.description}
                      </div>
                    </button>
                  );
                })
              )}
            </div>
          </div>
        )}

        {/* Tab 3: Deep Metric Rankings */}
        {activeTab === 'RANKINGS' && (
          <div className="flex-1 overflow-y-auto p-2 space-y-3">
            {/* Top Bottlenecks (Hotspot) */}
            <div>
              <div className="text-xs font-bold text-orange-400 flex items-center gap-1.5 mb-1.5 uppercase tracking-wide">
                <Activity className="w-3.5 h-3.5" />
                Top Bottlenecks (Hotspot)
              </div>
              <div className="space-y-1">
                {rankings.topHotspots.map((f, idx) => (
                  <button
                    key={f.id}
                    onClick={() => onSelectNode(f)}
                    className="w-full text-left px-2.5 py-1.5 rounded-lg bg-black/30 hover:bg-orange-500/10 border border-border/50 hover:border-orange-500/40 flex items-center justify-between text-xs transition-all"
                  >
                    <span className="truncate text-gray-200">
                      <span className="font-mono text-muted mr-1.5">{idx + 1}.</span>
                      {f.name}
                    </span>
                    <span className="text-[10px] font-bold text-orange-400 font-mono ml-2">
                      {Math.round((f.hotspot_score || 0) * 100)}%
                    </span>
                  </button>
                ))}
              </div>
            </div>

            {/* Top Blast Radius */}
            <div>
              <div className="text-xs font-bold text-rose-400 flex items-center gap-1.5 mb-1.5 uppercase tracking-wide">
                <Flame className="w-3.5 h-3.5" />
                Top Blast Radius (Ta'sir Doirasi)
              </div>
              <div className="space-y-1">
                {rankings.topBlast.map((f, idx) => (
                  <button
                    key={f.id}
                    onClick={() => onSelectNode(f)}
                    className="w-full text-left px-2.5 py-1.5 rounded-lg bg-black/30 hover:bg-rose-500/10 border border-border/50 hover:border-rose-500/40 flex items-center justify-between text-xs transition-all"
                  >
                    <span className="truncate text-gray-200">
                      <span className="font-mono text-muted mr-1.5">{idx + 1}.</span>
                      {f.name}
                    </span>
                    <span className="text-[10px] font-bold text-rose-400 font-mono ml-2">
                      {f.blast_radius} ta bog'liq
                    </span>
                  </button>
                ))}
              </div>
            </div>

            {/* Top Central Hubs */}
            <div>
              <div className="text-xs font-bold text-amber-400 flex items-center gap-1.5 mb-1.5 uppercase tracking-wide">
                <Zap className="w-3.5 h-3.5" />
                Markaziy Tugunlar (Hubs)
              </div>
              <div className="space-y-1">
                {rankings.topGods.map((f, idx) => (
                  <button
                    key={f.id}
                    onClick={() => onSelectNode(f)}
                    className="w-full text-left px-2.5 py-1.5 rounded-lg bg-black/30 hover:bg-amber-500/10 border border-border/50 hover:border-amber-500/40 flex items-center justify-between text-xs transition-all"
                  >
                    <span className="truncate text-gray-200">
                      <span className="font-mono text-muted mr-1.5">{idx + 1}.</span>
                      {f.name}
                    </span>
                    <span className="text-[10px] font-bold text-amber-400 font-mono ml-2">
                      PR: {(f.pagerank || 0).toFixed(3)}
                    </span>
                  </button>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </>
  );
};
