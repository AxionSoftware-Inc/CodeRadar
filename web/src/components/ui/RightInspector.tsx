'use client';

import React, { useState } from 'react';
import { CodeNode, CodeIssue, AppData, getTransitiveDependents, getDirectDependencies } from '@/lib/types';
import { 
  X, 
  Sparkles, 
  Copy, 
  Check, 
  ArrowUpRight, 
  ArrowDownLeft, 
  Flame, 
  AlertTriangle, 
  Layers, 
  FileCode, 
  Cpu, 
  Activity,
  Bot
} from 'lucide-react';

interface RightInspectorProps {
  selectedNode: CodeNode | null;
  selectedSprawl: CodeIssue | null;
  data: AppData | null;
  onClose: () => void;
  onJumpToNode: (nodeId: string) => void;
}

export const RightInspector: React.FC<RightInspectorProps> = ({
  selectedNode,
  selectedSprawl,
  data,
  onClose,
  onJumpToNode,
}) => {
  const [copied, setCopied] = useState(false);
  const [activeTab, setActiveTab] = useState<'METRICS' | 'BLAST' | 'AI'>('METRICS');

  if (!data || (!selectedNode && !selectedSprawl)) {
    return null;
  }

  // Handle Abstraction Sprawl Inspector
  if (selectedSprawl) {
    const memberNodes = selectedSprawl.involved_nodes
      .map((nid) => data.nodes.find((n) => n.id === nid))
      .filter((n): n is CodeNode => Boolean(n));

    const totalLoc = memberNodes.reduce((acc, x) => acc + (x.loc || 0), 0);
    const fnList = memberNodes.map((n) => `- ${n.name} (${n.file_path || n.module})`).join('\n');

    const promptText = `Siz yuqori darajadagi dasturiy ta'minot arxitektorisiz.
Quyidagi ${memberNodes.length} ta funksiya/modul bitta semantik pattern oilasiga mansub bo'lib, ketma-ket nusxalangan yoki takrorlangan (Abstraction Sprawl):

${fnList}

Tahliliy diagnoz:
${selectedSprawl.description}

Arxitektura tavsiyasi:
${selectedSprawl.recommendation}

Vazifa:
1. Ushbu komponentlarni 1 ta umumiy Parametrik Abstraksiya (masalan, Builder, Strategy yoki Registry Factory) yordamida birlashtiring.
2. Har bir funksiya o'rniga bitta toza parametr/konfiguratsiya uzatilishini ko'rsatib, toza TypeScript yoki C++ refaktoring kodini taqdim eting.`;

    const handleCopy = () => {
      navigator.clipboard.writeText(promptText);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };

    return (
      <aside className="absolute top-[74px] right-3 bottom-3 w-[360px] bg-panel/95 backdrop-blur-md border border-border rounded-xl z-20 flex flex-col p-4 shadow-2xl overflow-y-auto">
        <div className="flex justify-between items-center pb-2.5 border-b border-border">
          <span className="text-xs font-bold text-amber-400 flex items-center gap-1.5 uppercase tracking-wide">
            <Sparkles className="w-4 h-4" />
            Abstraksiya Sprawl
          </span>
          <button onClick={onClose} className="text-muted hover:text-white transition-colors p-1">
            <X className="w-4 h-4" />
          </button>
        </div>

        <div className="mt-3">
          <h2 className="text-sm font-bold text-white leading-snug">{selectedSprawl.title}</h2>
          <div className="text-[11px] text-muted mt-1">Turi: ABSTRACTION_SPRAWL (AI Pattern Cloning)</div>
        </div>

        {/* Stat Grid */}
        <div className="grid grid-cols-2 gap-2 my-3">
          <div className="bg-black/40 border border-border rounded-lg p-2.5 text-center">
            <div className="text-lg font-bold text-amber-400">{memberNodes.length}</div>
            <div className="text-[10px] text-muted">A'zo Funksiyalar</div>
          </div>
          <div className="bg-black/40 border border-border rounded-lg p-2.5 text-center">
            <div className="text-lg font-bold text-amber-400">{totalLoc}</div>
            <div className="text-[10px] text-muted">Jami LOC</div>
          </div>
        </div>

        {/* Description */}
        <div className="p-3 bg-black/40 border-l-2 border-amber-400 rounded-lg text-xs text-gray-200 leading-relaxed">
          {selectedSprawl.description}
        </div>

        {/* Member list chips */}
        <div className="mt-3">
          <div className="text-[11px] font-semibold text-muted uppercase tracking-wider mb-1.5">
            Klasterdagi komponentlar ({memberNodes.length})
          </div>
          <div className="flex flex-wrap gap-1 max-h-[130px] overflow-y-auto p-1.5 bg-black/30 rounded-lg border border-border/50">
            {memberNodes.map((n) => (
              <button
                key={n.id}
                onClick={() => onJumpToNode(n.id)}
                className="text-[10px] px-2 py-1 rounded bg-amber-500/10 border border-amber-500/30 text-white hover:border-amber-400 transition-all truncate max-w-[160px]"
              >
                {n.name}
              </button>
            ))}
          </div>
        </div>

        {/* LLM Prompt Card */}
        <div className="mt-4 p-3 bg-amber-500/10 border border-dashed border-amber-500/50 rounded-xl">
          <div className="flex items-center justify-between">
            <span className="text-xs font-bold text-amber-400 flex items-center gap-1.5">
              <Bot className="w-4 h-4" />
              AI Refactoring Blueprint
            </span>
            <button
              onClick={handleCopy}
              className="text-[10px] font-semibold px-2 py-1 rounded bg-amber-500/20 border border-amber-500 text-amber-300 hover:bg-amber-500/30 transition-all flex items-center gap-1"
            >
              {copied ? (
                <>
                  <Check className="w-3 h-3 text-emerald-400" />
                  Nusxalandi!
                </>
              ) : (
                <>
                  <Copy className="w-3 h-3" />
                  Promptni nusxalash
                </>
              )}
            </button>
          </div>
          <div className="text-[10px] text-muted mt-1">
            Qwen2.5-Coder yoki Claude modeliga yuborib avtomatik birlashtirish:
          </div>
          <pre className="mt-2 p-2 bg-black/60 rounded text-[9px] text-gray-300 font-mono max-h-[120px] overflow-y-auto select-text whitespace-pre-wrap leading-relaxed">
            {promptText}
          </pre>
        </div>
      </aside>
    );
  }

  // Handle Regular Node Inspector
  const node = selectedNode!;
  const directDeps = getDirectDependencies(node.id, data.edges);
  const transitiveDependents = Array.from(getTransitiveDependents(node.id, data.edges, 3));

  // Forward & Backward CodeNodes
  const outgoingNodes = directDeps.forward
    .map((id) => data.nodes.find((n) => n.id === id))
    .filter((n): n is CodeNode => Boolean(n));

  const incomingNodes = directDeps.backward
    .map((id) => data.nodes.find((n) => n.id === id))
    .filter((n): n is CodeNode => Boolean(n));

  const blastNodes = transitiveDependents
    .map((id) => data.nodes.find((n) => n.id === id))
    .filter((n): n is CodeNode => Boolean(n));

  // Node Issues
  const relatedIssues = data.issues.filter((iss) => iss.involved_nodes.includes(node.id));

  const instabilityPct = Math.round((node.instability || 0) * 100);

  // AI Prompt for Node Refactoring
  const nodeAiPrompt = `Siz yuqori darajadagi arxitektor va refaktoring mutaxassisisiz.
Quyidagi komponentni tahlil qiling:
Komponent: ${node.name} (${node.file_path || node.module})
LOC: ${node.loc}, Siklomik murakkablik: ${node.complexity}
Afferent Coupling (Ca): ${node.in_degree}, Efferent Coupling (Ce): ${node.out_degree}
Instability (I): ${instabilityPct}%, Blast Radius: ${node.blast_radius} ta bog'liq
Aniqlangan muammolar: ${relatedIssues.map((i) => i.title).join('; ') || "Yo'q"}

Vazifa:
1. Ushbu komponentning beqarorligi va yuqori bog'liqligini kamaytirish uchun toza arxitektura (Dependency Inversion, Facade yoki Event-Driven) yechimini taklif eting.
2. Refaktoring qadamlarini kod misoli bilan ko'rsating.`;

  const handleCopyNodePrompt = () => {
    navigator.clipboard.writeText(nodeAiPrompt);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <aside className="absolute top-[74px] right-3 bottom-3 w-[360px] bg-panel/95 backdrop-blur-md border border-border rounded-xl z-20 flex flex-col p-4 shadow-2xl overflow-hidden">
      {/* Header */}
      <div className="flex justify-between items-start pb-2.5 border-b border-border">
        <div className="truncate pr-2">
          <div className="flex items-center gap-2">
            <span className="text-[10px] uppercase font-mono px-1.5 py-0.2 rounded bg-primary/20 text-primary border border-primary/40">
              {node.type}
            </span>
            <span className="text-[10px] text-muted font-mono">{node.module}</span>
          </div>
          <h2 className="text-sm font-bold text-white truncate mt-1" title={node.name}>
            {node.name}
          </h2>
        </div>
        <button onClick={onClose} className="text-muted hover:text-white transition-colors p-1">
          <X className="w-4 h-4" />
        </button>
      </div>

      {/* Internal Subtabs */}
      <div className="flex border-b border-border/60 py-2 gap-1 text-[11px]">
        <button
          onClick={() => setActiveTab('METRICS')}
          className={`flex-1 py-1 rounded font-semibold transition-all ${
            activeTab === 'METRICS'
              ? 'bg-white/10 text-white font-bold'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
        >
          Metrikalar
        </button>
        <button
          onClick={() => setActiveTab('BLAST')}
          className={`flex-1 py-1 rounded font-semibold transition-all flex items-center justify-center gap-1 ${
            activeTab === 'BLAST'
              ? 'bg-rose-500/20 text-rose-400 font-bold border border-rose-500/40'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
        >
          <Flame className="w-3 h-3 text-rose-400" />
          <span>Blast Radius ({blastNodes.length})</span>
        </button>
        <button
          onClick={() => setActiveTab('AI')}
          className={`flex-1 py-1 rounded font-semibold transition-all flex items-center justify-center gap-1 ${
            activeTab === 'AI'
              ? 'bg-primary/20 text-primary font-bold border border-primary/40'
              : 'text-muted hover:text-white hover:bg-white/5'
          }`}
        >
          <Bot className="w-3 h-3 text-primary" />
          <span>AI Copilot</span>
        </button>
      </div>

      {/* Content Area */}
      <div className="flex-1 overflow-y-auto py-2 space-y-3">
        {activeTab === 'METRICS' && (
          <>
            {/* Stat Cards */}
            <div className="grid grid-cols-2 gap-2">
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-base font-bold text-white">{node.loc}</div>
                <div className="text-[10px] text-muted">Qatorlar soni (LOC)</div>
              </div>
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-base font-bold text-sky-400">{node.complexity}</div>
                <div className="text-[10px] text-muted">Siklomik Murakkablik</div>
              </div>
            </div>

            {/* Martin Instability Gauge */}
            <div className="bg-black/40 border border-border rounded-lg p-3">
              <div className="flex justify-between items-center text-xs mb-1.5">
                <span className="text-muted font-medium">Beqarorlik Darajasi (Instability):</span>
                <span className="font-bold text-white font-mono">{instabilityPct}%</span>
              </div>
              <div className="w-full bg-white/10 h-2 rounded-full overflow-hidden">
                <div
                  className={`h-full transition-all duration-300 ${
                    instabilityPct > 70 ? 'bg-rose-500' : instabilityPct > 40 ? 'bg-amber-400' : 'bg-emerald-400'
                  }`}
                  style={{ width: `${instabilityPct}%` }}
                />
              </div>
              <div className="flex justify-between items-center text-[10px] text-muted mt-2">
                <span>Ca (Kiruvchi): {node.in_degree}</span>
                <span>Ce (Chiquvchi): {node.out_degree}</span>
              </div>
            </div>

            {/* PageRank & Blast Radius Info */}
            <div className="grid grid-cols-2 gap-2">
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-sm font-bold text-amber-400 font-mono">{(node.pagerank || 0).toFixed(4)}</div>
                <div className="text-[10px] text-muted">PageRank (Centrality)</div>
              </div>
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-sm font-bold text-rose-400 font-mono">{node.blast_radius || 0} ta</div>
                <div className="text-[10px] text-muted">Blast Radius ta'siri</div>
              </div>
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-sm font-bold text-orange-400 font-mono">{Math.round((node.hotspot_score || 0) * 100)}%</div>
                <div className="text-[10px] text-muted">Hotspot (Bottleneck)</div>
              </div>
              <div className="bg-black/40 border border-border rounded-lg p-2.5">
                <div className="text-sm font-bold text-red-400 font-mono">{Math.round((node.runtime_heat || 0) * 100)}%</div>
                <div className="text-[10px] text-muted">Runtime Heat (Profiler)</div>
              </div>
            </div>

            {/* Related Issues */}
            {relatedIssues.length > 0 && (
              <div>
                <div className="text-[11px] font-semibold text-rose-400 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                  <AlertTriangle className="w-3.5 h-3.5" />
                  Bog'liq Muammolar ({relatedIssues.length})
                </div>
                <div className="space-y-1">
                  {relatedIssues.map((iss) => (
                    <div key={iss.id} className="p-2 bg-rose-500/10 border border-rose-500/30 rounded-lg text-xs">
                      <div className="font-bold text-white">{iss.title}</div>
                      <div className="text-[10px] text-gray-300 mt-0.5 line-clamp-2">{iss.description}</div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Direct Dependencies */}
            <div className="space-y-2">
              <div>
                <div className="text-[10px] font-semibold text-muted uppercase tracking-wider mb-1 flex items-center gap-1">
                  <ArrowUpRight className="w-3.5 h-3.5 text-sky-400" />
                  U tayanadigan komponentlar ({outgoingNodes.length})
                </div>
                <div className="flex flex-wrap gap-1 max-h-[90px] overflow-y-auto p-1 bg-black/30 rounded border border-border/40">
                  {outgoingNodes.map((n) => (
                    <button
                      key={n.id}
                      onClick={() => onJumpToNode(n.id)}
                      className="text-[10px] px-2 py-0.5 rounded bg-sky-500/10 border border-sky-500/30 text-white hover:border-sky-400 truncate max-w-[150px]"
                    >
                      {n.name}
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <div className="text-[10px] font-semibold text-muted uppercase tracking-wider mb-1 flex items-center gap-1">
                  <ArrowDownLeft className="w-3.5 h-3.5 text-emerald-400" />
                  Unga bog'langan komponentlar ({incomingNodes.length})
                </div>
                <div className="flex flex-wrap gap-1 max-h-[90px] overflow-y-auto p-1 bg-black/30 rounded border border-border/40">
                  {incomingNodes.map((n) => (
                    <button
                      key={n.id}
                      onClick={() => onJumpToNode(n.id)}
                      className="text-[10px] px-2 py-0.5 rounded bg-emerald-500/10 border border-emerald-500/30 text-white hover:border-emerald-400 truncate max-w-[150px]"
                    >
                      {n.name}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          </>
        )}

        {activeTab === 'BLAST' && (
          <div className="space-y-2">
            <div className="p-2.5 bg-rose-500/10 border border-rose-500/30 rounded-lg text-xs text-rose-300 leading-relaxed">
              <strong>Zarba Doirasi (Blast Radius):</strong> Agar siz <span className="underline">{node.name}</span> faylini o'zgartirsangiz yoki sindirsangiz, quyidagi {blastNodes.length} ta komponent bevosita yoki bilvosita zarba oladi!
            </div>
            <div className="space-y-1 max-h-[320px] overflow-y-auto">
              {blastNodes.map((dep, idx) => (
                <button
                  key={dep.id}
                  onClick={() => onJumpToNode(dep.id)}
                  className="w-full text-left px-2.5 py-1.5 rounded-lg bg-black/40 hover:bg-rose-500/10 border border-border hover:border-rose-500/40 flex items-center justify-between text-xs transition-all"
                >
                  <span className="truncate text-gray-200">
                    <span className="font-mono text-muted mr-1.5">{idx + 1}.</span>
                    {dep.name}
                  </span>
                  <span className="text-[10px] text-muted font-mono ml-2">{dep.module}</span>
                </button>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'AI' && (
          <div className="p-3 bg-primary/10 border border-dashed border-primary/40 rounded-xl space-y-2">
            <div className="flex items-center justify-between">
              <span className="text-xs font-bold text-primary flex items-center gap-1.5">
                <Bot className="w-4 h-4" />
                AI Refactoring Prompt
              </span>
              <button
                onClick={handleCopyNodePrompt}
                className="text-[10px] font-semibold px-2 py-1 rounded bg-primary/20 border border-primary text-primary hover:bg-primary/30 transition-all flex items-center gap-1"
              >
                {copied ? (
                  <>
                    <Check className="w-3 h-3 text-emerald-400" />
                    Nusxalandi!
                  </>
                ) : (
                  <>
                    <Copy className="w-3 h-3" />
                    Nusxalash
                  </>
                )}
              </button>
            </div>
            <div className="text-[10px] text-muted leading-relaxed">
              Ushbu promptni Claude, GPT yoki Qwen modeliga yuboring:
            </div>
            <pre className="p-2.5 bg-black/60 rounded text-[9px] text-gray-300 font-mono max-h-[220px] overflow-y-auto select-text whitespace-pre-wrap leading-relaxed">
              {nodeAiPrompt}
            </pre>
          </div>
        )}
      </div>
    </aside>
  );
};
