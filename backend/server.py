from flask import Flask, request, jsonify, send_from_directory
import subprocess, os, json

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
    data = request.get_json()
    codeA = data.get("codeA", "")
    codeB = data.get("codeB", "")
    k = str(data.get("k", 5))
    w = str(data.get("w", 4))

    try:
        result = subprocess.run(
            ["./plagiarism_core.exe" if os.name == "nt" else "./plagiarism_core"],
            input=json.dumps({"codeA": codeA, "codeB": codeB, "k": k, "w": w}),
            text=True,
            capture_output=True,
            timeout=10
        )
        output = result.stdout.strip()
        return jsonify(json.loads(output))
    except Exception as e:
        return jsonify({"error": str(e)})

if __name__ == "__main__":
    app.run(host="127.0.0.1", port=8080, debug=True)
