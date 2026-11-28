'use client';

import React, { useState, useMemo } from 'react';
import { AppData, CodeNode } from '@/lib/types';
import { Layers, AlertTriangle, ArrowRight, Info, ZoomIn, ZoomOut } from 'lucide-react';

interface DsmMatrixProps {
  data: AppData | null;
  onSelectNode: (node: CodeNode) => void;
}

export const DsmMatrix: React.FC<DsmMatrixProps> = ({ data, onSelectNode }) => {
  const [hoveredCell, setHoveredCell] = useState<{ row: string; col: string; count: number; isCycle: boolean } | null>(null);
  const [level, setLevel] = useState<'MODULE' | 'FILE'>('MODULE');
  const [search, setSearch] = useState('');
  const [zoom, setZoom] = useState(1);

  // Calculate modules or top files list
  const matrixData = useMemo(() => {
    if (!data) return { items: [], matrix: new Map<string, number>(), cyclePairs: new Set<string>() };

    let items: Array<{ id: string; name: string; loc: number; issueCount: number }> = [];

    if (level === 'MODULE') {
      const moduleMap = new Map<string, { loc: number; issueCount: number }>();
      data.nodes.forEach((n) => {
        const m = n.module || 'root';
        const cur = moduleMap.get(m) || { loc: 0, issueCount: 0 };
        cur.loc += n.loc || 0;
        if (n.has_cycle || n.is_god || n.is_sprawl) cur.issueCount++;
        moduleMap.set(m, cur);
      });

      items = Array.from(moduleMap.entries()).map(([name, stat]) => ({
        id: name,
        name,
        loc: stat.loc,
        issueCount: stat.issueCount,
      }));
    } else {
      // Top 40 files by LOC / Complexity
      const files = data.nodes
        .filter((n) => n.type === 'file')
        .sort((a, b) => (b.loc || 0) + b.complexity * 10 - ((a.loc || 0) + a.complexity * 10))
        .slice(0, 45);

      items = files.map((f) => ({
        id: f.id,
        name: f.name,
        loc: f.loc || 0,
        issueCount: f.has_cycle || f.is_god || f.is_sprawl ? 1 : 0,
      }));
    }

    if (search) {
      const q = search.toLowerCase();
      items = items.filter((item) => item.name.toLowerCase().includes(q));
    }

    // Build Adjacency Weights
    const itemIds = new Set(items.map((i) => i.id));
    const matrix = new Map<string, number>();

    data.edges.forEach((e) => {
      if (e.type === 'contains') return;
      let srcKey = '';
      let tgtKey = '';

      if (level === 'MODULE') {
        const sNode = data.nodes.find((n) => n.id === e.source);
        const tNode = data.nodes.find((n) => n.id === e.target);
        if (sNode && tNode) {
          srcKey = sNode.module || 'root';
          tgtKey = tNode.module || 'root';
        }
      } else {
        srcKey = e.source;
        tgtKey = e.target;
      }

      if (srcKey && tgtKey && srcKey !== tgtKey && itemIds.has(srcKey) && itemIds.has(tgtKey)) {
        const key = `${srcKey}->${tgtKey}`;
        matrix.set(key, (matrix.get(key) || 0) + (e.weight || 1));
      }
    });

    // Detect 2-way Circular Dependencies (A -> B && B -> A)
    const cyclePairs = new Set<string>();
    for (const [key] of matrix.entries()) {
      const [src, tgt] = key.split('->');
      const revKey = `${tgt}->${src}`;
      if (matrix.has(revKey)) {
        cyclePairs.add(key);
        cyclePairs.add(revKey);
      }
    }

    return { items, matrix, cyclePairs };
  }, [data, level, search]);

  if (!data) return null;

  return (
    <div className="relative w-full h-full bg-[#07090e] flex flex-col overflow-hidden select-none pt-[74px] pl-[356px]">
      {/* Top Matrix Controls */}
      <div className="h-12 border-b border-border bg-panel/60 backdrop-blur-md px-4 flex items-center justify-between z-10">
        <div className="flex items-center gap-3">
          <span className="text-xs font-bold text-white flex items-center gap-1.5 uppercase tracking-wide">
            <Layers className="w-4 h-4 text-primary" />
            DSM (Qator ➔ Ustun)
          </span>
          <div className="flex items-center gap-1 bg-black/40 p-0.5 rounded border border-border">
            <button
              onClick={() => setLevel('MODULE')}
              className={`text-[11px] px-2.5 py-1 rounded font-medium transition-all ${
                level === 'MODULE' ? 'bg-primary/20 text-primary border border-primary/40 font-bold' : 'text-muted hover:text-white'
              }`}
            >
              Modullar ({matrixData.items.length})
            </button>
            <button
              onClick={() => setLevel('FILE')}
              className={`text-[11px] px-2.5 py-1 rounded font-medium transition-all ${
                level === 'FILE' ? 'bg-primary/20 text-primary border border-primary/40 font-bold' : 'text-muted hover:text-white'
              }`}
            >
              Eng Muhim Fayllar (Top 45)
            </button>
          </div>
        </div>

        <div className="flex items-center gap-3">
          <input
            type="text"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Matritsada filtrlash..."
            className="w-48 bg-black/40 border border-border rounded px-2.5 py-1 text-xs text-white placeholder-muted focus:outline-none focus:border-primary"
          />

          <div className="flex items-center gap-1 bg-black/40 border border-border rounded px-1.5 py-0.5">
            <button
              onClick={() => setZoom((z) => Math.max(0.6, z - 0.1))}
              className="p-1 text-muted hover:text-white"
              title="Kichraytirish"
            >
              <ZoomOut className="w-3.5 h-3.5" />
            </button>
            <span className="text-[10px] text-muted font-mono w-8 text-center">{Math.round(zoom * 100)}%</span>
            <button
              onClick={() => setZoom((z) => Math.min(1.8, z + 0.1))}
              className="p-1 text-muted hover:text-white"
              title="Kattalashtirish"
            >
              <ZoomIn className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      </div>

      {/* Main Matrix Grid Container */}
      <div className="flex-1 overflow-auto p-6 relative">
        <div
          className="inline-block transition-transform duration-75 origin-top-left"
          style={{ transform: `scale(${zoom})` }}
        >
          {/* Header Row (Column Names) */}
          <div className="flex pl-48 pb-2">
            {matrixData.items.map((colItem, cIdx) => (
              <div
                key={colItem.id}
                className="w-8 h-32 flex items-end justify-center px-0.5 pb-1 flex-shrink-0"
              >
                <div
                  className="text-[10px] font-mono text-muted transform -rotate-45 origin-bottom-left truncate max-w-[120px] hover:text-primary transition-colors cursor-default"
                  title={colItem.name}
                >
                  {cIdx + 1}. {colItem.name}
                </div>
              </div>
            ))}
          </div>

          {/* Matrix Body (Rows) */}
          <div className="space-y-1">
            {matrixData.items.map((rowItem, rIdx) => (
              <div key={rowItem.id} className="flex items-center group">
                {/* Row Header */}
                <div
                  className="w-48 text-right pr-3 truncate flex items-center justify-end gap-1.5 flex-shrink-0 cursor-pointer hover:text-primary"
                  title={`${rowItem.name} (${rowItem.loc} LOC)`}
                  onClick={() => {
                    if (level === 'FILE') {
                      const n = data.nodes.find((x) => x.id === rowItem.id);
                      if (n) onSelectNode(n);
                    }
                  }}
                >
                  {rowItem.issueCount > 0 && <span className="w-1.5 h-1.5 rounded-full bg-rose-500 animate-pulse" />}
                  <span className="text-[10px] font-mono text-muted group-hover:text-white truncate">
                    {rIdx + 1}. {rowItem.name}
                  </span>
                </div>

                {/* Cells */}
                <div className="flex gap-1">
                  {matrixData.items.map((colItem, cIdx) => {
                    const isDiagonal = rIdx === cIdx;
                    const key = `${rowItem.id}->${colItem.id}`;
                    const weight = matrixData.matrix.get(key) || 0;
                    const isCycle = matrixData.cyclePairs.has(key);

                    let cellBg = 'bg-white/[0.03] border-white/[0.04]';
                    let textCol = 'text-transparent';

                    if (isDiagonal) {
                      cellBg = 'bg-white/10 border-white/20';
                    } else if (isCycle && weight > 0) {
                      cellBg = 'bg-rose-500/30 border-rose-500/80 shadow-[0_0_8px_rgba(244,63,94,0.4)] animate-pulse';
                      textCol = 'text-rose-300 font-bold';
                    } else if (weight > 0) {
                      cellBg = rIdx > cIdx
                        ? 'bg-amber-500/20 border-amber-500/40 text-amber-300' // Upper triangle or lower triangle
                        : 'bg-primary/20 border-primary/40 text-sky-300';
                      textCol = 'text-white font-semibold';
                    }

                    return (
                      <div
                        key={colItem.id}
                        onMouseEnter={() => {
                          if (weight > 0 || isDiagonal) {
                            setHoveredCell({
                              row: rowItem.name,
                              col: colItem.name,
                              count: weight,
                              isCycle,
                            });
                          }
                        }}
                        onMouseLeave={() => setHoveredCell(null)}
                        className={`w-8 h-8 rounded border flex items-center justify-center text-[10px] font-mono cursor-pointer transition-all hover:scale-110 hover:z-20 ${cellBg} ${textCol}`}
                      >
                        {isDiagonal ? '●' : weight > 0 ? weight : ''}
                      </div>
                    );
                  })}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Floating Info Tooltip */}
      {hoveredCell && (
        <div className="absolute bottom-6 right-6 p-3 bg-panel/95 border border-primary/50 rounded-lg shadow-2xl backdrop-blur-md z-30 max-w-sm">
          <div className="text-xs font-bold text-white flex items-center gap-1.5">
            <span className="text-sky-400">{hoveredCell.row}</span>
            <ArrowRight className="w-3.5 h-3.5 text-muted" />
            <span className="text-sky-400">{hoveredCell.col}</span>
          </div>
          <div className="text-[11px] text-gray-300 mt-1">
            Bog'lanishlar soni: <strong className="text-white">{hoveredCell.count} ta chaqiruv/import</strong>
          </div>
          {hoveredCell.isCycle && (
            <div className="mt-1.5 p-1.5 bg-rose-500/20 border border-rose-500/50 rounded text-[10px] text-rose-300 flex items-center gap-1 font-bold">
              <AlertTriangle className="w-3.5 h-3.5 text-rose-400 flex-shrink-0" />
              Ikki tomonlama aylanma bog'liqlik aniqlandi!
            </div>
          )}
        </div>
      )}

      {/* Footer Legend */}
      <div className="h-8 border-t border-border bg-panel/40 px-4 flex items-center justify-between text-[11px] text-muted">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-1.5">
            <span className="w-2.5 h-2.5 rounded bg-white/20 border border-white/40" />
            <span>Bosh diagonal (o'zi)</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="w-2.5 h-2.5 rounded bg-primary/20 border border-primary/50" />
            <span>Oddiy bog'liqlik</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="w-2.5 h-2.5 rounded bg-rose-500/40 border border-rose-500" />
            <span className="text-rose-400 font-semibold">Aylanma sikl (Siklik kesishma)</span>
          </div>
        </div>
        <div>
          Diagonaldan chetdagi qizil kataklar zudlik bilan refaktoring talab qiluvchi aylanma nuqtalardir.
        </div>
      </div>
    </div>
  );
};
