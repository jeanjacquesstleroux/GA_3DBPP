// controls.js — Play/Pause/Speed/Container-nav control bindings
export class Controls {
    constructor() {
        this._speedSteps  = window.SPEED_STEPS || [0.25, 0.5, 1.0, 2.0, 5.0];
        this._speedIdx    = 2;   // default: 1.0x

        this._el = {
            playPause:      document.getElementById('play-pause-btn'),
            restart:        document.getElementById('restart-btn'),
            stepBack:       document.getElementById('step-back-btn'),
            stepFwd:        document.getElementById('step-fwd-btn'),
            slider:         document.getElementById('speed-slider'),
            speedLabel:     document.getElementById('speed-label'),
            dots:           document.getElementById('container-dots'),
            containerNav:   document.getElementById('container-nav'),
            containerPrev:  document.getElementById('container-prev-btn'),
            containerNext:  document.getElementById('container-next-btn'),
            containerLabel: document.getElementById('container-nav-label'),
        };

        this._el.slider.addEventListener('input', () => {
            this._speedIdx = parseInt(this._el.slider.value, 10);
            const speed = this._speedSteps[this._speedIdx];
            this._el.speedLabel.textContent = speed + 'x';
            this._cb.onSpeed?.(speed);
        });

        this._cb = {};
    }

    setHandlers(handlers) {
        this._cb = handlers;

        this._el.playPause.addEventListener('click', () => {
            if (this._el.playPause.textContent === '⏸') {
                this._el.playPause.textContent = '▶';
                this._cb.onPause?.();
            } else {
                this._el.playPause.textContent = '⏸';
                this._cb.onPlay?.();
            }
        });

        this._el.restart.addEventListener('click',       () => this._cb.onRestart?.());
        this._el.stepBack.addEventListener('click',      () => this._cb.onStepBack?.());
        this._el.stepFwd.addEventListener('click',       () => this._cb.onStepFwd?.());
        this._el.containerPrev.addEventListener('click', () => this._cb.onContainerPrev?.());
        this._el.containerNext.addEventListener('click', () => this._cb.onContainerNext?.());
    }

    enable() {
        [this._el.playPause, this._el.restart, this._el.stepBack, this._el.stepFwd]
            .forEach(b => { b.disabled = false; });
        this._el.playPause.textContent = '⏸';
    }

    setPlaying(playing) {
        this._el.playPause.textContent = playing ? '⏸' : '▶';
    }

    getSpeed() {
        return this._speedSteps[this._speedIdx] ?? 1.0;
    }

    buildContainerDots(n) {
        this._el.dots.innerHTML = '';
        for (let i = 0; i < n; i++) {
            const dot = document.createElement('div');
            dot.className = 'container-dot';
            dot.title     = `Container ${i + 1}`;
            dot.addEventListener('click', () => this._cb.onContainerSelect?.(i));
            this._el.dots.appendChild(dot);
        }
    }

    // state: 'active' | 'done' | ''
    setContainerDot(idx, state) {
        const dots = this._el.dots.querySelectorAll('.container-dot');
        if (dots[idx]) {
            dots[idx].className = 'container-dot' + (state ? ' ' + state : '');
        }
    }

    showContainerNav(visible) {
        this._el.containerNav.classList.toggle('hidden', !visible);
    }

    // idx is 0-based; total is count
    updateContainerNav(idx, total) {
        this._el.containerLabel.textContent = `Container ${idx + 1} / ${total}`;
        this._el.containerPrev.disabled = idx <= 0;
        this._el.containerNext.disabled = idx >= total - 1;
    }
}
