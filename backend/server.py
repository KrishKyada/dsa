from flask import Flask, request, jsonify, send_from_directory
import subprocess, os, json

# Serve frontend from "../frontend"
app = Flask(__name__, static_folder="../frontend")

# ------------------ Serve Frontend ------------------
@app.route("/")
def index():
    return send_from_directory(app.static_folder, "index.html")

@app.route("/<path:path>")
def static_files(path):
    return send_from_directory(app.static_folder, path)

# ------------------ API Endpoint ------------------
@app.route("/api/analyze", methods=["POST"])
def analyze():
    data = request.get_json(force=True, silent=True) or {}
    codeA = data.get("codeA", "") or ""
    codeB = data.get("codeB", "") or ""

    # Convert k and w to integers (safe fallback)
    def to_int(x, default):
        try:
            return int(x)
        except Exception:
            return default

    k = to_int(data.get("k", 5), 5)
    w = to_int(data.get("w", 4), 4)

    exe_name = "plagiarism_core.exe" if os.name == "nt" else "plagiarism_core"
    exe_path = os.path.join(os.getcwd(), exe_name)

    if not os.path.isfile(exe_path):
        return jsonify({
            "error": f"Core binary not found at {exe_path}. Please compile plagiarism_core.cpp first."
        }), 500

    try:
        payload = {"codeA": codeA, "codeB": codeB, "k": k, "w": w}
        result = subprocess.run(
            [exe_path],
            input=json.dumps(payload),
            text=True,
            capture_output=True,
            timeout=10
        )

        if result.returncode != 0:
            return jsonify({
                "error": "Core returned non-zero exit code",
                "stderr": result.stderr
            }), 500

        output = result.stdout.strip()
        if not output.startswith("{"):
            return jsonify({
                "error": "Invalid core output",
                "raw": output
            }), 500

        return jsonify(json.loads(output))

    except subprocess.TimeoutExpired:
        return jsonify({"error": "Core process timed out"}), 504
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    # You can change the port if 8080 is busy
    app.run(host="127.0.0.1", port=8080, debug=True)
