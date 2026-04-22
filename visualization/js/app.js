// app.js — top-level init: dataset fetch, run button, animation coordination
import { Renderer }        from '/static/js/renderer.js';
import { Phase1Animator }  from '/static/js/phase1Animator.js';
import { LayerPanel }      from '/static/js/layerPanel.js';
import { Controls }        from '/static/js/controls.js';

// ── DOM refs ─────────────────────────────────────────────────────────────────
const runBtn         = document.getElementById('run-btn');
const spinnerEl      = document.getElementById('spinner');
const datasetSelect  = document.getElementById('dataset-select');
const loadingOverlay = document.getElementById('loading-overlay');
const errorToast     = document.getElementById('error-toast');
const statDataset    = document.getElementById('stat-dataset');

// ── Core objects ──────────────────────────────────────────────────────────────
const canvas     = document.getElementById('three-canvas');
const renderer   = new Renderer(canvas);
const controls   = new Controls();
const layerPanel = new LayerPanel(document.getElementById('layer-list'), renderer);

let anim                = null;
let worker              = null;
let _activeContainerIdx = 0;
let _containerCount     = 0;

// ── Dataset loader ────────────────────────────────────────────────────────────
async function loadDatasets() {
    try {
        const res  = await fetch('/datasets');
        const data = await res.json();

        datasetSelect.innerHTML = '';
        const groups = {};
        for (const ds of data.datasets) {
            if (!groups[ds.group]) groups[ds.group] = [];
            groups[ds.group].push(ds);
        }
        for (const [groupName, items] of Object.entries(groups)) {
            const og = document.createElement('optgroup');
            og.label = groupName;
            for (const ds of items) {
                const opt       = document.createElement('option');
                opt.value       = ds.path;
                opt.textContent = ds.label;
                og.appendChild(opt);
            }
            datasetSelect.appendChild(og);
        }
    } catch {
        datasetSelect.innerHTML = '<option disabled selected>Failed to load datasets</option>';
    }
}

// ── Run ───────────────────────────────────────────────────────────────────────
runBtn.addEventListener('click', async () => {
    const path = datasetSelect.value;
    if (!path) return;

    anim?.pause();

    runBtn.disabled = true;
    spinnerEl.style.display = 'block';
    loadingOverlay.classList.remove('hidden');
    layerPanel.clear();

    statDataset.textContent = path.split('/').pop();

    try {
        const res = await fetch('/run', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ dataset: path }),
        });

        if (!res.ok) {
            const err = await res.json().catch(() => ({ error: 'Server error' }));
            showError(err.error || `Run failed (HTTP ${res.status})`);
            return;
        }

        const solution = await res.json();
        loadingOverlay.classList.add('hidden');
        _startAnimation(solution);

    } catch (err) {
        showError(err.message);
    } finally {
        runBtn.disabled = false;
        spinnerEl.style.display = 'none';
        loadingOverlay.classList.add('hidden');
    }
});

// ── Animation setup ───────────────────────────────────────────────────────────
function _startAnimation(solution) {
    if (worker) worker.terminate();
    worker = new Worker('/static/js/worker.js');

    worker.onmessage = (e) => {
        if (e.data.type !== 'READY') return;

        const { animationMode, allFrames, layerManifests } = e.data;

        _containerCount     = solution.containers.length;
        _activeContainerIdx = 0;

        anim = new Phase1Animator(renderer, layerPanel, controls);
        anim.prepare(solution, allFrames, animationMode, layerManifests);
        anim.setSpeed(controls.getSpeed());

        controls.buildContainerDots(_containerCount);
        controls.showContainerNav(_containerCount > 1);
        controls.updateContainerNav(0, _containerCount);
        controls.enable();

        anim.start(() => controls.setPlaying(false));
    };

    worker.postMessage({ type: 'PREPARE', solution });
}

// ── Control handlers ──────────────────────────────────────────────────────────
controls.setHandlers({
    onPlay:     () => anim?.resume(),
    onPause:    () => anim?.pause(),
    onRestart:  () => { _activeContainerIdx = 0; anim?.restart(); },
    onStepBack: () => anim?.stepBack(),
    onStepFwd:  () => anim?.stepFwd(),
    onSpeed:    (s) => anim?.setSpeed(s),
    onContainerSelect: (i) => {
        _activeContainerIdx = i;
        controls.updateContainerNav(i, _containerCount);
        anim?.viewFinalContainer(i);
    },
    onContainerPrev: () => {
        if (_activeContainerIdx > 0) {
            _activeContainerIdx--;
            controls.updateContainerNav(_activeContainerIdx, _containerCount);
            anim?.viewFinalContainer(_activeContainerIdx);
        }
    },
    onContainerNext: () => {
        if (_activeContainerIdx < _containerCount - 1) {
            _activeContainerIdx++;
            controls.updateContainerNav(_activeContainerIdx, _containerCount);
            anim?.viewFinalContainer(_activeContainerIdx);
        }
    },
});

// ── Error toast ───────────────────────────────────────────────────────────────
let _toastTimer = null;
function showError(msg) {
    errorToast.textContent = msg;
    errorToast.classList.remove('hidden');
    clearTimeout(_toastTimer);
    _toastTimer = setTimeout(() => errorToast.classList.add('hidden'), 5000);
}

// ── Init ──────────────────────────────────────────────────────────────────────
loadDatasets();
