// ---------- Helper ----------
const $ = (id) => document.getElementById(id);

// ---------- Main Analyze Function ----------
async function analyze() {
  const codeA = $("codeA").value.trim();
  const codeB = $("codeB").value.trim();
  const k = parseInt($("k")?.value || "5", 10);
  const w = parseInt($("w")?.value || "4", 10);

  // Reset UI
  $("result").classList.remove("hidden");
  $("jaccard").textContent = "…";
  $("edSim").textContent = "…";
  $("structSim").textContent = "…";
  $("score").textContent = "…";
  $("tokensA").textContent = "…";
  $("tokensB").textContent = "…";
  $("fpsA").textContent = "…";
  $("fpsB").textContent = "…";
  $("msg").textContent = "Running similarity engine (Winnowing + AST)…";
  $("barFill").style.width = "5%";

  // Input validation
  if (!codeA || !codeB) {
    $("msg").textContent = "Please enter code in both boxes before comparing.";
    $("barFill").style.width = "0%";
    return;
  }

  try {
    // ---------- Send Request to Flask ----------
    const res = await fetch("/compare", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ codeA, codeB, window: w })
    });

    $("barFill").style.width = "50%";

    // ---------- Handle non-JSON / HTML responses ----------
    const text = await res.text();
    let data;
    try {
      data = JSON.parse(text);
    } catch (err) {
      $("msg").textContent = "Server returned invalid response:\n" + text.slice(0, 200);
      $("barFill").style.width = "0%";
      return;
    }

    // ---------- Handle backend errors ----------
    if (!res.ok || data.error) {
      $("msg").textContent = "Error: " + (data.error || "Unknown server error");
      $("barFill").style.width = "0%";
      return;
    }

    // ---------- Display results ----------
    const jaccard = (data.jaccard || 0) * 100;
    const score = (data.score || data.jaccard || 0) * 100;
    const edSim = (data.editSim || 0) * 100;
    const structSim = (data.structSim || 0) * 100;

    $("jaccard").textContent = jaccard.toFixed(2) + "%";
    $("edSim").textContent = edSim.toFixed(2) + "%";
    $("structSim").textContent = structSim.toFixed(2) + "%";
    $("score").textContent = score.toFixed(2) + "%";

    $("tokensA").textContent = data.tokensA ?? "-";
    $("tokensB").textContent = data.tokensB ?? "-";
    $("fpsA").textContent = data.fpsA ?? "-";
    $("fpsB").textContent = data.fpsB ?? "-";
    $("msg").textContent = data.message || "Analysis complete.";

    const pct = Math.max(5, Math.min(100, Math.round(score)));
    $("barFill").style.width = pct + "%";

  } catch (e) {
    $("msg").textContent = "Request failed: " + e.message;
    $("barFill").style.width = "0%";
  }
}

// ---------- Event Binding ----------
window.addEventListener("DOMContentLoaded", () => {
  const btn = $("analyzeBtn");
  if (btn) btn.addEventListener("click", analyze);
});
