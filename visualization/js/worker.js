// worker.js — Web Worker: pre-computes animation frames from solution JSON
// All CPU-intensive frame building runs here, not on the main thread.

const THRESHOLD_BOX_FAST    = 200;    // eslint-disable-line no-unused-vars
const THRESHOLD_LAYER        = 1000;
const BASE_ITEMS_PER_FRAME   = 1;     // eslint-disable-line no-unused-vars

self.onmessage = function(e) {
    if (e.data.type !== 'PREPARE') return;
    const solution   = e.data.solution;
    const containers = solution.containers || [];

    // All items per container, sorted by placement_order
    const allByContainer = containers.map(c =>
        (c.items || []).sort((a, b) => a.placement_order - b.placement_order)
    );

    const totalItems    = allByContainer.reduce((s, arr) => s + arr.length, 0);
    const animationMode = totalItems > THRESHOLD_LAYER ? 'layer' : 'box';

    const allFrames = [];

    for (let ci = 0; ci < containers.length; ci++) {
        const items   = allByContainer[ci];
        const p1Items = items.filter(i => i.phase === 1);
        const p2Items = items.filter(i => i.phase !== 1);

        if (animationMode === 'box') {
            for (const item of items) {
                allFrames.push({ containerIndex: ci, itemIndices: [item.placement_order] });
            }
        } else {
            // Layer mode: group phase-1 items by layer_index, then append phase-2 items individually
            const byLayer = new Map();
            for (const item of p1Items) {
                const li = item.layer_index != null ? item.layer_index : -1;
                if (!byLayer.has(li)) byLayer.set(li, []);
                byLayer.get(li).push(item.placement_order);
            }
            const sorted = [...byLayer.keys()].sort((a, b) => a - b);
            for (const li of sorted) {
                allFrames.push({ containerIndex: ci, itemIndices: byLayer.get(li), layerIndex: li });
            }
            for (const item of p2Items) {
                allFrames.push({ containerIndex: ci, itemIndices: [item.placement_order] });
            }
        }
    }

    const layerManifests = {};
    for (let ci = 0; ci < containers.length; ci++) {
        if (containers[ci].layer_manifest && containers[ci].layer_manifest.length > 0) {
            layerManifests[ci] = containers[ci].layer_manifest;
        }
    }

    self.postMessage({ type: 'READY', animationMode, allFrames, layerManifests });
};
