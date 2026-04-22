"""
GA_3DBPP demo server.

Endpoints:
  GET  /               → visualization/index.html
  GET  /static/<path>  → visualization/ assets
  GET  /datasets       → JSON list of available datasets grouped by type
  POST /run            → invoke binary with --animated-output, return JSON result
"""

import json
import os
import pathlib
import subprocess

from flask import Flask, abort, jsonify, request, send_from_directory

# ── Paths ─────────────────────────────────────────────────────────────────────

PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
VIZ_DIR      = PROJECT_ROOT / "visualization"
DATA_DIR     = PROJECT_ROOT / "data"
OUTPUT_DIR   = PROJECT_ROOT / "output"
BINARY       = PROJECT_ROOT / "build" / "release" / "GA_3DBPP"

app = Flask(__name__, static_folder=None)

# ── Static serving ─────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return send_from_directory(VIZ_DIR, "index.html")


@app.route("/static/<path:filename>")
def static_files(filename):
    return send_from_directory(VIZ_DIR, filename)


# ── GET /datasets ──────────────────────────────────────────────────────────────

@app.route("/datasets")
def datasets():
    """
    Scan data/ recursively for .csv and .txt files and return a grouped list.

    Files in data/br_benchmark/ → group "BR Benchmark"
    All other .csv files         → group "Author"
    """
    entries = []

    br_dir = DATA_DIR / "br_benchmark"

    for path in sorted(DATA_DIR.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix not in (".csv", ".txt"):
            continue
        # Skip files in test_fixtures or other non-data subdirectories.
        if "test_fixtures" in path.parts:
            continue

        rel = path.relative_to(PROJECT_ROOT).as_posix()

        if path.is_relative_to(br_dir):
            group = "BR Benchmark"
            label = f"BR Benchmark ({path.stem})"
        else:
            group = "Author"
            label = path.stem

        entries.append({"label": label, "path": rel, "group": group})

    return jsonify({"datasets": entries})


# ── POST /run ──────────────────────────────────────────────────────────────────

@app.route("/run", methods=["POST"])
def run():
    """
    Body: {"dataset": "<relative path from project root>"}

    Validates the path is inside data/, invokes the release binary with
    --animated-output, and returns solution_animated.json as JSON.
    """
    body = request.get_json(silent=True)
    if not body or "dataset" not in body:
        return jsonify({"error": "Missing 'dataset' field in request body"}), 400

    # ── Security: reject path traversal ───────────────────────────────────────
    try:
        dataset_path = (PROJECT_ROOT / body["dataset"]).resolve()
    except Exception:
        return jsonify({"error": "Invalid path"}), 400

    if not str(dataset_path).startswith(str(DATA_DIR.resolve())):
        return jsonify({"error": "Dataset path must be inside the data/ directory"}), 403

    if not dataset_path.exists():
        return jsonify({"error": f"Dataset not found: {body['dataset']}"}), 404

    # ── Clean up stale output ─────────────────────────────────────────────────
    OUTPUT_DIR.mkdir(exist_ok=True)
    animated_json = OUTPUT_DIR / "solution_animated.json"
    if animated_json.exists():
        animated_json.unlink()

    # ── Invoke binary ─────────────────────────────────────────────────────────
    cmd = [
        str(BINARY),
        str(dataset_path),
        str(animated_json),
        "--animated-output",
    ]

    try:
        result = subprocess.run(
            cmd,
            cwd=str(PROJECT_ROOT),
            timeout=300,
            capture_output=True,
            text=True,
        )
    except subprocess.TimeoutExpired:
        return jsonify({"error": "Algorithm timed out after 300 seconds"}), 500
    except FileNotFoundError:
        return jsonify({
            "error": f"Binary not found at {BINARY}. Run: cmake --build --preset release"
        }), 500

    if result.returncode != 0:
        return jsonify({
            "error": "Binary exited with non-zero code",
            "stderr": result.stderr[-2000:],   # last 2 KB of stderr
        }), 500

    if not animated_json.exists():
        return jsonify({"error": "Binary succeeded but output file was not created"}), 500

    with open(animated_json) as f:
        data = json.load(f)

    return jsonify(data)


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("GA_3DBPP demo server running at http://localhost:5000")
    app.run(host="::", port=5000, debug=False)
