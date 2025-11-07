from flask import Flask, request, jsonify, send_from_directory
import subprocess, os, sys

app = Flask(__name__, static_folder="../frontend")

def core_path():
    here = os.path.dirname(os.path.abspath(__file__))
    exe = "plagiarism_core.exe" if os.name == "nt" else "plagiarism_core"
    return os.path.join(here, exe)

def run_core(codeA: str, codeB: str, window: int = 4, timeout=10):
    exe = core_path()
    if not os.path.exists(exe):
        return {"error": f"Core binary not found at {exe}. Build it first."}

    a_bytes = codeA.encode("utf-8", errors="ignore")
    b_bytes = codeB.encode("utf-8", errors="ignore")
    header = f"LENA {len(a_bytes)}\nLENB {len(b_bytes)}\n".encode("utf-8")

    env = os.environ.copy()
    env["WINDOW"] = str(max(1, int(window)))

    try:
        proc = subprocess.run(
            [exe],
            input=header + a_bytes + b_bytes,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            env=env
        )
    except subprocess.TimeoutExpired:
        return {"error": "Analysis timed out."}
    except Exception as e:
        return {"error": str(e)}

    out = proc.stdout.decode("utf-8", errors="replace").strip()
    err = proc.stderr.decode("utf-8", errors="replace").strip()

    # Try JSON parse (simple, without importing json to keep it light)
    import json
    try:
        data = json.loads(out)
    except json.JSONDecodeError:
        return {"error": "Core returned invalid JSON.", "raw": out, "stderr": err}

    # sanitize any NaN/Inf (should not happen with current core, but safe)
    import math
    for k, v in list(data.items()):
        if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
            data[k] = 0.0

    if err:
        data["stderr"] = err
    return data

# -------- Serve frontend (optional) --------
@app.route("/")
def index():
    return send_from_directory(app.static_folder, "index.html")

@app.route("/<path:path>")
def static_files(path):
    return send_from_directory(app.static_folder, path)

# -------- API --------
@app.route("/compare", methods=["POST"])
def compare_codes():
    payload = request.get_json(force=True, silent=True) or {}
    codeA = payload.get("codeA", "")
    codeB = payload.get("codeB", "")
    window = payload.get("window", 4)

    if not codeA.strip() or not codeB.strip():
        return jsonify({"error": "One or both code inputs are empty.", "jaccard": 0.0})

    result = run_core(codeA, codeB, window=window)
    return jsonify(result)

if __name__ == "__main__":
    # Bind explicitly so you can open it in browser as http://127.0.0.1:8080
    app.run(host="127.0.0.1", port=8080, debug=True)
