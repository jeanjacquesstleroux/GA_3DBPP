// renderer.js — Three.js scene setup and update API (InstancedMesh-based)
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const PALETTE = [
    0x4a9eff, 0xff6b6b, 0x6bff8a, 0xffcc44, 0xcc6bff,
    0xff9a44, 0x44ffee, 0xff44cc, 0x88ff44, 0x4488ff,
    0xff8844, 0x44ff88, 0xee44ff, 0xffee44, 0x44eeff,
    0xff4488, 0x88ff88, 0x8844ff, 0xff4444, 0x44ff44,
];

// Algorithm: Z-up, origin at corner, X=length, Y=width, Z=up
// Three.js: Y-up
// Mapping: algo (dx,dy,dz) → Three BoxGeometry(dx, dz, dy)
//          algo position (x,y,z) → Three center (x+dx/2, z+dz/2, y+dy/2)
function algoToThree(item) {
    return {
        cx: item.x + item.dx / 2,
        cy: item.z + item.dz / 2,
        cz: item.y + item.dy / 2,
        sx: item.dx,
        sy: item.dz,
        sz: item.dy,
    };
}

export class Renderer {
    constructor(canvas) {
        this._canvas  = canvas;
        this._running = false;

        this._scene = new THREE.Scene();
        this._scene.background = new THREE.Color(0x1a1a2e);

        const w = canvas.clientWidth  || 800;
        const h = canvas.clientHeight || 600;

        this._camera = new THREE.PerspectiveCamera(50, w / h, 1, 60000);
        this._camera.position.set(2000, 1600, 2500);

        this._webgl = new THREE.WebGLRenderer({ canvas, antialias: true });
        this._webgl.setPixelRatio(window.devicePixelRatio);
        this._webgl.setSize(w, h, false);

        this._scene.add(new THREE.AmbientLight(0xffffff, 0.55));
        const dir = new THREE.DirectionalLight(0xffffff, 0.9);
        dir.position.set(3000, 5000, 2000);
        this._scene.add(dir);

        this._controls = new OrbitControls(this._camera, canvas);
        this._controls.enableDamping  = true;
        this._controls.dampingFactor  = 0.08;

        this._meshes     = new Map();   // typeIdx → InstancedMesh
        this._typeCounts = new Map();   // typeIdx → instances shown so far
        this._shownItems = [];          // [{item, typeIdx, instanceIdx}]
        this._palletLine = null;

        this._ro = new ResizeObserver(() => this._onResize());
        this._ro.observe(canvas.parentElement);

        this._running = true;
        this._loop();
    }

    // Set up pallet wireframe + InstancedMeshes for a new container.
    // items: all items that may appear (determines capacity per type).
    // moveCam: false skips focusCameraOnPallet (container switch, keep user's view).
    setupContainer(dims, items, moveCam = true) {
        this._setPalletWire(dims);
        this._allocateMeshes(items);
        if (moveCam) this.focusCameraOnPallet(dims);
    }

    _setPalletWire(dims) {
        if (this._palletLine) {
            this._scene.remove(this._palletLine);
            this._palletLine.geometry.dispose();
        }
        const boxGeo = new THREE.BoxGeometry(dims.L, dims.H, dims.W);
        const edges  = new THREE.EdgesGeometry(boxGeo);
        boxGeo.dispose();
        this._palletLine = new THREE.LineSegments(
            edges,
            new THREE.LineBasicMaterial({ color: 0xffffff, opacity: 0.28, transparent: true })
        );
        this._palletLine.position.set(dims.L / 2, dims.H / 2, dims.W / 2);
        this._scene.add(this._palletLine);
    }

