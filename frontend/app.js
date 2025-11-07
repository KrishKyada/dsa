const $ = (id) => document.getElementById(id);

async function analyze() {
  const codeA = $("codeA").value.trim();
  const codeB = $("codeB").value.trim();
  const k = parseInt($("k").value || "5", 10);
  const w = parseInt($("w").value || "4", 10);

  const msgBox = $("msg");
  const bar = $("barFill");
  $("result").classList.remove("hidden");
  msgBox.textContent = "🔄 Running similarity engine (Rolling Hash + Winnowing + AST)…";
  bar.style.width = "10%";

  if (!codeA || !codeB) {
    msgBox.textContent = "⚠️ Please enter code in both text areas.";
    bar.style.width = "0%";
    return;
  }

  try {
    const res = await fetch("/compare", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ codeA, codeB, window: w, k })
    });

    bar.style.width = "50%";
    const text = await res.text();
    let data;
    try {
      data = JSON.parse(text);
    } catch {
      msgBox.textContent = "❌ Invalid server response: " + text.slice(0, 200);
      bar.style.width = "0%";
      return;
    }

    if (!res.ok || data.error) {
      msgBox.textContent = "⚠️ Server Error: " + (data.error || "Unknown issue");
      bar.style.width = "0%";
      return;
    }

    const jaccard = (data.jaccard || 0) * 100;
    const edSim = (data.editSim || 0) * 100;
    const structSim = (data.structSim || 0) * 100;
    const score = (data.score || data.jaccard || 0) * 100;

    $("jaccard").textContent = jaccard.toFixed(2) + "%";
    $("edSim").textContent = edSim.toFixed(2) + "%";
    $("structSim").textContent = structSim.toFixed(2) + "%";
    $("score").textContent = score.toFixed(2) + "%";

    $("tokensA").textContent = data.tokensA ?? "-";
    $("tokensB").textContent = data.tokensB ?? "-";
    $("fpsA").textContent = data.fpsA ?? "-";
    $("fpsB").textContent = data.fpsB ?? "-";
    msgBox.textContent = data.message || "✅ Analysis complete.";

    const pct = Math.max(5, Math.min(100, Math.round(score)));
    bar.style.width = pct + "%";
  } catch (e) {
    msgBox.textContent = "❌ Request failed: " + e.message;
    bar.style.width = "0%";
  }
}

window.addEventListener("DOMContentLoaded", () => {
  $("analyzeBtn").addEventListener("click", analyze);
});
