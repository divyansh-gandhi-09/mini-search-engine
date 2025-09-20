const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => document.querySelectorAll(sel);
const resultsEl = $("#results");
const suggEl = $("#suggestions");
const corrEl = $("#corrections");
const input = $("#search-box");
const form = $("#search-form");
const statsEl = $("#stats-display");

// Global state
let currentStats = null;
let searchHistory = [];
let isOnline = true;

function debounce(fn, ms = 200) {
  let t;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}

async function fetchJSON(url, options = {}) {
  try {
    showLoadingIndicator(true);
    const r = await fetch(url, options);
    if (!r.ok) {
      let errorText;
      try {
        const errorData = await r.json();
        errorText = errorData.error || errorData.details || `HTTP ${r.status}`;
      } catch {
        errorText = await r.text() || `HTTP ${r.status}`;
      }
      throw new Error(errorText);
    }
    return r.json();
  } catch (error) {
    console.error("Fetch error:", error);
    if (error.name === 'TypeError' && error.message.includes('fetch')) {
      isOnline = false;
      showNotification("Connection lost. Please check if the server is running.", "error");
    }
    throw error;
  } finally {
    showLoadingIndicator(false);
  }
}

function showLoadingIndicator(show) {
  let indicator = $("#loading-indicator");
  if (!indicator && show) {
    indicator = document.createElement("div");
    indicator.id = "loading-indicator";
    indicator.innerHTML = '<div class="spinner"></div><span>Processing...</span>';
    document.body.appendChild(indicator);
  }
  if (indicator) {
    indicator.style.display = show ? "flex" : "none";
  }
}

function showNotification(message, type = "info") {
  const notification = document.createElement("div");
  notification.className = `notification ${type}`;
  notification.innerHTML = `
    <span>${message}</span>
    <button class="close-btn" onclick="this.parentElement.remove()">×</button>
  `;
  
  const container = $("#notifications") || (() => {
    const c = document.createElement("div");
    c.id = "notifications";
    document.body.appendChild(c);
    return c;
  })();
  
  container.appendChild(notification);
  
  // Auto-remove after 5 seconds
  setTimeout(() => {
    if (notification.parentElement) {
      notification.remove();
    }
  }, 5000);
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
    highlighted = highlighted.replace(regex, "<mark>$1</mark>");
  });
  return highlighted;
}

