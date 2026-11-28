import { AppData, CodeNode, CodeEdge, CodeIssue, getLanguage } from './types';

export function unpackBinary(arrayBuffer: ArrayBuffer): AppData {
  const view = new DataView(arrayBuffer);
  let off = 0;

  const magic = String.fromCharCode(
    view.getUint8(off++),
    view.getUint8(off++),
    view.getUint8(off++),
    view.getUint8(off++)
  );
  if (magic !== 'ARCH') {
    throw new Error('Invalid binary magic: expected ARCH');
  }

  const version = view.getUint16(off, true);
  off += 2;
  const health = view.getFloat32(off, true);
  off += 4;
  const nodeCount = view.getUint32(off, true);
  off += 4;
  const edgeCount = view.getUint32(off, true);
  off += 4;
  const issueCount = view.getUint32(off, true);
  off += 4;
  const strTableSize = view.getUint32(off, true);
  off += 4;

  const strBytes = new Uint8Array(arrayBuffer, off, strTableSize);
  off += strTableSize;
  const decoder = new TextDecoder('utf-8');

  function getStr(offset: number): string {
    if (offset >= strTableSize) return '';
    let end = offset;
    while (end < strTableSize && strBytes[end] !== 0) end++;
    return decoder.decode(strBytes.subarray(offset, end));
  }

  const nodes: CodeNode[] = [];
  for (let i = 0; i < nodeCount; i++) {
    const idOff = view.getUint32(off, true); off += 4;
    const nameOff = view.getUint32(off, true); off += 4;
    const typeCode = view.getUint8(off); off += 1;
    const modOff = view.getUint32(off, true); off += 4;
    const fileOff = view.getUint32(off, true); off += 4;
    const loc = view.getUint32(off, true); off += 4;

    let lineStart = 0, lineEnd = 0, blastRadius = 0;
    if (version >= 3) {
      lineStart = view.getUint32(off, true); off += 4;
      lineEnd = view.getUint32(off, true); off += 4;
    } else if (version >= 2) {
      lineStart = view.getUint16(off, true); off += 2;
      lineEnd = view.getUint16(off, true); off += 2;
    }
    const comp = view.getUint16(off, true); off += 2;
    const inDeg = view.getUint16(off, true); off += 2;
    const outDeg = view.getUint16(off, true); off += 2;
    if (version >= 2) {
      blastRadius = view.getUint16(off, true); off += 2;
    }
    const inst = view.getFloat32(off, true); off += 4;
    const pr = view.getFloat32(off, true); off += 4;
    const flags = view.getUint8(off); off += 1;

    const name = getStr(nameOff);
    nodes.push({
      id: getStr(idOff),
      name,
      type: typeCode === 0 ? 'module' : (typeCode === 1 ? 'file' : 'function'),
      module: getStr(modOff),
      file_path: getStr(fileOff),
      lang: getLanguage(name),
      loc,
      line_start: lineStart,
      line_end: lineEnd,
      complexity: comp,
      in_degree: inDeg,
      out_degree: outDeg,
      blast_radius: blastRadius,
      instability: inst,
      pagerank: pr,
      has_cycle: Boolean(flags & (1 << 0)),
      is_god: Boolean(flags & (1 << 1)),
      is_clone: Boolean(flags & (1 << 2)),
      is_sprawl: Boolean(flags & (1 << 3)),
      is_layer_violation: Boolean(flags & (1 << 4)),
      is_orphan: Boolean(flags & (1 << 5)),
    });
  }

  const edges: CodeEdge[] = [];
  for (let i = 0; i < edgeCount; i++) {
    const srcIdx = view.getUint32(off, true); off += 4;
    const tgtIdx = view.getUint32(off, true); off += 4;
    const typeCode = view.getUint8(off); off += 1;
    off += 2; // weight
    edges.push({
      source: srcIdx < nodeCount ? nodes[srcIdx].id : '',
      target: tgtIdx < nodeCount ? nodes[tgtIdx].id : '',
      type: typeCode === 0 ? 'imports' : (typeCode === 1 ? 'calls' : 'contains'),
    });
  }

  const issues: CodeIssue[] = [];
  for (let i = 0; i < issueCount; i++) {
    const idOff = view.getUint32(off, true); off += 4;
    const typeOff = view.getUint32(off, true); off += 4;
    const sevCode = view.getUint8(off); off += 1;
    const titleOff = view.getUint32(off, true); off += 4;
    const descOff = view.getUint32(off, true); off += 4;
    const recomOff = view.getUint32(off, true); off += 4;
    const invCount = view.getUint16(off, true); off += 2;

    const inv: string[] = [];
    for (let k = 0; k < invCount; k++) {
      const idx = view.getUint32(off, true); off += 4;
      if (idx < nodeCount) inv.push(nodes[idx].id);
    }

    issues.push({
      id: getStr(idOff),
      type: getStr(typeOff),
      severity: sevCode === 2 ? 'CRITICAL' : (sevCode === 1 ? 'WARNING' : 'INFO'),
      title: getStr(titleOff),
      description: getStr(descOff),
      recommendation: getStr(recomOff),
      involved_nodes: inv,
    });
  }

  return { health, nodes, edges, issues };
}
