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

async function fetchJSON(url, options = {}) {
  try {
    const r = await fetch(url, options);
    if (!r.ok) {
      const errorText = await r.text();
      throw new Error(`HTTP ${r.status}: ${errorText}`);
    }
    return r.json();
  } catch (error) {
    console.error("Fetch error:", error);
    throw error;
  }
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

  const reserved = ["and", "or"];
  const escapeRegex = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

  const terms = query
    .trim()
    .split(/\s+/)
    .filter((t) => !reserved.includes(t.toLowerCase()));

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
    resultsEl.innerHTML = `<div class="muted">No results found.</div>`;
    return;
  }
  const q = input.value.trim();

  resultsEl.innerHTML = arr
    .map(
      (r) => `
    <div class="result">
      <div class="score">score: ${r.score ? r.score.toFixed(3) : "-"}</div>
      <div><a href="#" onclick="return false;">${r.url || r.filename}</a></div>
      <div class="muted">${r.path || ""}</div>
      <p class="preview" data-id="${r.id}">
        ${highlightText(r.preview || r.content?.slice(0, 200) || "", q)}
      </p>
      <div class="result-actions">
        <button class="action-btn view-btn" data-id="${r.id}">View Full Document</button>
        <button class="action-btn edit-btn" data-id="${r.id}">Edit</button>
        <button class="action-btn delete-btn" data-id="${r.id}">Delete</button>
      </div>
    </div>
  `
    )
    .join("");

  // View full doc
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

  // Edit doc → auto-fill edit form
  document.querySelectorAll(".edit-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      try {
        const doc = await fetchJSON(`/document?id=${id}`);
        $("#edit-id").value = id;
        $("#edit-content").value = doc.content;
        document
          .querySelector("#edit-form")
          .scrollIntoView({ behavior: "smooth" });
      } catch (e) {
        alert("Error loading document for edit: " + e.message);
      }
    });
  });

  // ❌ Delete document
  document.querySelectorAll(".delete-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      if (!confirm(`Are you sure you want to delete document ${id}?`)) return;

      try {
        await fetchJSON(`/delete/${id}`, { method: "DELETE" });
        alert(`🗑️ Document ${id} deleted successfully!`);
        // Refresh results
        const currentQuery = input.value.trim();
        if (currentQuery) {
          performSearch();
        } else {
          listAllDocuments();
        }
      } catch (e) {
        alert("Delete failed: " + e.message);
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
      <h2>${doc.url || doc.filename}</h2>
      <div class="muted">Path: ${doc.path || ""}</div>
      <pre class="doc-text">${highlightText(doc.content, query)}</pre>
    </div>
  `;
  document.body.appendChild(modal);

  modal.querySelector(".close").onclick = () => modal.remove();
  modal.addEventListener("click", (e) => {
    if (e.target === modal) modal.remove();
  });

  // Close modal on Escape key
  const handleEscape = (e) => {
    if (e.key === "Escape") {
      modal.remove();
      document.removeEventListener("keydown", handleEscape);
    }
  };
  document.addEventListener("keydown", handleEscape);
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
  } catch (e) {
    console.error("Suggestion error:", e);
  }

  if (!q.includes(" ")) {
    try {
      const c = await fetchJSON(
        `/correct?word=${encodeURIComponent(q)}&max=3`
      );
      renderPills(corrEl, "Did you mean:", c, "correction");
    } catch (e) {
      console.error("Correction error:", e);
    }
  } else {
    corrEl.innerHTML = "";
  }
}, 150);

input.addEventListener("input", liveAssist);

async function performSearch() {
  const q = input.value.trim();
  if (!q) {
    resultsEl.innerHTML = `<div class="muted">Enter a search query.</div>`;
    return;
  }

  resultsEl.innerHTML = `<div class="muted">Searching...</div>`;

  try {
    const data = await fetchJSON(`/search?query=${encodeURIComponent(q)}`);
    renderResults(data);
  } catch (e) {
    resultsEl.innerHTML = `<div class="muted">Error: ${e.message}</div>`;
  }
}

// ---------- 📄 List All Documents ----------
async function listAllDocuments() {
  resultsEl.innerHTML = `<div class="muted">Loading documents...</div>`;
  try {
    const data = await fetchJSON("/documents");
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
        <li><strong>"Did You Mean…?"</strong> - Small typos are corrected using <em>fuzzy matching (BK-Tree)</em>.</li>
        <li><strong>Flexible Querying</strong> - Use <code>AND</code> / <code>OR</code> between words to refine results.</li>
        <li><strong>Relevant Ranking</strong> - Results are ordered using <em>TF-IDF scoring</em> for better relevance.</li>
        <li><strong>Preview Highlights</strong> - Search terms are shown <b><u>bold and underlined</u></b> in snippets.</li>
        <li><strong>Upload & Edit</strong> - Add new documents or modify existing ones in real-time.</li>
      </ul>
    </div>
  `;
  document.body.appendChild(modal);

  modal.querySelector(".close").onclick = () => modal.remove();
  modal.addEventListener("click", (e) => {
    if (e.target === modal) modal.remove();
  });

  const handleEscape = (e) => {
    if (e.key === "Escape") {
      modal.remove();
      document.removeEventListener("keydown", handleEscape);
    }
  };
  document.addEventListener("keydown", handleEscape);
}

const infoBtn = document.createElement("button");
infoBtn.type = "button";
infoBtn.className = "info-btn";
infoBtn.textContent = "ℹ️ How Search Works";
infoBtn.onclick = showInfoModal;
document.querySelector("#search-form").appendChild(infoBtn);

// ---------- Upload & Edit ----------
async function uploadDocument(filename, content) {
  const res = await fetch("/upload", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ filename, content }),
  });
  if (!res.ok) {
    const errorText = await res.text();
    throw new Error(errorText);
  }
  return res.json();
}

