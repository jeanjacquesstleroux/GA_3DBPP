// phase1Animator.js — Phase 1 box/layer animation state machine
export class Phase1Animator {
    constructor(renderer, layerPanel, controls) {
        this._renderer    = renderer;
        this._layerPanel  = layerPanel;
        this._controls    = controls;

        this._solution       = null;
        this._frames         = [];
        this._animMode       = 'box';
        this._layerManifests = {};

        this._frameIdx             = 0;
        this._budget               = 0;
        this._currentContainerIdx  = 0;
        this._paused               = false;
        this._speed                = 1.0;
        this._rafId                = null;
        this._itemLookup           = new Map();   // placement_order → item
        this._onComplete           = null;
    }

    prepare(solution, allFrames, animationMode, layerManifests) {
        this._solution       = solution;
        this._frames         = allFrames;
        this._animMode       = animationMode;
        this._layerManifests = layerManifests;
    }

    start(onComplete) {
        this._onComplete = onComplete;
        this._frameIdx   = 0;
        this._budget     = 0;
        this._paused     = false;
        this._setupContainer(0);
        this._controls.setPlaying(true);
        this._tick();
    }

    pause() {
        this._paused = true;
        this._controls.setPlaying(false);
        cancelAnimationFrame(this._rafId);
    }

    resume() {
        if (!this._paused) return;
        this._paused = false;
        this._controls.setPlaying(true);
        this._tick();
    }

    restart() {
        cancelAnimationFrame(this._rafId);
        this._frameIdx = 0;
        this._budget   = 0;
        this._paused   = false;
        const n = this._solution.containers.length;
        for (let i = 0; i < n; i++) this._controls.setContainerDot(i, '');
        this._setupContainer(0);
        this._controls.setPlaying(true);
        this._tick();
    }

    // Jump back one container
    stepBack() {
        const target = Math.max(0, this._currentContainerIdx - 1);
        this._jumpToContainer(target);
    }

    // Jump forward one container
    stepFwd() {
        const last   = this._solution.containers.length - 1;
        const target = Math.min(last, this._currentContainerIdx + 1);
        this._jumpToContainer(target);
    }

    // Jump directly to a specific container by index (dot click)
    stepFwdToContainer(ci) {
        const clamp = Math.max(0, Math.min(this._solution.containers.length - 1, ci));
        this._jumpToContainer(clamp);
    }

    // Show a container's fully-packed final state and pause — used for inspection navigation.
    viewFinalContainer(ci) {
        cancelAnimationFrame(this._rafId);
        this._currentContainerIdx = ci;
        this._paused = true;
        this._controls.setPlaying(false);

        const cont  = this._solution.containers[ci];
        const items = cont.items || [];

        this._renderer.setupContainer(cont.dims, items);
        this._renderer.showLayer(items);

        this._itemLookup = new Map();
        for (const item of items) this._itemLookup.set(item.placement_order, item);

        const manifest = this._layerManifests[ci];
        this._layerPanel.render(ci, manifest, items);

        const n = this._solution.containers.length;
        for (let i = 0; i < n; i++) {
            this._controls.setContainerDot(i, i === ci ? 'active' : 'done');
        }
        this._updateStats(ci);
    }

    setSpeed(s) { this._speed = s; }

    _setupContainer(ci) {
        if (ci >= this._solution.containers.length) return;
        this._currentContainerIdx = ci;

        const cont  = this._solution.containers[ci];
        const items = cont.items || [];

        this._renderer.setupContainer(cont.dims, items);

        // Build O(1) lookup by placement_order
        this._itemLookup = new Map();
        for (const item of items) this._itemLookup.set(item.placement_order, item);

        const manifest = this._layerManifests[ci];
        this._layerPanel.render(ci, manifest, items);
        this._controls.setContainerDot(ci, 'active');
        this._updateStats(ci);
    }

    _jumpToContainer(targetCi) {
        cancelAnimationFrame(this._rafId);
        const n = this._solution.containers.length;
        for (let i = 0; i < n; i++) {
            this._controls.setContainerDot(i, i < targetCi ? 'done' : '');
        }
        const frameStart = this._frames.findIndex(f => f.containerIndex === targetCi);
        if (frameStart < 0) {
            // No frames for this container — show empty wireframe and pause.
            this._frameIdx = this._frames.length;
            this._setupContainer(targetCi);
            this._paused = true;
            this._controls.setPlaying(false);
            return;
        }
        this._frameIdx = frameStart;
        this._budget   = 0;
        this._setupContainer(targetCi);
        if (!this._paused) this._tick();
    }

    _applyFrame(frame) {
        const frameItems = frame.itemIndices
            .map(po => this._itemLookup.get(po))
            .filter(Boolean);

        if (frameItems.length === 1) {
            this._renderer.showItem(frameItems[0]);
        } else {
            this._renderer.showLayer(frameItems);
        }

        // Highlight active layer row in side panel
        const li = frame.layerIndex != null ? frame.layerIndex
                 : frameItems[0]?.layer_index;
        if (li != null && li >= 0) this._layerPanel.setActiveLayer(li);
    }

    _tick() {
        this._rafId = requestAnimationFrame(() => {
            if (this._paused) return;

            this._budget += this._speed;

            // Process as many frames as the budget allows (min 0 for slow speeds).
            let processed = 0;
            while (this._budget >= 1.0 && processed < 32) {
                if (this._frameIdx >= this._frames.length) {
                    const last = this._frames[this._frames.length - 1];
                    if (last) this._controls.setContainerDot(last.containerIndex, 'done');
                    this._controls.setPlaying(false);
                    this._onComplete?.();
                    return;
                }

                const frame = this._frames[this._frameIdx];

                // Detect container transition
                if (frame.containerIndex !== this._currentContainerIdx) {
                    this._controls.setContainerDot(this._currentContainerIdx, 'done');
                    const nextCi = frame.containerIndex;
                    setTimeout(() => {
                        if (this._paused) return;
                        this._setupContainer(nextCi);
                        this._tick();
                    }, 800);
                    return;
                }

                this._applyFrame(frame);
                this._frameIdx++;
                this._budget -= 1.0;
                processed++;
            }

            this._tick();
        });
    }

    _updateStats(ci) {
        const sol = this._solution;
        const md  = sol.metadata || {};
        const totalItems = (md.phase1_item_count ?? 0) + (md.phase2_item_count ?? 0);
        document.getElementById('stat-containers').textContent = md.container_count ?? '—';
        document.getElementById('stat-items').textContent      = totalItems > 0 ? totalItems : '—';
        document.getElementById('stat-avg-util').textContent   =
            md.avg_utilization != null ? (md.avg_utilization * 100).toFixed(1) + '%' : '—';
        document.getElementById('stat-ga-gens').textContent    = md.ga_generations_recorded ?? '—';

        if (ci < sol.containers.length) {
            const cont  = sol.containers[ci];
            const used  = (cont.items || []).reduce((s, it) => s + it.dx * it.dy * it.dz, 0);
            const total = cont.dims.L * cont.dims.W * cont.dims.H;
            document.getElementById('stat-cur-util').textContent =
                total > 0 ? (used / total * 100).toFixed(1) + '%' : '—';
        }
    }
}
