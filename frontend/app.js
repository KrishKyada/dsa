const $ = (id) => document.getElementById(id);

async function analyze() {
  const codeA = $("codeA").value;
  const codeB = $("codeB").value;
  const k = parseInt($("k").value || "5", 10);
  const w = parseInt($("w").value || "4", 10);

  $("result").classList.remove("hidden");
  $("jaccard").textContent = "…";
  $("edSim").textContent = "…";
  $("structSim").textContent = "…";
  $("score").textContent = "…";
  $("tokensA").textContent = "…";
  $("tokensB").textContent = "…";
  $("fpsA").textContent = "…";
  $("fpsB").textContent = "…";
  $("msg").textContent = "Running C++ engine (Winnowing + Edit Distance + AST)…";
  $("barFill").style.width = "5%";

  try {
    const res = await fetch("/api/analyze", {
      method: "POST",
      headers: { "Content-Type":"application/json" },
      body: JSON.stringify({ codeA, codeB, k, w })
    });
    $("barFill").style.width = "50%";
    const data = await res.json();

    if(!res.ok){
      $("msg").textContent = data.error || "Unknown error";
      $("barFill").style.width = "0%";
      return;
    }

    $("jaccard").textContent = (data.jaccard*100).toFixed(2) + "%";
    $("edSim").textContent = (data.editSim*100).toFixed(2) + "%";
    $("structSim").textContent = (data.structSim*100).toFixed(2) + "%";
    $("score").textContent = (data.score*100).toFixed(2) + "%";
    $("tokensA").textContent = data.tokensA;
    $("tokensB").textContent = data.tokensB;
    $("fpsA").textContent = data.fpsA;
    $("fpsB").textContent = data.fpsB;
    $("msg").textContent = data.message || "";
    $("barFill").style.width = Math.max(5, Math.min(100, Math.round(data.score*100))) + "%";
  } catch (e) {
    $("msg").textContent = "Request failed: " + e.message;
    $("barFill").style.width = "0%";
  }
}

$("analyzeBtn").addEventListener("click", analyze);