function formatFileSize(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function formatDate(timestamp) {
  return new Date(parseInt(timestamp) / 1000).toLocaleDateString();
}

async function loadStats() {
  try {
    currentStats = await fetchJSON("/stats");
    displayStats();
  } catch (e) {
    console.error("Failed to load stats:", e);
    if (statsEl) {
      statsEl.innerHTML = '<div class="error">Failed to load statistics</div>';
    }
  }
}

function displayStats() {
  if (!currentStats || !statsEl) return;
  
  const { total_documents, indexed_documents, vocabulary_size, folders, root_documents, file_types, total_content_size } = currentStats;
  
  statsEl.innerHTML = `
    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-number">${indexed_documents}</div>
        <div class="stat-label">Documents</div>
      </div>
      <div class="stat-card">
        <div class="stat-number">${folders.length}</div>
        <div class="stat-label">Folders</div>
      </div>
      <div class="stat-card">
        <div class="stat-number">${vocabulary_size.toLocaleString()}</div>
        <div class="stat-label">Unique Terms</div>
      </div>
      <div class="stat-card">
        <div class="stat-number">${formatFileSize(total_content_size)}</div>
        <div class="stat-label">Total Size</div>
      </div>
    </div>
  `;
}

function renderResults(arr) {
  if (!arr || arr.length === 0) {
    resultsEl.innerHTML = `
      <div class="no-results">
        <div class="no-results-icon">🔍</div>
        <div class="no-results-text">No results found</div>
        <div class="muted">Try different keywords or check your spelling</div>
      </div>
    `;
    return;
  }
  
  const q = input.value.trim();
  
  // Add search history
  if (q && !searchHistory.includes(q)) {
    searchHistory.unshift(q);
    if (searchHistory.length > 10) searchHistory.pop();
    localStorage.setItem('searchHistory', JSON.stringify(searchHistory));
  }

  const resultsHeader = `
    <div class="results-header">
      <div class="results-count">${arr.length} result${arr.length === 1 ? '' : 's'} found</div>
      <div class="results-actions">
        <button class="secondary-btn" onclick="exportResults(${JSON.stringify(arr).replace(/"/g, '&quot;')})">
          Export Results
        </button>
      </div>
    </div>
  `;

  const resultsHTML = arr
    .map(
      (r, index) => `
    <div class="result" data-id="${r.id}">
      <div class="result-header">
        <div class="result-title">
          <span class="result-index">${index + 1}</span>
          <a href="#" class="result-link" onclick="return false;">${r.url || r.filename}</a>
          <div class="result-meta">
            <span class="score-badge">Score: ${r.score ? r.score.toFixed(3) : "-"}</span>
            <span class="size-badge">${formatFileSize(r.size || 0)}</span>
          </div>
        </div>
      </div>
      <div class="result-path muted">${r.path || ""}</div>
      ${r.folder ? `<div class="folder-badge">📁 ${r.folder}</div>` : ''}
      <div class="preview" data-id="${r.id}">
        ${highlightText(r.preview || r.content?.slice(0, 200) || "", q)}
      </div>
      <div class="result-actions">
        <button class="action-btn view-btn" data-id="${r.id}" title="View full document">
          👁️ View
        </button>
        <button class="action-btn edit-btn" data-id="${r.id}" title="Edit document">
          ✏️ Edit
        </button>
        <button class="action-btn delete-btn" data-id="${r.id}" title="Delete document">
          🗑️ Delete
        </button>
      </div>
    </div>
  `
    )
    .join("");

  resultsEl.innerHTML = resultsHeader + resultsHTML;

  // Attach event listeners
  attachResultEventListeners();
}

function attachResultEventListeners() {
  // View full doc
  $$(".view-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      try {
        const doc = await fetchJSON(`/document?id=${id}`);
        showModal(doc, input.value.trim());
      } catch (e) {
        showNotification("Error loading document: " + e.message, "error");
      }
    });
  });

  // Edit doc
  $$(".edit-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      try {
        const doc = await fetchJSON(`/document?id=${id}`);
        $("#edit-id").value = id;
        $("#edit-content").value = doc.content;
        $("#edit-form").scrollIntoView({ behavior: "smooth" });
        showNotification("Document loaded for editing", "success");
      } catch (e) {
        showNotification("Error loading document for edit: " + e.message, "error");
      }
    });
  });

  // Delete document
  $$(".delete-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      const result = document.querySelector(`[data-id="${id}"]`);
      const filename = result?.querySelector('.result-link')?.textContent || `document ${id}`;
      
      if (!confirm(`Are you sure you want to delete "${filename}"?\n\nThis action cannot be undone.`)) return;

      try {
        await fetchJSON(`/delete/${id}`, { method: "DELETE" });
        showNotification(`"${filename}" deleted successfully!`, "success");
        
        // Remove from current results
        result?.remove();
        
        // Refresh results or document list
        const currentQuery = input.value.trim();
        if (currentQuery) {
          setTimeout(performSearch, 500);
        } else {
          setTimeout(listAllDocuments, 500);
        }
        
        // Reload stats
        loadStats();
      } catch (e) {
        showNotification("Delete failed: " + e.message, "error");
      }
    });
  });
}

function exportResults(results) {
  const csvContent = "data:text/csv;charset=utf-8," + 
    "ID,Filename,Folder,Score,Size,Path\n" +
    results.map(r => 
      `${r.id},"${r.url || r.filename}","${r.folder || ''}",${r.score || 0},${r.size || 0},"${r.path || ''}"`
    ).join("\n");
  
  const encodedUri = encodeURI(csvContent);
  const link = document.createElement("a");
  link.setAttribute("href", encodedUri);
  link.setAttribute("download", `search_results_${new Date().toISOString().split('T')[0]}.csv`);
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  
  showNotification("Results exported to CSV", "success");
}

