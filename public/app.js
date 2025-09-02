const $ = (sel) => document.querySelector(sel);
const resultsEl = $("#results");
const suggEl = $("#suggestions");
const corrEl = $("#corrections");
const input = $("#search-box");
const form = $("#search-form");

function debounce(fn, ms = 200) {
  let t;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}

async function fetchJSON(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}

function renderPills(container, label, items, className) {
  if (!items || items.length === 0) {
    container.innerHTML = "";
    return;
  }
  container.innerHTML =
    `<span class="muted">${label}</span> ` +
    items
      .map(
        (w) =>
          `<span class="pill ${className}" data-val="${w}">${w}</span>`
      )
      .join(" ");
  container.querySelectorAll(".pill").forEach((el) => {
    el.addEventListener("click", () => {
      input.value = el.dataset.val;
      performSearch();
    });
  });
}

// ---------- 🔍 Highlight Helper ----------
function highlightText(text, query) {
  if (!query) return text;

  const reserved = ["and", "or"]; // skip boolean operators
  const escapeRegex = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

  const terms = query.trim().split(/\s+/).filter(t => !reserved.includes(t.toLowerCase()));

  let highlighted = text;
  terms.forEach((term) => {
    const regex = new RegExp(`(${escapeRegex(term)})`, "gi");
    highlighted = highlighted.replace(regex, "<b><u>$1</u></b>");
  });
  return highlighted;
}

// ---------- Results ----------
function renderResults(arr) {
  if (!arr || arr.length === 0) {
    resultsEl.innerHTML = `<div class="muted">No results.</div>`;
    return;
  }
  const q = input.value.trim();

  resultsEl.innerHTML = arr
    .map(
      (r) => `
    <div class="result">
      <div class="score">score: ${r.score.toFixed(3)}</div>
      <div><a href="${r.url}" target="_blank">${r.url}</a></div>
      <div class="muted">${r.path}</div>
      <p class="preview" data-id="${r.id}">
        ${highlightText(r.preview, q)}
      </p>
      <button class="view-btn" data-id="${r.id}">View Full Document</button>
    </div>
  `
    )
    .join("");

  document.querySelectorAll(".view-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      try {
        const doc = await fetchJSON(`/document?id=${id}`);
        showModal(doc, input.value.trim());
      } catch (e) {
        alert("Error loading document: " + e.message);
      }
    });
  });
}

// ---------- Modal ----------
function showModal(doc, query) {
  const modal = document.createElement("div");
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content">
      <span class="close">&times;</span>
      <h2>${doc.url}</h2>
      <pre class="doc-text">${highlightText(doc.content, query)}</pre>
    </div>
  `;
  document.body.appendChild(modal);

  modal.querySelector(".close").onclick = () => modal.remove();
  modal.addEventListener("click", (e) => {
    if (e.target === modal) modal.remove();
  });
}

// ---------- Suggestions + Corrections ----------
const liveAssist = debounce(async () => {
  const q = input.value.trim();
  if (q.length < 2) {
    suggEl.innerHTML = "";
    corrEl.innerHTML = "";
    return;
  }
  try {
    const s = await fetchJSON(`/suggest?prefix=${encodeURIComponent(q)}`);
    renderPills(suggEl, "Suggestions:", s, "suggestion");
  } catch (e) {}

  try {
    const c = await fetchJSON(
      `/correct?word=${encodeURIComponent(q)}&max=2`
    );
    renderPills(corrEl, "Did you mean:", c, "correction");
  } catch (e) {}
}, 150);

input.addEventListener("input", liveAssist);

async function performSearch() {
  const q = input.value.trim();
  if (!q) return;
  try {
    const data = await fetchJSON(`/search?query=${encodeURIComponent(q)}`);
    renderResults(data);
  } catch (e) {
    resultsEl.innerHTML = `<div class="muted">Error: ${e.message}</div>`;
  }
}

form.addEventListener("submit", (e) => {
  e.preventDefault();
  performSearch();
});

// ---------- Info Modal ----------
function showInfoModal() {
  const modal = document.createElement("div");
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content">
      <span class="close">&times;</span>
      <h2 class="info-heading">🔎 How Our Search Works</h2>
      <ul class="info-list">
        <li><strong>Smart Suggestions</strong> - While you type, we suggest completions using a <em>prefix tree (Trie)</em>.</li>
        <li><strong>“Did You Mean…?”</strong> - Small typos are corrected using <em>fuzzy matching (BK-Tree)</em>.</li>
        <li><strong>Flexible Querying</strong> - Use <code>AND</code> / <code>OR</code> between words to refine results.</li>
        <li><strong>Relevant Ranking</strong> - Results are ordered using <em>TF-IDF scoring</em> for better relevance.</li>
        <li><strong>Preview Highlights</strong> - Search terms are shown <b><u>bold and underlined</u></b> in snippets.</li>
      </ul>
    </div>
  `;
  document.body.appendChild(modal);

  modal.querySelector(".close").onclick = () => modal.remove();
  modal.addEventListener("click", (e) => {
    if (e.target === modal) modal.remove();
  });
}

// Add info button next to search form
const infoBtn = document.createElement("button");
infoBtn.type = "button";
infoBtn.className = "info-btn";
infoBtn.textContent = "ℹ️ How Search Works";
infoBtn.onclick = showInfoModal;

document.querySelector("#search-form").appendChild(infoBtn);
