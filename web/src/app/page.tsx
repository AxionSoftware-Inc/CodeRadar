'use client';

import React, { useState, useEffect, useRef, useCallback } from 'react';
import dynamic from 'next/dynamic';
import { AppData, CodeNode, CodeIssue, ViewMode, SmellFilter, ViewportSettings } from '@/lib/types';
import { unpackBinary } from '@/lib/binaryParser';
import { Header } from '@/components/ui/Header';
import { LeftDrawer } from '@/components/ui/LeftDrawer';
import { RightInspector } from '@/components/ui/RightInspector';
import { HudLegend } from '@/components/ui/HudLegend';
import { DsmMatrix } from '@/components/viewport/DsmMatrix';
import { SettingsModal } from '@/components/ui/SettingsModal';

// Dynamic import ThreeViewport with ssr: false so WebGL only runs client-side
const ThreeViewport = dynamic(
  () => import('@/components/viewport/ThreeViewport').then((mod) => mod.ThreeViewport),
  { ssr: false }
);

export default function HomePage() {
  const [data, setData] = useState<AppData | null>(null);
  const [datasetName, setDatasetName] = useState<string>('BIM-Mobile');
  const [mode, setMode] = useState<ViewMode>('CITY');
  const [selectedLang, setSelectedLang] = useState<string>('ALL');
  const [smellFilter, setSmellFilter] = useState<SmellFilter>('ALL');
  const [settingsOpen, setSettingsOpen] = useState<boolean>(false);
  const [settings, setSettings] = useState<ViewportSettings>({
    groupBy: 'MODULE',
    heightMetric: 'LOC',
    fogEnabled: false,
    showLabels: true,
    lodEnabled: true,
  });
  const [flowActive, setFlowActive] = useState<boolean>(true);
  const [blastSimActive, setBlastSimActive] = useState<boolean>(false);

  const [selectedNode, setSelectedNode] = useState<CodeNode | null>(null);
  const [selectedSprawl, setSelectedSprawl] = useState<CodeIssue | null>(null);

  const [sprawlIndex, setSprawlIndex] = useState(0);
  const [cycleIndex, setCycleIndex] = useState(0);
  const [cloneIndex, setCloneIndex] = useState(0);
  const [godIndex, setGodIndex] = useState(0);

  const viewportRef = useRef<{
    flyTo: (x: number, y: number, z: number, target?: { x: number; y: number; z: number }) => void;
    resetCam: () => void;
    resolveMeshPosition: (nodeId: string) => any;
  } | null>(null);

  // Load binary or JSON data
  const loadDataset = useCallback((type: 'bim' | 'demo') => {
    if (type === 'bim') {
      setDatasetName('BIM-Mobile');
      fetch(`/arch_data.bin?v=${Date.now()}`, { cache: 'no-store' })
        .then((res) => {
          if (!res.ok) throw new Error('Failed to load arch_data.bin');
          return res.arrayBuffer();
        })
        .then((buf) => {
          const parsed = unpackBinary(buf);
          setData(parsed);
          setSelectedNode(null);
          setSelectedSprawl(null);
        })
        .catch((err) => {
          console.warn('Binary load error, trying JSON:', err);
          fetch('/bim_analysis.json')
            .then((res) => res.json())
            .then((json) => {
              const nodes: CodeNode[] = (json.nodes || []).map((n: any) => ({
                ...n,
                lang: n.name?.endsWith('.dart')
                  ? 'dart'
                  : n.name?.endsWith('.cpp') || n.name?.endsWith('.hpp') || n.name?.endsWith('.h')
                  ? 'cpp'
                  : n.name?.endsWith('.cs')
                  ? 'cs'
                  : 'other',
                has_cycle: false,
                is_god: false,
                is_clone: false,
                is_sprawl: false,
                is_layer_violation: false,
                is_orphan: false,
              }));
              setData({
                health: json.summary?.health_score || 85,
                nodes,
                edges: json.edges || [],
                issues: json.issues || [],
              });
            })
            .catch((jsonErr) => console.error('Both binary and JSON load failed:', jsonErr));
        });
    } else {
      setDatasetName('Demo Project');
      fetch('/demo_analysis.json')
        .then((res) => res.json())
        .then((json) => {
          setData(json);
          setSelectedNode(null);
          setSelectedSprawl(null);
        })
        .catch(() => {
          fetch('/analysis_result.json')
            .then((res) => res.json())
            .then((json) => setData(json))
            .catch((err) => console.error('Demo load failed:', err));
        });
    }
  }, []);

  // Mount effect
  useEffect(() => {
    loadDataset('bim');
  }, [loadDataset]);

  // Keyboard Hotkeys
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Don't trigger when typing in inputs
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) return;

      if (e.key === '1') setMode('CITY');
      else if (e.key === '2') setMode('LAYER');
      else if (e.key === '3') setMode('GALAXY');
      else if (e.key === '4') setMode('MICRO');
      else if (e.key === '5') setMode('DSM');
      else if (e.key === ' ' || e.code === 'Space') {
        e.preventDefault();
        setFlowActive((prev) => !prev);
      } else if (e.key === 'b' || e.key === 'B') {
        setBlastSimActive((prev) => !prev);
      } else if (e.key === 'Escape') {
        setSelectedNode(null);
        setSelectedSprawl(null);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const handleSelectNode = (node: CodeNode) => {
    setSelectedSprawl(null);
    setSelectedNode(node);
  };

  const handleSelectIssue = (iss: CodeIssue) => {
    if (iss.type === 'ABSTRACTION_SPRAWL') {
      if (mode !== 'MICRO' && mode !== 'DSM') setMode('MICRO');
      setSelectedNode(null);
      setSelectedSprawl(iss);
      return;
    }

    if (iss.type === 'CODE_DUPLICATION') {
      if (mode !== 'MICRO' && mode !== 'DSM') setMode('MICRO');
    }

    if (iss.involved_nodes.length > 0 && data) {
      const firstNode = data.nodes.find((n) => n.id === iss.involved_nodes[0]);
      if (firstNode) {
        setSelectedSprawl(null);
        setSelectedNode(firstNode);
      }
    }
  };

  const handleNextSprawl = () => {
    if (!data) return;
    const sprawls = data.issues.filter((i) => i.type === 'ABSTRACTION_SPRAWL');
    if (sprawls.length === 0) return;
    const s = sprawls[sprawlIndex % sprawls.length];
    setSprawlIndex((prev) => prev + 1);
    handleSelectIssue(s);
  };

  const handleNextCycle = () => {
    if (!data) return;
    const cycles = data.issues.filter((i) => i.type === 'CIRCULAR_DEPENDENCY');
    if (cycles.length === 0) return;
    const c = cycles[cycleIndex % cycles.length];
    setCycleIndex((prev) => prev + 1);
    handleSelectIssue(c);
  };

  const handleNextClone = () => {
    if (!data) return;
    const clones = data.issues.filter((i) => i.type === 'CODE_DUPLICATION');
    if (clones.length === 0) return;
    const cl = clones[cloneIndex % clones.length];
    setCloneIndex((prev) => prev + 1);
    handleSelectIssue(cl);
  };

  const handleNextGod = () => {
    if (!data) return;
    const gods = data.nodes.filter((n) => n.is_god || (n.pagerank || 0) > 0.05);
    if (gods.length === 0) return;
    const g = gods[godIndex % gods.length];
    setGodIndex((prev) => prev + 1);
    handleSelectNode(g);
  };

  const handleFileUpload = (file: File) => {
    const reader = new FileReader();
    reader.onload = (e) => {
      const res = e.target?.result;
      if (res instanceof ArrayBuffer) {
        try {
          const parsed = unpackBinary(res);
          setData(parsed);
          setDatasetName(file.name);
          setSelectedNode(null);
          setSelectedSprawl(null);
          viewportRef.current?.resetCam();
        } catch (err) {
          console.warn('Binary unpack failed, trying JSON:', err);
          try {
            const dec = new TextDecoder('utf-8');
            const str = dec.decode(res);
            const json = JSON.parse(str);
            setData(json);
            setDatasetName(file.name);
          } catch (jsonErr) {
            alert('Faylni yuklashda xatolik: ' + jsonErr);
          }
        }
      }
    };
    reader.readAsArrayBuffer(file);
  };

  return (
    <main className="relative w-screen h-screen overflow-hidden bg-bg">
      {/* Top Header Navigation */}
      <Header
        healthScore={data?.health ?? 100}
        currentMode={mode}
        onModeChange={(newMode) => {
          setMode(newMode);
          setSelectedNode(null);
          setSelectedSprawl(null);
        }}
        flowActive={flowActive}
        onToggleFlow={() => setFlowActive((prev) => !prev)}
        blastSimActive={blastSimActive}
        onToggleBlastSim={() => setBlastSimActive((prev) => !prev)}
        smellFilter={smellFilter}
        onSmellFilterChange={setSmellFilter}
        onNextSprawl={handleNextSprawl}
        onNextCycle={handleNextCycle}
        onNextClone={handleNextClone}
        onNextGod={handleNextGod}
        onResetCam={() => viewportRef.current?.resetCam()}
        onOpenSettings={() => setSettingsOpen(true)}
        onFileUpload={handleFileUpload}
        datasetName={datasetName}
        onSelectDataset={loadDataset}
      />

      {/* Left Tree & Issues Drawer */}
      <LeftDrawer
        data={data}
        selectedLang={selectedLang}
        onSelectLang={setSelectedLang}
        onSelectNode={handleSelectNode}
        onSelectIssue={handleSelectIssue}
        selectedNodeId={selectedNode?.id}
      />

      {/* Viewport: Either 3D WebGL Scene or 2D DSM Matrix */}
      {mode === 'DSM' ? (
        <DsmMatrix data={data} onSelectNode={handleSelectNode} />
      ) : (
        <ThreeViewport
          data={data}
          mode={mode}
          selectedLang={selectedLang}
          smellFilter={smellFilter}
          settings={settings}
          flowActive={flowActive}
          selectedNode={selectedNode}
          selectedSprawl={selectedSprawl}
          blastSimActive={blastSimActive}
          onSelectNode={handleSelectNode}
          viewportRef={viewportRef}
        />
      )}

      {/* Right Object & Sprawl LLM Inspector */}
      <RightInspector
        selectedNode={selectedNode}
        selectedSprawl={selectedSprawl}
        data={data}
        onClose={() => {
          setSelectedNode(null);
          setSelectedSprawl(null);
        }}
        onJumpToNode={(nodeId) => {
          if (!data) return;
          const target = data.nodes.find((n) => n.id === nodeId);
          if (target) handleSelectNode(target);
        }}
      />

      {/* Bottom Center Floating HUD Legend (only in 3D modes) */}
      {mode !== 'DSM' && <HudLegend />}

      {/* Viewport Settings Modal */}
      <SettingsModal
        isOpen={settingsOpen}
        settings={settings}
        onChange={setSettings}
        onClose={() => setSettingsOpen(false)}
      />
    </main>
  );
}
