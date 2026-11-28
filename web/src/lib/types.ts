export type ViewMode = 'CITY' | 'LAYER' | 'GALAXY' | 'MICRO' | 'DSM';

export type LanguageType = 'cpp' | 'dart' | 'cs' | 'python' | 'js' | 'other';

export interface CodeNode {
  id: string;
  name: string;
  type: 'module' | 'file' | 'function';
  module: string;
  file_path: string;
  lang: LanguageType;
  loc: number;
  line_start: number;
  line_end: number;
  complexity: number;
  in_degree: number;
  out_degree: number;
  blast_radius: number;
  instability: number;
  pagerank: number;
  has_cycle: boolean;
  is_god: boolean;
  is_clone: boolean;
  is_sprawl: boolean;
  is_layer_violation: boolean;
  is_orphan: boolean;
}

export interface CodeEdge {
  source: string;
  target: string;
  type: 'imports' | 'calls' | 'contains';
  weight?: number;
}

export interface CodeIssue {
  id: string;
  type: 'CIRCULAR_DEPENDENCY' | 'GOD_OBJECT' | 'CODE_DUPLICATION' | 'ABSTRACTION_SPRAWL' | 'LAYER_VIOLATION' | 'USELESS_WRAPPER' | 'ORPHAN_CODE' | string;
  severity: 'CRITICAL' | 'WARNING' | 'INFO';
  title: string;
  description: string;
  recommendation: string;
  involved_nodes: string[];
  score?: number;
}

export interface AppData {
  health: number;
  nodes: CodeNode[];
  edges: CodeEdge[];
  issues: CodeIssue[];
  datasetName?: string;
}

export const LANG_HEX_COLORS: Record<LanguageType, number> = {
  cpp: 0x60a5fa,      // Neon Sky Blue
  dart: 0x00d2b8,     // Neon Teal / Cyan
  cs: 0xa78bfa,       // Neon Violet
  python: 0xfbbf24,   // Golden Amber
  js: 0xf43f5e,       // Vivid Rose / Ruby
  other: 0x94a3b8,    // Slate
};

export const LANG_TEXT_COLORS: Record<LanguageType, string> = {
  cpp: '#60a5fa',
  dart: '#00d2b8',
  cs: '#a78bfa',
  python: '#fbbf24',
  js: '#f43f5e',
  other: '#94a3b8',
};

export function getLanguage(filename: string): LanguageType {
  if (!filename || !filename.includes('.')) return 'other';
  const ext = filename.substring(filename.lastIndexOf('.') + 1).toLowerCase();
  if (['cpp', 'hpp', 'h', 'cc', 'c', 'cxx'].includes(ext)) return 'cpp';
  if (ext === 'dart') return 'dart';
  if (ext === 'cs') return 'cs';
  if (ext === 'py') return 'python';
  if (['js', 'ts', 'jsx', 'tsx', 'mjs', 'cjs'].includes(ext)) return 'js';
  return 'other';
}

export function getLangColorHex(filename: string): number {
  return LANG_HEX_COLORS[getLanguage(filename)] || 0x94a3b8;
}

// Calculate reverse blast radius nodes (all nodes that transitively depend on target)
export function getTransitiveDependents(targetId: string, edges: CodeEdge[], maxDepth = 4): Set<string> {
  const result = new Set<string>();
  const queue: Array<{ id: string; depth: number }> = [{ id: targetId, depth: 0 }];

  // Build reverse adjacency list: target -> sources
  const revAdj: Record<string, string[]> = {};
  for (const edge of edges) {
    if (edge.type === 'contains') continue;
    if (!revAdj[edge.target]) revAdj[edge.target] = [];
    revAdj[edge.target].push(edge.source);
  }

  while (queue.length > 0) {
    const { id, depth } = queue.shift()!;
    if (depth >= maxDepth) continue;
    const callers = revAdj[id] || [];
    for (const caller of callers) {
      if (!result.has(caller) && caller !== targetId) {
        result.add(caller);
        queue.push({ id: caller, depth: depth + 1 });
      }
    }
  }

  return result;
}

// Direct dependencies of a node
export function getDirectDependencies(nodeId: string, edges: CodeEdge[]): { forward: string[]; backward: string[] } {
  const forward: string[] = [];
  const backward: string[] = [];
  for (const edge of edges) {
    if (edge.type === 'contains') continue;
    if (edge.source === nodeId && !forward.includes(edge.target)) forward.push(edge.target);
    if (edge.target === nodeId && !backward.includes(edge.source)) backward.push(edge.source);
  }
  return { forward, backward };
}