    _allocateMeshes(items) {
        for (const [, mesh] of this._meshes) {
            this._scene.remove(mesh);
            mesh.geometry.dispose();
            mesh.material.dispose();
        }
        this._meshes.clear();
        this._typeCounts.clear();
        this._shownItems = [];

        // Count capacity per type
        const cap = new Map();
        for (const item of items) {
            const t = item.item_type_index;
            cap.set(t, (cap.get(t) || 0) + 1);
        }

        const dummy = new THREE.Object3D();
        dummy.scale.set(0, 0, 0);
        dummy.updateMatrix();

        for (const [t, count] of cap) {
            const geo  = new THREE.BoxGeometry(1, 1, 1);
            const mat  = new THREE.MeshLambertMaterial({ color: PALETTE[t % PALETTE.length] });
            const mesh = new THREE.InstancedMesh(geo, mat, count);
            mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
            for (let i = 0; i < count; i++) mesh.setMatrixAt(i, dummy.matrix);
            mesh.instanceMatrix.needsUpdate = true;
            this._scene.add(mesh);
            this._meshes.set(t, mesh);
            this._typeCounts.set(t, 0);
        }
    }

    showItem(item) {
        const t    = item.item_type_index;
        const mesh = this._meshes.get(t);
        if (!mesh) return;
        const idx = this._typeCounts.get(t) || 0;
        if (idx >= mesh.count) return;   // safety: capacity exceeded
        const p     = algoToThree(item);
        const dummy = new THREE.Object3D();
        dummy.scale.set(p.sx, p.sy, p.sz);
        dummy.position.set(p.cx, p.cy, p.cz);
        dummy.updateMatrix();
        mesh.setMatrixAt(idx, dummy.matrix);
        mesh.instanceMatrix.needsUpdate = true;
        this._typeCounts.set(t, idx + 1);
        this._shownItems.push({ item, typeIdx: t, instanceIdx: idx });
    }

    showLayer(items) {
        for (const item of items) this.showItem(item);
    }

    clearItems() {
        const dummy = new THREE.Object3D();
        dummy.scale.set(0, 0, 0);
        dummy.updateMatrix();
        for (const [t, mesh] of this._meshes) {
            const cap = mesh.count;
            for (let i = 0; i < cap; i++) mesh.setMatrixAt(i, dummy.matrix);
            mesh.instanceMatrix.needsUpdate = true;
            this._typeCounts.set(t, 0);
            if (mesh.instanceColor) {
                // Reset colors to base palette color
                const base = new THREE.Color(PALETTE[t % PALETTE.length]);
                for (let i = 0; i < cap; i++) mesh.setColorAt(i, base);
                mesh.instanceColor.needsUpdate = true;
            }
        }
        this._shownItems = [];
    }

    highlightLayer(layerIndex) {
        for (const { item, typeIdx, instanceIdx } of this._shownItems) {
            const mesh  = this._meshes.get(typeIdx);
            if (!mesh) continue;
            const color = item.layer_index === layerIndex
                ? new THREE.Color(PALETTE[typeIdx % PALETTE.length])
                : new THREE.Color(0x1c1c2c);
            mesh.setColorAt(instanceIdx, color);
            mesh.instanceColor.needsUpdate = true;
        }
    }

    resetHighlight() {
        for (const { typeIdx, instanceIdx } of this._shownItems) {
            const mesh = this._meshes.get(typeIdx);
            if (!mesh || !mesh.instanceColor) continue;
            mesh.setColorAt(instanceIdx, new THREE.Color(PALETTE[typeIdx % PALETTE.length]));
            mesh.instanceColor.needsUpdate = true;
        }
    }

    focusCameraOnPallet(dims) {
        const cx = dims.L / 2;
        const cy = dims.H / 2;
        const cz = dims.W / 2;
        const d  = Math.max(dims.L, dims.W, dims.H) * 1.6;
        this._camera.position.set(cx + d, cy + d * 0.55, cz + d);
        this._controls.target.set(cx, cy, cz);
        this._controls.update();
    }

    _loop() {
        if (!this._running) return;
        requestAnimationFrame(() => this._loop());
        this._controls.update();
        this._webgl.render(this._scene, this._camera);
    }

    _onResize() {
        const canvas = this._canvas;
        const w = canvas.clientWidth;
        const h = canvas.clientHeight;
        if (!w || !h) return;
        this._camera.aspect = w / h;
        this._camera.updateProjectionMatrix();
        this._webgl.setSize(w, h, false);
    }

    dispose() {
        this._running = false;
        this._ro.disconnect();
        this._webgl.dispose();
    }
}