function showModal(doc, query) {
  const modal = document.createElement("div");
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content">
      <div class="modal-header">
        <h2>${doc.url || doc.filename}</h2>
        <span class="close" title="Close (Esc)">&times;</span>
      </div>
      <div class="modal-meta">
        <div class="muted">Path: ${doc.path || ""}</div>
        ${doc.folder ? `<div class="muted">Folder: ${doc.folder}</div>` : ''}
        <div class="muted">Size: ${formatFileSize(doc.size || doc.content?.length || 0)}</div>
      </div>
      <div class="modal-actions">
        <button class="secondary-btn" onclick="copyToClipboard(\`${doc.content?.replace(/`/g, "\\`").replace(/\\/g, "\\\\")}\`)">
          📋 Copy Content
        </button>
        <button class="secondary-btn" onclick="downloadDocument('${doc.url || doc.filename}', \`${doc.content?.replace(/`/g, "\\`").replace(/\\/g, "\\\\")}\`)">
          💾 Download
        </button>
      </div>
      <pre class="doc-text">${highlightText(doc.content, query)}</pre>
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

function copyToClipboard(text) {
  navigator.clipboard.writeText(text).then(() => {
    showNotification("Content copied to clipboard", "success");
  }).catch(() => {
    showNotification("Failed to copy content", "error");
  });
}

function downloadDocument(filename, content) {
  const blob = new Blob([content], { type: 'text/plain' });
  const url = window.URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  window.URL.revokeObjectURL(url);
  showNotification("Document downloaded", "success");
}

// Live suggestions and corrections
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
      const c = await fetchJSON(`/correct?word=${encodeURIComponent(q)}&max=3`);
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
  const folderFilter = $("#folder-filter")?.value || "";
  
  if (!q) {
    showNotification("Please enter a search query", "warning");
    return;
  }
  
  resultsEl.innerHTML = `<div class="loading-state">🔍 Searching...</div>`;
  
  try {
    let url = `/search?query=${encodeURIComponent(q)}`;
    if (folderFilter) {
      url += `&folder=${encodeURIComponent(folderFilter)}`;
    }
    const data = await fetchJSON(url);
    renderResults(data);
    
    // Update page title
    document.title = `Search: ${q} - Mini Search Engine`;
  } catch (e) {
    resultsEl.innerHTML = `<div class="error-state">❌ Error: ${e.message}</div>`;
    showNotification("Search failed: " + e.message, "error");
  }
}

async function listAllDocuments() {
  resultsEl.innerHTML = `<div class="loading-state">📂 Loading documents...</div>`;
  
  try {
    const folderFilter = $("#folder-filter")?.value || "";
    let url = "/documents";
    if (folderFilter) {
      url += `?folder=${encodeURIComponent(folderFilter)}`;
    }
    const data = await fetchJSON(url);
    renderResults(data);
    
    document.title = "All Documents - Mini Search Engine";
  } catch (e) {
    resultsEl.innerHTML = `<div class="error-state">❌ Error: ${e.message}</div>`;
    showNotification("Failed to load documents: " + e.message, "error");
  }
}

form.addEventListener("submit", (e) => {
  e.preventDefault();
  performSearch();
});

// Show info modal
function showInfoModal() {
  const modal = document.createElement("div");
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content">
      <div class="modal-header">
        <h2>🔎 How Our Search Works</h2>
        <span class="close">&times;</span>
      </div>
      <ul class="info-list">
        <li><strong>Smart Suggestions</strong> - While you type, we suggest completions using a prefix tree (Trie).</li>
        <li><strong>"Did You Mean…?"</strong> - Small typos are corrected using fuzzy matching (BK-Tree).</li>
        <li><strong>Flexible Querying</strong> - Use <code>AND</code> / <code>OR</code> between words to refine results.</li>
        <li><strong>Relevant Ranking</strong> - Results are ordered using TF-IDF scoring for better relevance.</li>
        <li><strong>Preview Highlights</strong> - Search terms are shown highlighted in snippets.</li>
        <li><strong>File Upload & Extract</strong> - Upload PDF, images, and text files. Text is extracted automatically.</li>
        <li><strong>Folder Organization</strong> - Organize documents into folders for better management.</li>
        <li><strong>Real-time Updates</strong> - Add, edit, or delete documents with immediate re-indexing.</li>
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

// Upload form handler
$("#upload-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const fileInput = $("#upload-file");
  const filenameInput = $("#upload-filename");
  const contentInput = $("#upload-content");
  const folderInput = $("#folder-select");

  const folder = folderInput?.value || "";
  let filename = filenameInput.value.trim();
  let content = contentInput.value.trim();

  const formData = new FormData();
  const submitBtn = e.target.querySelector('button[type="submit"]');
  const originalText = submitBtn.textContent;

  if (fileInput.files.length > 0) {
    const file = fileInput.files[0];
    filename = filename || file.name;
    formData.append("file", file);
    formData.append("filename", filename);
    if (folder) formData.append("folder", folder);

    try {
      submitBtn.textContent = "Extracting & Uploading...";
      submitBtn.disabled = true;

      const res = await fetch("/upload", {
        method: "POST",
        body: formData
      });

      const data = await res.json();
      
      if (!res.ok || !data.success) {
        throw new Error(data.error || "Upload failed");
      }

      showNotification(
        `✅ "${data.filename}" uploaded successfully! Document ID: ${data.id}. Extracted: ${data.extracted_chars || 0} characters.`,
        "success"
      );

      $("#upload-form").reset();
      await Promise.all([loadFolders(), loadStats()]);
      
      // Refresh current view
      const currentQuery = input.value.trim();
      if (currentQuery) {
        setTimeout(performSearch, 500);
      } else {
        setTimeout(listAllDocuments, 500);
      }

    } catch (err) {
      showNotification("Upload failed: " + err.message, "error");
    } finally {
      submitBtn.textContent = originalText;
      submitBtn.disabled = false;
    }
  } else if (content) {
    // Manual text content
    filename = filename || "manual_note.txt";
    
    try {
      submitBtn.textContent = "Uploading...";
      submitBtn.disabled = true;

      const res = await fetch("/upload", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ filename, content, folder })
      });

      const data = await res.json();
      
      if (!res.ok || !data.success) {
        throw new Error(data.error || "Upload failed");
      }

      showNotification(
        `✅ "${data.filename}" created successfully! Document ID: ${data.id}`,
        "success"
      );

      $("#upload-form").reset();
      await Promise.all([loadFolders(), loadStats()]);
      
      const currentQuery = input.value.trim();
      if (currentQuery) {
        setTimeout(performSearch, 500);
      } else {
        setTimeout(listAllDocuments, 500);
      }

    } catch (err) {
      showNotification("Upload failed: " + err.message, "error");
    } finally {
      submitBtn.textContent = originalText;
      submitBtn.disabled = false;
    }
  } else {
    showNotification("Please provide a file or enter content.", "warning");
  }
});

// Edit form handler
$("#edit-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const id = $("#edit-id").value.trim();
  const content = $("#edit-content").value.trim();

  if (!id || !content) {
    showNotification("Please provide both ID and new content.", "warning");
    return;
  }

  if (isNaN(parseInt(id))) {
    showNotification("Document ID must be a number.", "warning");
    return;
  }

  const submitBtn = e.target.querySelector('button[type="submit"]');
  const originalText = submitBtn.textContent;

  try {
    submitBtn.textContent = "Saving...";
    submitBtn.disabled = true;

    const res = await fetch(`/edit/${id}`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ content: content }),
    });

    const data = await res.json();
    
    if (!res.ok || !data.success) {
      throw new Error(data.error || "Edit failed");
    }

    showNotification(`✏️ Document edited successfully! (ID: ${data.id})`, "success");
    $("#edit-form").reset();
    
    await loadStats();

    const currentQuery = input.value.trim();
    if (currentQuery) {
      setTimeout(async () => {
        await performSearch();
        await liveAssist();
      }, 500);
    } else {
      setTimeout(listAllDocuments, 500);
    }

  } catch (err) {
    showNotification("Edit failed: " + err.message, "error");
  } finally {
    submitBtn.textContent = originalText;
    submitBtn.disabled = false;
  }
});

// Load folders into dropdowns
async function loadFolders() {
  try {
    const folders = await fetchJSON("/folders");
    const folderSelects = $$("#folder-select, #folder-filter");
    
    folderSelects.forEach(select => {
      const isFilter = select.id === "folder-filter";
      const currentValue = select.value;
      
      select.innerHTML = isFilter 
        ? `<option value="">All Folders</option>` 
        : `<option value="">-- Select Folder --</option>`;
      
      folders.forEach(f => {
        const option = document.createElement("option");
        option.value = f;
        option.textContent = f;
        if (f === currentValue) option.selected = true;
        select.appendChild(option);
      });
    });
  } catch (err) {
    console.error("Failed to load folders:", err);
    showNotification("Failed to load folders", "error");
  }
}

// Load search history
function loadSearchHistory() {
  try {
    const saved = localStorage.getItem('searchHistory');
    if (saved) {
      searchHistory = JSON.parse(saved);
    }
  } catch (e) {
    console.error("Failed to load search history:", e);
  }
}

// Rebuild index function
async function rebuildIndex(action = 'fresh') {
  if (!confirm(`Are you sure you want to ${action === 'fresh' ? 'completely rebuild' : 'update'} the search index? This may take some time.`)) {
    return;
  }

  try {
    showNotification(`Starting index ${action === 'fresh' ? 'rebuild' : 'update'}...`, "info");
    
    const res = await fetch("/rebuild", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ action })
    });

    const data = await res.json();
    
    if (!res.ok || !data.success) {
      throw new Error(data.error || "Rebuild failed");
    }

    showNotification(`Index ${action === 'fresh' ? 'rebuild' : 'update'} completed successfully!`, "success");
    
    // Reload everything
    await Promise.all([loadFolders(), loadStats()]);
    
    // Clear suggestions/corrections cache
    suggEl.innerHTML = "";
    corrEl.innerHTML = "";
    
    // Refresh current view
    const currentQuery = input.value.trim();
    if (currentQuery) {
      setTimeout(performSearch, 1000);
    } else {
      setTimeout(listAllDocuments, 1000);
    }

  } catch (err) {
    showNotification("Index rebuild failed: " + err.message, "error");
  }
}

// Initialize everything
document.addEventListener("DOMContentLoaded", async () => {
  console.log("Mini Search Engine initialized");
  
  // Load initial data
  loadSearchHistory();
  await Promise.all([loadFolders(), loadStats()]);
  
  input.focus();

  // Setup event listeners
  const allBtn = $("#all-btn");
  if (allBtn) {
    allBtn.addEventListener("click", listAllDocuments);
  }

  const infoBtn = $("#info-btn");
  if (infoBtn) {
    infoBtn.addEventListener("click", showInfoModal);
  }

  // Add rebuild buttons if they exist
  const rebuildFreshBtn = $("#rebuild-fresh");
  if (rebuildFreshBtn) {
    rebuildFreshBtn.addEventListener("click", () => rebuildIndex('fresh'));
  }

  const rebuildUpdateBtn = $("#rebuild-update");
  if (rebuildUpdateBtn) {
    rebuildUpdateBtn.addEventListener("click", () => rebuildIndex('update'));
  }

  // Folder filter change handler
  const folderFilter = $("#folder-filter");
  if (folderFilter) {
    folderFilter.addEventListener("change", () => {
      const currentQuery = input.value.trim();
      if (currentQuery) {
        performSearch();
      } else {
        listAllDocuments();
      }
    });
  }

  // Keyboard shortcuts
  document.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "k") {
      e.preventDefault();
      input.focus();
      input.select();
    }
    
    if (e.key === "Escape") {
      input.blur();
      // Close any open modals
      const modals = $$(".modal");
      modals.forEach(modal => modal.remove());
    }
  });

  // Check server connectivity on startup
  try {
    await fetchJSON("/health");
    isOnline = true;
    showNotification("Connected to search engine", "success");
  } catch (e) {
    isOnline = false;
    showNotification("Cannot connect to server. Please check if it's running.", "error");
  }
});