async function editDocument(id, newContent) {
  const res = await fetch(`/edit/${id}`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content: newContent }),
  });
  if (!res.ok) {
    const errorText = await res.text();
    throw new Error(errorText);
  }
  return res.json();
}

$("#upload-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const filename = $("#upload-filename").value.trim();
  const content = $("#upload-content").value.trim();

  if (!filename || !content) {
    alert("Please provide both filename and content.");
    return;
  }

  if (filename.includes("/") || filename.includes("\\")) {
    alert("Filename cannot contain path separators.");
    return;
  }

  try {
    const submitBtn = e.target.querySelector('button[type="submit"]');
    const originalText = submitBtn.textContent;
    submitBtn.textContent = "Uploading...";
    submitBtn.disabled = true;

    const res = await uploadDocument(filename, content);
    alert(
      `✅ Uploaded successfully!\nFile: ${res.filename}\nDocument ID: ${res.id}`
    );
    $("#upload-form").reset();

    const currentQuery = input.value.trim();
    if (currentQuery) {
      setTimeout(async () => {
        await performSearch();
        await liveAssist();
      }, 100);
    } else {
      listAllDocuments();
    }

    submitBtn.textContent = originalText;
    submitBtn.disabled = false;
  } catch (err) {
    alert("Upload failed: " + err.message);
    e.target.querySelector('button[type="submit"]').disabled = false;
    e.target.querySelector('button[type="submit"]').textContent = "Upload";
  }
});

$("#edit-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const id = $("#edit-id").value.trim();
  const content = $("#edit-content").value.trim();

  if (!id || !content) {
    alert("Please provide both ID and new content.");
    return;
  }

  if (isNaN(parseInt(id))) {
    alert("Document ID must be a number.");
    return;
  }

  try {
    const submitBtn = e.target.querySelector('button[type="submit"]');
    const originalText = submitBtn.textContent;
    submitBtn.textContent = "Saving...";
    submitBtn.disabled = true;

    const res = await editDocument(id, content);
    alert(`✏️ Document edited successfully! (ID: ${res.id})`);
    $("#edit-form").reset();

    const currentQuery = input.value.trim();
    if (currentQuery) {
      setTimeout(async () => {
        await performSearch();
        await liveAssist();
      }, 100);
    } else {
      listAllDocuments();
    }

    submitBtn.textContent = originalText;
    submitBtn.disabled = false;
  } catch (err) {
    alert("Edit failed: " + err.message);
    e.target.querySelector('button[type="submit"]').disabled = false;
    e.target.querySelector('button[type="submit"]').textContent =
      "Save Changes";
  }
});

// ---------- Initialize ----------
document.addEventListener("DOMContentLoaded", () => {
  console.log("Mini Search Engine initialized");
  input.focus();
  // Hook up "Show All" button
document.querySelector("#all-btn").addEventListener("click", listAllDocuments);
/*
  const allBtn = document.createElement("button");
  allBtn.type = "button";
  allBtn.id = "all-btn";
  allBtn.className = "info-btn";
  allBtn.textContent = "📄 Show All";
  allBtn.onclick = listAllDocuments;
  document.querySelector("#search-form").appendChild(allBtn);
*/
  document.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "k") {
      e.preventDefault();
      input.focus();
      input.select();
    }
  });
});
