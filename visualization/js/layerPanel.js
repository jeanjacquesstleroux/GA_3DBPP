// layerPanel.js — Layer manifest side panel
export class LayerPanel {
    constructor(listEl, renderer) {
        this._list     = listEl;
        this._renderer = renderer;
    }

    clear() {
        this._list.innerHTML =
            '<span style="color:var(--text-muted);font-size:12px;">Run algorithm to see layers.</span>';
    }

    // manifest: LayerManifestEntry[] from worker layerManifests[ci]
    // containerItems: Phase 1 items for this container (used for hover highlight)
    render(containerIndex, manifest, containerItems) {
        this._list.innerHTML = '';

        if (!manifest || manifest.length === 0) {
            this._list.innerHTML =
                '<span style="color:var(--text-muted);font-size:12px;">No layer data.</span>';
            return;
        }

        const header = document.createElement('div');
        header.style.cssText =
            'font-size:11px;color:var(--text-muted);margin-bottom:6px;' +
            'text-transform:uppercase;letter-spacing:.05em;padding-bottom:4px;' +
            'border-bottom:1px solid var(--border);';
        header.textContent = `Container ${containerIndex + 1} — Layer Breakdown`;
        this._list.appendChild(header);

        for (const lme of manifest) {
            const details = document.createElement('details');
            details.className         = 'layer-row';
            details.dataset.layerIndex = String(lme.layer_index);

            const summary = document.createElement('summary');
            summary.textContent =
                `Layer ${lme.layer_index}  (z: ${lme.z_min}–${lme.z_max} mm)  — ${lme.item_count} items`;
            details.appendChild(summary);

            if (lme.item_type_summary && lme.item_type_summary.length > 0) {
                const typeDiv = document.createElement('div');
                typeDiv.className   = 'layer-type-summary';
                typeDiv.textContent = lme.item_type_summary
                    .map(s => `Type ${s.item_type_index} × ${s.count}`)
                    .join(', ');
                details.appendChild(typeDiv);
            }

            details.addEventListener('mouseenter', () => this._renderer.highlightLayer(lme.layer_index));
            details.addEventListener('mouseleave', () => this._renderer.resetHighlight());

            this._list.appendChild(details);
        }
    }

    setActiveLayer(layerIndex) {
        for (const row of this._list.querySelectorAll('.layer-row')) {
            const isActive = parseInt(row.dataset.layerIndex, 10) === layerIndex;
            row.classList.toggle('active', isActive);
        }
        const activeRow = this._list.querySelector('.layer-row.active');
        if (activeRow) activeRow.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
}
