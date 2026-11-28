'use client';

import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { 
  AppData, 
  CodeNode, 
  CodeIssue, 
  ViewMode, 
  SmellFilter,
  ViewportSettings,
  getLangColorHex, 
  getTransitiveDependents 
} from '@/lib/types';

interface ThreeViewportProps {
  data: AppData | null;
  mode: ViewMode;
  selectedLang: string;
  smellFilter: SmellFilter;
  settings: ViewportSettings;
  flowActive: boolean;
  selectedNode: CodeNode | null;
  selectedSprawl: CodeIssue | null;
  blastSimActive: boolean;
  onSelectNode: (node: CodeNode) => void;
  viewportRef: React.MutableRefObject<{
    flyTo: (x: number, y: number, z: number, target?: { x: number; y: number; z: number }) => void;
    resetCam: () => void;
    resolveMeshPosition: (nodeId: string) => THREE.Vector3 | null;
  } | null>;
}

export const ThreeViewport: React.FC<ThreeViewportProps> = ({
  data,
  mode,
  selectedLang,
  smellFilter,
  settings,
  flowActive,
  selectedNode,
  selectedSprawl,
  blastSimActive,
  onSelectNode,
  viewportRef,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const tooltipRef = useRef<HTMLDivElement>(null);

  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.PerspectiveCamera | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);

  const cityGroupRef = useRef<THREE.Group>(new THREE.Group());
  const beamGroupRef = useRef<THREE.Group>(new THREE.Group());
  const selectBeamGroupRef = useRef<THREE.Group>(new THREE.Group());
  const particlesGroupRef = useRef<THREE.Group>(new THREE.Group());
  const highlightRingRef = useRef<THREE.Mesh | null>(null);

  const clickableObjectsRef = useRef<THREE.Mesh[]>([]);
  const meshMapRef = useRef<Map<string, THREE.Mesh>>(new Map());
  const flowCurvesRef = useRef<Array<{ curve: THREE.QuadraticBezierCurve3; color: number; progress: number }>>([]);
  const flyAnimRef = useRef<number | null>(null);
  const lodEnabledRef = useRef<boolean>(settings.lodEnabled);
  lodEnabledRef.current = settings.lodEnabled;

  // Clear three group helper
  const clearGroup = (group: THREE.Group) => {
    while (group.children.length > 0) {
      const obj = group.children[0];
      group.remove(obj);
      if ((obj as THREE.Mesh).geometry) (obj as THREE.Mesh).geometry.dispose();
      if ((obj as THREE.Mesh).material) {
        const mat = (obj as THREE.Mesh).material;
        const disposeMat = (m: THREE.Material) => {
          if ((m as THREE.SpriteMaterial).map) (m as THREE.SpriteMaterial).map?.dispose();
          m.dispose();
        };
        if (Array.isArray(mat)) mat.forEach(disposeMat);
        else disposeMat(mat);
      }
    }
  };

  // Create crisp 3D label sprite for module islands
  const createModuleLabel = (text: string): THREE.Sprite | null => {
    const canvas = document.createElement('canvas');
    canvas.width = 512;
    canvas.height = 96;
    const ctx = canvas.getContext('2d');
    if (!ctx) return null;

    ctx.fillStyle = 'rgba(10, 15, 28, 0.82)';
    ctx.strokeStyle = 'rgba(56, 189, 248, 0.55)';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.roundRect(8, 12, 496, 72, 14);
    ctx.fill();
    ctx.stroke();

    ctx.font = 'bold 34px monospace';
    ctx.fillStyle = '#e2e8f0';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    const label = text.length > 26 ? '...' + text.slice(-23) : text;
    ctx.fillText(label, 256, 48);

    const tex = new THREE.CanvasTexture(canvas);
    const mat = new THREE.SpriteMaterial({ map: tex, transparent: true, depthWrite: false });
    const sprite = new THREE.Sprite(mat);
    sprite.scale.set(210, 40, 1);
    return sprite;
  };

  // Draw smooth quadratic bezier beam
  const drawArc = (
    p1: THREE.Vector3,
    p2: THREE.Vector3,
    hex: number,
    targetGroup: THREE.Group,
    opacity = 0.8,
    registerFlow = false,
    liftFactor = 0.35
  ) => {
    if (!p1 || !p2 || p1.distanceTo(p2) < 2) return null;
    const mx = (p1.x + p2.x) / 2;
    const mz = (p1.z + p2.z) / 2;
    const dist = Math.hypot(p2.x - p1.x, p2.z - p1.z);
    const my = Math.max(p1.y, p2.y) + Math.min(320, dist * liftFactor + 30);

    const curve = new THREE.QuadraticBezierCurve3(
      new THREE.Vector3(p1.x, p1.y, p1.z),
      new THREE.Vector3(mx, my, mz),
      new THREE.Vector3(p2.x, p2.y, p2.z)
    );
    const line = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints(curve.getPoints(24)),
      new THREE.LineBasicMaterial({ color: hex, transparent: true, opacity })
    );
    targetGroup.add(line);

    if (registerFlow) {
      flowCurvesRef.current.push({ curve, color: hex, progress: Math.random() });
    }
    return line;
  };

  const resolveMeshForNode = (nodeId: string): THREE.Mesh | null => {
    if (meshMapRef.current.has(nodeId)) return meshMapRef.current.get(nodeId)!;
    if (!data) return null;
    const n = data.nodes.find((x) => x.id === nodeId);
    if (n && n.type === 'function') {
      const fId = 'file:' + n.file_path;
      if (meshMapRef.current.has(fId)) return meshMapRef.current.get(fId)!;
    }
    return null;
  };

  const cancelFlyTo = () => {
    if (flyAnimRef.current !== null) {
      cancelAnimationFrame(flyAnimRef.current);
      flyAnimRef.current = null;
    }
  };

  // Smooth camera interpolation
  const flyTo = (x: number, y: number, z: number, target?: { x: number; y: number; z: number }) => {
    if (!cameraRef.current || !controlsRef.current) return;
    cancelFlyTo();
    const sPos = cameraRef.current.position.clone();
    const tPos = new THREE.Vector3(x, y, z);
    const sTar = controlsRef.current.target.clone();
    const tTar = target ? new THREE.Vector3(target.x, target.y, target.z) : new THREE.Vector3(0, 0, 0);

    let p = 0;
    const f = () => {
      p += 0.055;
      cameraRef.current?.position.lerpVectors(sPos, tPos, Math.min(1, p));
      controlsRef.current?.target.lerpVectors(sTar, tTar, Math.min(1, p));
      if (p < 1) {
        flyAnimRef.current = requestAnimationFrame(f);
      } else {
        flyAnimRef.current = null;
      }
    };
    flyAnimRef.current = requestAnimationFrame(f);
  };

  const resetCam = () => {
    clearGroup(selectBeamGroupRef.current);
    flyTo(0, 1500, 2400, { x: 0, y: 0, z: 0 });
  };

  // Expose methods to parent
  useEffect(() => {
    viewportRef.current = {
      flyTo,
      resetCam,
      resolveMeshPosition: (nodeId: string) => {
        const m = resolveMeshForNode(nodeId);
        return m ? m.position.clone() : null;
      },
    };
  }, [data]);

  // Initialize Three.js scene once
  useEffect(() => {
    if (!containerRef.current) return;
    const width = containerRef.current.clientWidth;
    const height = containerRef.current.clientHeight;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x06080d);
    scene.fog = settings.fogEnabled ? new THREE.Fog(0x06080d, 6000, 22000) : null;
    sceneRef.current = scene;

    const camera = new THREE.PerspectiveCamera(45, width / height, 1, 50000);
    camera.position.set(0, 1500, 2400);
    cameraRef.current = camera;

    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(width, height);
    renderer.shadowMap.enabled = false;
    containerRef.current.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.maxPolarAngle = Math.PI / 2 + 0.04;
    controls.minDistance = 30;
    controls.maxDistance = 16000;
    controls.addEventListener('start', cancelFlyTo);
    controlsRef.current = controls;

    // Lighting: balanced ambient + hemisphere + dual directional so zoomed-out city stays crisp & bright
    const amb = new THREE.AmbientLight(0xffffff, 1.15);
    scene.add(amb);
    const hemi = new THREE.HemisphereLight(0xbae6fd, 0x1e293b, 0.85);
    scene.add(hemi);
    const dir = new THREE.DirectionalLight(0xffffff, 1.35);
    dir.position.set(1200, 2800, 1500);
    scene.add(dir);
    const dir2 = new THREE.DirectionalLight(0x38bdf8, 0.75);
    dir2.position.set(-1500, 2000, -1500);
    scene.add(dir2);

    scene.add(cityGroupRef.current);
    scene.add(beamGroupRef.current);
    scene.add(selectBeamGroupRef.current);
    scene.add(particlesGroupRef.current);

    // Modern cyber grid
    const grid = new THREE.GridHelper(16000, 160, 0x1e293b, 0x0f172a);
    grid.position.y = -1;
    scene.add(grid);

    // Target highlight ring
    const ringGeom = new THREE.RingGeometry(18, 22, 32);
    ringGeom.rotateX(-Math.PI / 2);
    const ringMat = new THREE.MeshBasicMaterial({ color: 0x38bdf8, side: THREE.DoubleSide, transparent: true, opacity: 0 });
    const highlightRing = new THREE.Mesh(ringGeom, ringMat);
    scene.add(highlightRing);
    highlightRingRef.current = highlightRing;

    // Flow particles
    const PARTICLE_COUNT = 450;
    const pGeom = new THREE.BufferGeometry();
    const pPos = new Float32Array(PARTICLE_COUNT * 3);
    const pCol = new Float32Array(PARTICLE_COUNT * 3);
    pGeom.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
    pGeom.setAttribute('color', new THREE.BufferAttribute(pCol, 3));

    const pMat = new THREE.PointsMaterial({
      size: 8,
      vertexColors: true,
      transparent: true,
      opacity: 0.95,
      blending: THREE.AdditiveBlending,
    });
    const particlesMesh = new THREE.Points(pGeom, pMat);
    particlesGroupRef.current.add(particlesMesh);

    // Raycaster for hover and click
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    const onPointerMove = (e: MouseEvent) => {
      const rect = renderer.domElement.getBoundingClientRect();
      mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const hits = raycaster.intersectObjects(clickableObjectsRef.current);

      if (hits.length > 0 && hits[0].object.userData) {
        const u = hits[0].object.userData as CodeNode;
        if (tooltipRef.current) {
          tooltipRef.current.style.display = 'block';
          tooltipRef.current.style.left = `${e.clientX + 16}px`;
          tooltipRef.current.style.top = `${e.clientY + 16}px`;

          const instabilityPct = Math.round((u.instability || 0) * 100);
          const prScore = (u.pagerank || 0).toFixed(4);
          const hotspotPct = Math.round((u.hotspot_score || 0) * 100);
          const heatPct = Math.round((u.runtime_heat || 0) * 100);

          tooltipRef.current.innerHTML = `
            <div class="font-bold text-white flex items-center justify-between gap-3 border-b border-white/10 pb-1">
              <span>${u.name}</span>
              <span class="text-[9px] uppercase px-1.5 py-0.2 rounded bg-white/10 text-primary font-mono">${u.lang}</span>
            </div>
            <div class="text-[10px] text-gray-300 mt-1">
              <span class="text-gray-400">Modul:</span> ${u.module || 'root'}<br/>
              <span class="text-gray-400">LOC:</span> ${u.loc || 0} • <span class="text-gray-400">Murakkablik:</span> ${u.complexity || 1}<br/>
              <span class="text-gray-400">Blast Radius:</span> <strong class="text-rose-400">${u.blast_radius || 0} ta bog'liq</strong><br/>
              <span class="text-gray-400">Instability (I):</span> ${instabilityPct}% • <span class="text-gray-400">PageRank:</span> ${prScore}<br/>
              <span class="text-gray-400">Hotspot:</span> <strong class="text-orange-400">${hotspotPct}%</strong>${heatPct > 0 ? ` • <span class="text-red-400">Heat: ${heatPct}%</span>` : ''}
            </div>
            ${u.is_god ? '<div class="text-[10px] text-amber-400 font-bold mt-1">⚡ Markaziy Tugun (Hub) / Yuqori Bog\'liqlik</div>' : ''}
            ${u.is_sprawl ? '<div class="text-[10px] text-yellow-300 font-bold mt-1">✨ Abstraksiya Sprawl Klasteri</div>' : ''}
            ${u.is_clone ? '<div class="text-[10px] text-purple-400 font-bold mt-1">🧬 Semantik Dublikat (Type-3/4)</div>' : ''}
            ${u.has_cycle ? '<div class="text-[10px] text-red-400 font-bold mt-1">⚠️ Aylanma Bog\'liqlik (Cycle)</div>' : ''}
          `;
        }
        renderer.domElement.style.cursor = 'pointer';
      } else {
        if (tooltipRef.current) tooltipRef.current.style.display = 'none';
        renderer.domElement.style.cursor = 'default';
      }
    };

    let pointerDownPos: { x: number; y: number; button: number } | null = null;

    const onPointerDown = (e: PointerEvent) => {
      pointerDownPos = { x: e.clientX, y: e.clientY, button: e.button };
    };

    const onPointerUp = (e: PointerEvent) => {
      if (!pointerDownPos || pointerDownPos.button !== 0 || e.button !== 0) {
        pointerDownPos = null;
        return;
      }
      const moved = Math.hypot(e.clientX - pointerDownPos.x, e.clientY - pointerDownPos.y);
      pointerDownPos = null;
      if (moved >= 5) return; // Ignore orbit/pan drags; only select on genuine click

      const rect = renderer.domElement.getBoundingClientRect();
      mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const hits = raycaster.intersectObjects(clickableObjectsRef.current);
      if (hits.length > 0 && hits[0].object.userData) {
        onSelectNode(hits[0].object.userData as CodeNode);
      }
    };

    const onResize = () => {
      if (!containerRef.current) return;
      const w = containerRef.current.clientWidth;
      const h = containerRef.current.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };

    window.addEventListener('resize', onResize);
    renderer.domElement.addEventListener('pointermove', onPointerMove);
    renderer.domElement.addEventListener('pointerdown', onPointerDown);
    renderer.domElement.addEventListener('pointerup', onPointerUp);

    // Animation Loop
    let animId: number;
    let clock = new THREE.Clock();

    const animate = () => {
      animId = requestAnimationFrame(animate);
      const elapsedTime = clock.getElapsedTime();
      controls.update();

      // Semantic LOD: hide dense background dependency arcs when zoomed far out
      if (beamGroupRef.current) {
        beamGroupRef.current.visible = !lodEnabledRef.current || camera.position.y < 2600;
      }

      // Pulsate highlight ring
      if (highlightRingRef.current && (highlightRingRef.current.material as THREE.MeshBasicMaterial).opacity > 0) {
        const s = 1 + Math.sin(elapsedTime * 6) * 0.08;
        highlightRingRef.current.scale.set(s, s, s);
      }

      // Animate flow particles
      if (flowCurvesRef.current.length > 0 && particlesGroupRef.current.visible) {
        const positions = (particlesMesh.geometry.attributes.position as THREE.BufferAttribute).array as Float32Array;
        const colors = (particlesMesh.geometry.attributes.color as THREE.BufferAttribute).array as Float32Array;

        for (let i = 0; i < PARTICLE_COUNT; i++) {
          const item = flowCurvesRef.current[i % flowCurvesRef.current.length];
          item.progress = (item.progress + 0.007) % 1.0;
          const pt = item.curve.getPoint(item.progress);
          positions[i * 3] = pt.x;
          positions[i * 3 + 1] = pt.y;
          positions[i * 3 + 2] = pt.z;

          const c = new THREE.Color(item.color);
          colors[i * 3] = c.r;
          colors[i * 3 + 1] = c.g;
          colors[i * 3 + 2] = c.b;
        }
        particlesMesh.geometry.attributes.position.needsUpdate = true;
        particlesMesh.geometry.attributes.color.needsUpdate = true;
      }

      renderer.render(scene, camera);
    };
    animate();

    return () => {
      cancelAnimationFrame(animId);
      cancelFlyTo();
      window.removeEventListener('resize', onResize);
      controls.removeEventListener('start', cancelFlyTo);
      renderer.domElement.removeEventListener('pointermove', onPointerMove);
      renderer.domElement.removeEventListener('pointerdown', onPointerDown);
      renderer.domElement.removeEventListener('pointerup', onPointerUp);
      renderer.dispose();
      if (containerRef.current?.contains(renderer.domElement)) {
        containerRef.current.removeChild(renderer.domElement);
      }
    };
  }, []);

  // Toggle particles visibility
  useEffect(() => {
    particlesGroupRef.current.visible = flowActive;
  }, [flowActive]);

  // Toggle distance fog live without rebuilding scene
  useEffect(() => {
    if (sceneRef.current) {
      sceneRef.current.fog = settings.fogEnabled ? new THREE.Fog(0x06080d, 6000, 22000) : null;
    }
  }, [settings.fogEnabled]);

  // Rebuild 3D scene when data, mode, selectedLang, or layout settings change
  useEffect(() => {
    if (!data) return;
    clearGroup(cityGroupRef.current);
    clearGroup(beamGroupRef.current);
    clearGroup(selectBeamGroupRef.current);
    clickableObjectsRef.current = [];
    meshMapRef.current.clear();
    flowCurvesRef.current = [];

    if (highlightRingRef.current) {
      (highlightRingRef.current.material as THREE.MeshBasicMaterial).opacity = 0;
    }

    if (mode === 'CITY') {
      buildCity();
    } else if (mode === 'LAYER') {
      buildLayerFlow();
    } else if (mode === 'GALAXY') {
      buildGalaxy();
    } else if (mode === 'MICRO') {
      buildMicro();
    }
  }, [data, mode, selectedLang, settings.groupBy, settings.heightMetric, settings.showLabels]);

  // Re-draw selection beams, Blast Radius Focus, and Focus Dimming / Smell Spotlight
  useEffect(() => {
    clearGroup(selectBeamGroupRef.current);
    if (!data) return;

    const applyDimming = (focusedIds: Set<string> | null) => {
      clickableObjectsRef.current.forEach((m) => {
        const mat = m.material as THREE.MeshStandardMaterial;
        const u = m.userData as CodeNode | undefined;
        if (!mat || !u) return;
        if (!focusedIds) {
          mat.transparent = false;
          mat.opacity = 1.0;
        } else {
          const isFocused = focusedIds.has(u.id) || (u.file_path && focusedIds.has('file:' + u.file_path));
          mat.transparent = !isFocused;
          mat.opacity = isFocused ? 1.0 : 0.14;
        }
        mat.needsUpdate = true;
      });
    };

    if (selectedSprawl) {
      const focused = new Set<string>(selectedSprawl.involved_nodes);
      applyDimming(focused);
      const meshes: THREE.Mesh[] = [];
      selectedSprawl.involved_nodes.forEach((nid) => {
        const m = resolveMeshForNode(nid);
        if (m) meshes.push(m);
      });

      if (meshes.length >= 2) {
        for (let i = 0; i < meshes.length; i++) {
          for (let j = i + 1; j < Math.min(meshes.length, i + 5); j++) {
            drawArc(meshes[i].position, meshes[j].position, 0xe3b341, selectBeamGroupRef.current, 0.95, false, 0.25);
          }
        }
      }

      if (meshes.length > 0) {
        let cx = 0, cy = 0, cz = 0;
        meshes.forEach((m) => {
          cx += m.position.x;
          cy += m.position.y;
          cz += m.position.z;
        });
        cx /= meshes.length;
        cy /= meshes.length;
        cz /= meshes.length;
        flyTo(cx, cy + 140, cz + 280, { x: cx, y: cy, z: cz });
      }
      return;
    }

    if (selectedNode) {
      const focused = new Set<string>([selectedNode.id]);
      const mesh = resolveMeshForNode(selectedNode.id);
      if (mesh) {
        // Place highlight ring under selected mesh
        if (highlightRingRef.current) {
          highlightRingRef.current.position.set(mesh.position.x, 3, mesh.position.z);
          (highlightRingRef.current.material as THREE.MeshBasicMaterial).opacity = 0.9;
        }

        // Draw direct dependency arcs
        data.edges.forEach((e) => {
          if (e.type === 'contains') return;
          if (e.source === selectedNode.id) {
            focused.add(e.target);
            const tm = resolveMeshForNode(e.target);
            if (tm) drawArc(mesh.position, tm.position, 0x38bdf8, selectBeamGroupRef.current, 0.95, false, 0.4);
          }
          if (e.target === selectedNode.id) {
            focused.add(e.source);
            const sm = resolveMeshForNode(e.source);
            if (sm) drawArc(sm.position, mesh.position, 0x3fb950, selectBeamGroupRef.current, 0.95, false, 0.4);
          }
        });

        // If Blast Simulation is active, calculate all reverse dependents and highlight them in red
        if (blastSimActive) {
          const dependents = getTransitiveDependents(selectedNode.id, data.edges, 3);
          dependents.forEach((depId) => {
            focused.add(depId);
            const depMesh = resolveMeshForNode(depId);
            if (depMesh) {
              drawArc(mesh.position, depMesh.position, 0xf43f5e, selectBeamGroupRef.current, 0.85, false, 0.55);
            }
          });
        }

        applyDimming(focused);
        flyTo(mesh.position.x, mesh.position.y + 70, mesh.position.z + 160, mesh.position);
      }
    } else {
      if (highlightRingRef.current) {
        (highlightRingRef.current.material as THREE.MeshBasicMaterial).opacity = 0;
      }
      if (smellFilter !== 'ALL') {
        const focused = new Set<string>();
        data.nodes.forEach((n) => {
          if (smellFilter === 'CYCLES' && (n.has_cycle || n.is_layer_violation)) focused.add(n.id);
          else if (smellFilter === 'CLONES' && (n.is_clone || n.is_sprawl)) focused.add(n.id);
          else if (smellFilter === 'HOTSPOTS' && ((n.hotspot_score || 0) >= 0.45 || (n.runtime_heat || 0) > 0.2 || n.is_god)) focused.add(n.id);
        });
        applyDimming(focused);
      } else {
        applyDimming(null);
      }
    }
  }, [selectedNode, selectedSprawl, blastSimActive, smellFilter, data, mode, selectedLang]);

  // 1. CODE CITY BUILDER (Software City Metaphor)
  const buildCity = () => {
    if (!data) return;
    const modules: Record<string, CodeNode[]> = {};
    const files = data.nodes.filter((n) => n.type === 'file');

    files.forEach((n) => {
      if (selectedLang !== 'ALL' && n.lang !== selectedLang) return;
      let m = n.module || 'root';
      if (settings.groupBy === 'LANGUAGE') {
        const langMap: Record<string, string> = {
          cpp: 'C / C++',
          dart: 'Dart / Flutter',
          kotlin: 'Kotlin / Android',
          cs: 'C# / .NET',
          python: 'Python',
          js: 'JS / TypeScript',
          other: 'Other',
        };
        m = langMap[n.lang] || 'Other';
      } else if (settings.groupBy === 'TOP_LEVEL') {
        const fp = n.file_path || m;
        const parts = fp.split('/');
        if (parts.length >= 2 && (parts[0] === 'apps' || parts[0] === 'packages' || parts[0] === 'tools' || parts[0] === 'src')) {
          m = parts[0] + '/' + parts[1];
        } else {
          m = parts[0] || m;
        }
      }
      if (!modules[m]) modules[m] = [];
      modules[m].push(n);
    });

    const keys = Object.keys(modules).sort();
    const modCols: Record<string, number> = {};
    const modPlatSizes: Record<string, number> = {};

    keys.forEach((m) => {
      const cnt = modules[m].length;
      const inCols = Math.ceil(Math.sqrt(cnt));
      modCols[m] = inCols;
      modPlatSizes[m] = Math.max(340, inCols * 60 + 90);
    });

    const islandGap = 340;
    const gridCols = Math.ceil(Math.sqrt(keys.length));
    const colWidths: number[] = new Array(gridCols).fill(0);
    const rowHeights: number[] = new Array(Math.ceil(keys.length / gridCols)).fill(0);

    keys.forEach((m, idx) => {
      const c = idx % gridCols;
      const r = Math.floor(idx / gridCols);
      colWidths[c] = Math.max(colWidths[c], modPlatSizes[m]);
      rowHeights[r] = Math.max(rowHeights[r], modPlatSizes[m]);
    });

    const colOffsets: number[] = [];
    let curX = 0;
    for (let c = 0; c < colWidths.length; c++) {
      colOffsets.push(curX + colWidths[c] / 2);
      curX += colWidths[c] + islandGap;
    }
    const totalW = curX - islandGap;

    const rowOffsets: number[] = [];
    let curZ = 0;
    for (let r = 0; r < rowHeights.length; r++) {
      rowOffsets.push(curZ + rowHeights[r] / 2);
      curZ += rowHeights[r] + islandGap;
    }
    const totalH = curZ - islandGap;

    keys.forEach((modName, mIdx) => {
      const c = mIdx % gridCols;
      const r = Math.floor(mIdx / gridCols);
      const dx = colOffsets[c] - totalW / 2;
      const dz = rowOffsets[r] - totalH / 2;

      const fList = modules[modName];
      const inCols = modCols[modName];
      const pSize = modPlatSizes[modName];

      // Island base slab
      const plat = new THREE.Mesh(
        new THREE.BoxGeometry(pSize, 6, pSize),
        new THREE.MeshStandardMaterial({ 
          color: 0x0a0e17, 
          roughness: 0.85, 
          metalness: 0.2 
        })
      );
      plat.position.set(dx, 3, dz);
      cityGroupRef.current.add(plat);

      // Island neon border
      const borderGeom = new THREE.EdgesGeometry(new THREE.BoxGeometry(pSize, 6.2, pSize));
      const borderLine = new THREE.LineSegments(
        borderGeom,
        new THREE.LineBasicMaterial({ color: 0x1e293b, transparent: true, opacity: 0.85 })
      );
      borderLine.position.set(dx, 3, dz);
      cityGroupRef.current.add(borderLine);

      // 3D Module Label above island
      if (settings.showLabels) {
        const labelSprite = createModuleLabel(modName);
        if (labelSprite) {
          labelSprite.position.set(dx, 42, dz - pSize / 2 - 20);
          cityGroupRef.current.add(labelSprite);
        }
      }

      // Place file skyscrapers
      fList.forEach((f, fIdx) => {
        const fr = Math.floor(fIdx / inCols);
        const fc = fIdx % inCols;
        const startX = dx - pSize / 2 + 50;
        const startZ = dz - pSize / 2 + 50;
        const bx = startX + fc * 60;
        const bz = startZ + fr * 60;
        const rawVal =
          settings.heightMetric === 'COMPLEXITY'
            ? (f.complexity || 1) * 4.5
            : settings.heightMetric === 'BLAST'
            ? (f.blast_radius || 0) * 1.2
            : (f.loc || 20) * 0.42;
        const bh = Math.max(18, Math.min(360, rawVal));

        let color = getLangColorHex(f.name);
        let emissiveColor = 0x000000;
        let emissiveIntensity = 0;

        if (f.has_cycle || f.is_layer_violation) {
          color = 0xf85149;
          emissiveColor = 0xf85149;
          emissiveIntensity = 0.45;
        } else if (f.is_god) {
          color = 0xeab308;
          emissiveColor = 0xeab308;
          emissiveIntensity = 0.4;
        } else if (f.is_clone) {
          color = 0xa855f7;
          emissiveColor = 0xa855f7;
          emissiveIntensity = 0.35;
        } else if (f.is_sprawl) {
          color = 0xf59e0b;
          emissiveColor = 0xf59e0b;
          emissiveIntensity = 0.35;
        } else if ((f.runtime_heat || 0) > 0.3 || (f.hotspot_score || 0) >= 0.6) {
          color = 0xf97316;
          emissiveColor = 0xea580c;
          emissiveIntensity = Math.min(0.55, 0.25 + (f.hotspot_score || 0) * 0.4);
        }

        const bldgGeom = new THREE.BoxGeometry(26, bh, 26);
        const bldgMat = new THREE.MeshStandardMaterial({
          color,
          roughness: 0.25,
          metalness: 0.5,
          emissive: emissiveColor,
          emissiveIntensity,
        });
        const bldg = new THREE.Mesh(bldgGeom, bldgMat);
        bldg.position.set(bx, bh / 2 + 6, bz);
        bldg.userData = f;
        cityGroupRef.current.add(bldg);
        clickableObjectsRef.current.push(bldg);
        meshMapRef.current.set(f.id, bldg);

        // Roof glowing antenna or beacon for God Objects / Critical cycles
        if (f.is_god || f.has_cycle) {
          const beaconGeom = new THREE.CylinderGeometry(1.5, 1.5, 16, 8);
          const beaconMat = new THREE.MeshBasicMaterial({ color: f.has_cycle ? 0xff3b30 : 0xffcc00 });
          const beacon = new THREE.Mesh(beaconGeom, beaconMat);
          beacon.position.set(bx, bh + 14, bz);
          cityGroupRef.current.add(beacon);
        }
      });
    });

    // Draw active flow beams between modules/files (sample up to 250 top edges)
    data.edges
      .filter((e) => e.type === 'imports')
      .slice(0, 250)
      .forEach((e) => {
        const s = meshMapRef.current.get(e.source);
        const t = meshMapRef.current.get(e.target);
        if (s && t) drawArc(s.position, t.position, 0x38bdf8, beamGroupRef.current, 0.20, true, 0.35);
      });

    // Cycle arcs in high-visibility neon red
    data.issues
      .filter((i) => i.type === 'CIRCULAR_DEPENDENCY')
      .forEach((iss) => {
        for (let i = 0; i + 1 < iss.involved_nodes.length; i++) {
          const s = resolveMeshForNode(iss.involved_nodes[i]);
          const t = resolveMeshForNode(iss.involved_nodes[i + 1]);
          if (s && t) drawArc(s.position, t.position, 0xf85149, beamGroupRef.current, 0.90, true, 0.45);
        }
      });

    // Sprawl constellation arcs
    data.issues
      .filter((i) => i.type === 'ABSTRACTION_SPRAWL')
      .slice(0, 10)
      .forEach((iss) => {
        for (let i = 0; i + 1 < Math.min(iss.involved_nodes.length, 6); i++) {
          const s = resolveMeshForNode(iss.involved_nodes[i]);
          const t = resolveMeshForNode(iss.involved_nodes[i + 1]);
          if (s && t) drawArc(s.position, t.position, 0xf59e0b, beamGroupRef.current, 0.50, true, 0.25);
        }
      });
  };

  // 2. ARCHITECTURE LAYER FLOW (Core -> Domain -> API/UI)
  const buildLayerFlow = () => {
    if (!data) return;
    const files = data.nodes.filter((n) => n.type === 'file');

    const getLayerY = (mod: string) => {
      const m = mod.toLowerCase();
      if (m.includes('core') || m.includes('kernel') || m.includes('engine') || m.includes('analyzer')) return 50;
      if (m.includes('service') || m.includes('domain') || m.includes('billing') || m.includes('order')) return 260;
      if (m.includes('api') || m.includes('bridge') || m.includes('plugin') || m.includes('gateway')) return 470;
      return 680; // Presentation / UI / Tests / CLI
    };

    const layerTiers: Record<number, CodeNode[]> = { 50: [], 260: [], 470: [], 680: [] };
    files.forEach((f) => {
      const y = getLayerY(f.module);
      layerTiers[y].push(f);
    });

    const tierLabels: Record<number, string> = {
      50: '1. Core & Engine Layer',
      260: '2. Domain & Business Services',
      470: '3. API, FFI & Plugins Gateway',
      680: '4. Presentation & Client UI',
    };

    [50, 260, 470, 680].forEach((tierY) => {
      const list = layerTiers[tierY];
      const cols = Math.ceil(Math.sqrt(Math.max(1, list.length)));
      const pSize = Math.max(380, cols * 42 + 70);

      // Glass floor plate
      const plane = new THREE.Mesh(
        new THREE.BoxGeometry(pSize, 4, pSize),
        new THREE.MeshStandardMaterial({
          color: tierY === 50 ? 0x0f172a : tierY === 260 ? 0x1e1b4b : tierY === 470 ? 0x064e3b : 0x450a0a,
          transparent: true,
          opacity: 0.5,
          roughness: 0.3,
          metalness: 0.7,
        })
      );
      plane.position.set(0, tierY - 4, 0);
      cityGroupRef.current.add(plane);

      // Border outline
      const borderLine = new THREE.LineSegments(
        new THREE.EdgesGeometry(new THREE.BoxGeometry(pSize, 4.2, pSize)),
        new THREE.LineBasicMaterial({ color: 0x38bdf8, transparent: true, opacity: 0.4 })
      );
      borderLine.position.set(0, tierY - 4, 0);
      cityGroupRef.current.add(borderLine);

      list.forEach((f, i) => {
        const r = Math.floor(i / cols);
        const c = i % cols;
        const x = (c - cols / 2) * 40;
        const z = (r - cols / 2) * 40;
        const bh = Math.max(14, Math.min(120, (f.loc || 20) * 0.28));

        const col = getLangColorHex(f.name);
        const bldg = new THREE.Mesh(
          new THREE.BoxGeometry(20, bh, 20),
          new THREE.MeshStandardMaterial({
            color: f.has_cycle ? 0xf85149 : col,
            roughness: 0.3,
            metalness: 0.4,
          })
        );
        bldg.position.set(x, tierY + bh / 2, z);
        bldg.userData = f;
        cityGroupRef.current.add(bldg);
        clickableObjectsRef.current.push(bldg);
        meshMapRef.current.set(f.id, bldg);
      });
    });

    // Layer violations: if lower layer depends on upper layer, mark as severe violation!
    data.edges
      .filter((e) => e.type === 'imports')
      .slice(0, 300)
      .forEach((e) => {
        const s = meshMapRef.current.get(e.source);
        const t = meshMapRef.current.get(e.target);
        if (s && t) {
          const isViolation = s.position.y < t.position.y;
          drawArc(
            s.position,
            t.position,
            isViolation ? 0xf43f5e : 0x38bdf8,
            beamGroupRef.current,
            isViolation ? 0.95 : 0.25,
            true,
            isViolation ? 0.5 : 0.3
          );
        }
      });
  };

  // 3. FORCE GALAXY TOPOLOGY (Centrality & PageRank based)
  const buildGalaxy = () => {
    if (!data) return;
    const files = data.nodes.filter((n) => n.type === 'file');
    const rad = 600;

    files.forEach((f, i) => {
      // Golden spiral distribution on sphere
      const phi = Math.acos(-1 + (2 * i) / Math.max(1, files.length));
      const theta = Math.sqrt(files.length * Math.PI) * phi;
      const r = rad * (1.2 - Math.min(1.0, (f.pagerank || 0) * 8)); // High pagerank closer to center!
      const x = r * Math.cos(theta) * Math.sin(phi);
      const y = r * Math.sin(theta) * Math.sin(phi) + 350;
      const z = r * Math.cos(phi);

      const col = f.has_cycle ? 0xf85149 : getLangColorHex(f.name);
      const radius = f.is_god || (f.pagerank && f.pagerank > 0.05) ? 14 : f.has_cycle ? 10 : 7;

      const sp = new THREE.Mesh(
        new THREE.SphereGeometry(radius, 16, 16),
        new THREE.MeshStandardMaterial({
          color: col,
          emissive: f.has_cycle || f.is_god ? col : 0x000000,
          emissiveIntensity: 0.45,
          roughness: 0.3,
        })
      );
      sp.position.set(x, y, z);
      sp.userData = f;
      cityGroupRef.current.add(sp);
      clickableObjectsRef.current.push(sp);
      meshMapRef.current.set(f.id, sp);
    });

    data.edges
      .filter((e) => e.type === 'imports')
      .slice(0, 260)
      .forEach((e) => {
        const s = meshMapRef.current.get(e.source);
        const t = meshMapRef.current.get(e.target);
        if (s && t) drawArc(s.position, t.position, 0x38bdf8, beamGroupRef.current, 0.22, true, 0.25);
      });
  };

  // 4. FUNCTION MICRO-TOPOLOGY
  const buildMicro = () => {
    if (!data) return;
    const fns = data.nodes
      .filter((n) => n.type === 'function')
      .sort((a, b) => ((b.is_clone || b.is_sprawl ? 1000 : 0) + b.complexity) - ((a.is_clone || a.is_sprawl ? 1000 : 0) + a.complexity))
      .slice(0, 800);

    const cols = Math.ceil(Math.sqrt(Math.max(1, fns.length)));
    fns.forEach((fn, i) => {
      const r = Math.floor(i / cols);
      const c = i % cols;
      const x = (c - cols / 2) * 38;
      const z = (r - cols / 2) * 38;
      const h = Math.max(8, Math.min(260, (fn.loc || 5) * 1.6));

      let col = 0x3fb950;
      if (fn.is_clone) col = 0xa855f7;
      else if (fn.is_sprawl) col = 0xf59e0b;
      else if (fn.complexity > 10) col = 0xeab308;

      const cyl = new THREE.Mesh(
        new THREE.CylinderGeometry(7, 7, h, 10),
        new THREE.MeshStandardMaterial({
          color: col,
          emissive: fn.is_clone ? 0xa855f7 : fn.is_sprawl ? 0xf59e0b : 0x000000,
          emissiveIntensity: fn.is_clone ? 0.45 : fn.is_sprawl ? 0.45 : 0,
        })
      );
      cyl.position.set(x, h / 2, z);
      cyl.userData = fn;
      cityGroupRef.current.add(cyl);
      clickableObjectsRef.current.push(cyl);
      meshMapRef.current.set(fn.id, cyl);
    });

    data.issues
      .filter((i) => i.type === 'CODE_DUPLICATION')
      .forEach((iss) => {
        if (iss.involved_nodes.length >= 2) {
          const s = meshMapRef.current.get(iss.involved_nodes[0]);
          const t = meshMapRef.current.get(iss.involved_nodes[1]);
          if (s && t) drawArc(s.position, t.position, 0xa855f7, beamGroupRef.current, 0.95, true, 0.3);
        }
      });

    data.issues
      .filter((i) => i.type === 'ABSTRACTION_SPRAWL')
      .forEach((iss) => {
        for (let i = 0; i + 1 < Math.min(iss.involved_nodes.length, 6); i++) {
          const s = meshMapRef.current.get(iss.involved_nodes[i]);
          const t = meshMapRef.current.get(iss.involved_nodes[i + 1]);
          if (s && t) drawArc(s.position, t.position, 0xf59e0b, beamGroupRef.current, 0.8, true, 0.25);
        }
      });
  };

  return (
    <div className="relative w-full h-full select-none">
      <div ref={containerRef} className="w-full h-full" />
      <div
        ref={tooltipRef}
        className="absolute hidden pointer-events-none z-50 bg-[#090d16]/95 border border-primary/50 text-white text-[11px] p-2.5 rounded-lg shadow-2xl backdrop-blur-md leading-relaxed max-w-xs transition-opacity duration-150"
      />
    </div>
  );
};
