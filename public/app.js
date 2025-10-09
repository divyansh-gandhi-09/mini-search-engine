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
let availableFolders = []; // Store folders globally

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
    indicator.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:white;padding:2rem;border-radius:12px;box-shadow:0 10px 30px rgba(0,0,0,0.3);z-index:9999;display:flex;flex-direction:column;align-items:center;gap:1rem;';
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
  
  let container = $("#notifications");
  if (!container) {
    container = document.createElement("div");
    container.id = "notifications";
    container.style.cssText = 'position:fixed;top:20px;right:20px;z-index:10000;display:flex;flex-direction:column;gap:10px;max-width:400px;';
    document.body.appendChild(container);
  }
  
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
  
  const resultsHeader = `
    <div class="results-header">
      <div class="results-count">${arr.length} result${arr.length === 1 ? '' : 's'} found</div>
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
        <select class="action-btn move-folder-select" data-id="${r.id}" data-current-folder="${r.folder || ''}">
          <option value="">Move to folder...</option>
        </select>
      </div>
    </div>
  `
    )
    .join("");

  resultsEl.innerHTML = resultsHeader + resultsHTML;

  attachResultEventListeners();
  populateMoveToFolderDropdowns();
}

async function populateMoveToFolderDropdowns() {
  console.log("Populating move-to-folder dropdowns...");
  console.log("Available folders:", availableFolders);
  
  $$(".move-folder-select").forEach(select => {
    const currentDocFolder = select.dataset.currentFolder || "";
    
    // Clear and rebuild options
    select.innerHTML = '<option value="">Move to folder...</option>';
    
    // Add all folders except the current one
    availableFolders.forEach(folder => {
      if (folder !== currentDocFolder) { // Don't show current folder
        const option = document.createElement("option");
        option.value = folder;
        option.textContent = folder;
        select.appendChild(option);
      }
    });
    
    // Add "Remove from folder" option for documents currently in folders
    if (currentDocFolder) {
      const removeOption = document.createElement("option");
      removeOption.value = "__REMOVE__";
      removeOption.textContent = "📁 Remove from folder (move to root)";
      select.appendChild(removeOption);
    }
    
    // Set up change handler
    if (!select.hasAttribute('data-handler-set')) {
      select.addEventListener("change", async (e) => {
        if (e.target.value) {
          const targetFolder = e.target.value === "__REMOVE__" ? "" : e.target.value;
          await moveDocumentToFolder(e.target.dataset.id, targetFolder);
          e.target.value = ""; // Reset dropdown
        }
      });
      select.setAttribute('data-handler-set', 'true');
    }
  });
}

async function moveDocumentToFolder(docId, folderName) {
  try {
    const res = await fetch(`/documents/${docId}/folder`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ folder: folderName })
    });

    const data = await res.json();
    
    if (!res.ok || !data.success) {
      throw new Error(data.error || "Move failed");
    }

    const folderText = folderName ? `"${folderName}" folder` : "root (no folder)";
    showNotification(`Document moved to ${folderText} successfully!`, "success");
    
    // Refresh everything
    await loadStats();
    await loadFolders();
    
    const currentQuery = input.value.trim();
    if (currentQuery) {
      setTimeout(performSearch, 500);
    } else {
      setTimeout(listAllDocuments, 500);
    }
    
  } catch (err) {
    showNotification("Move failed: " + err.message, "error");
  }
}

function attachResultEventListeners() {
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

  $$(".delete-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      const result = document.querySelector(`[data-id="${id}"]`);
      const filename = result?.querySelector('.result-link')?.textContent || `document ${id}`;
      
      if (!confirm(`Are you sure you want to delete "${filename}"?\n\nThis action cannot be undone.`)) return;

      try {
        await fetchJSON(`/delete/${id}`, { method: "DELETE" });
        showNotification(`"${filename}" deleted successfully!`, "success");
        
        result?.remove();
        
        await loadStats();
        await loadFolders();
        
        const currentQuery = input.value.trim();
        if (currentQuery) {
          setTimeout(performSearch, 500);
        } else {
          setTimeout(listAllDocuments, 500);
        }
      } catch (e) {
        showNotification("Delete failed: " + e.message, "error");
      }
    });
  });
}

function showModal(doc, query) {
  const modal = document.createElement("div");
  modal.className = "modal";
  
  // Check if content is too large
  const isLargeFile = doc.content && doc.content.length > 100000;
  const displayContent = isLargeFile ? 
    doc.content.substring(0, 50000) + "\n\n... [Content truncated for display - use Download button to get full file] ..." :
    doc.content;
  
  modal.innerHTML = `
    <div class="modal-content">
      <div class="modal-header">
        <h2>📄 ${doc.url || doc.filename}</h2>
        <span class="close" title="Close (Esc)">&times;</span>
      </div>
      <div class="modal-meta">
        <div class="muted"><strong>Path:</strong> ${doc.path || ""}</div>
        ${doc.folder ? `<div class="muted"><strong>Folder:</strong> ${doc.folder}</div>` : ''}
        <div class="muted"><strong>Size:</strong> ${formatFileSize(doc.size || doc.content?.length || 0)}</div>
      </div>
      ${isLargeFile ? '<div class="large-file-warning">⚠️ Large file detected - showing first 50KB only. Use Download button for full content.</div>' : ''}
      <div class="modal-actions">
        <button class="modal-btn" onclick="copyToClipboard(\`${(displayContent || '').replace(/`/g, "\\`").replace(/\\/g, "\\\\")}\`)">
          📋 Copy ${isLargeFile ? 'Visible' : ''} Content
        </button>
        <button class="modal-btn" onclick="downloadDocument('${doc.url || doc.filename}', \`${(doc.content || '').replace(/`/g, "\\`").replace(/\\/g, "\\\\")}\`)">
          💾 Download Full File
        </button>
        <button class="modal-btn" onclick="this.closest('.modal').remove()">
          ✖️ Close
        </button>
      </div>
      <div class="doc-content-wrapper">
        <pre class="doc-text">${highlightText(displayContent || '', query)}</pre>
      </div>
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

window.copyToClipboard = function(text) {
  navigator.clipboard.writeText(text).then(() => {
    showNotification("Content copied to clipboard", "success");
  }).catch(() => {
    showNotification("Failed to copy content", "error");
  });
};

window.downloadDocument = function(filename, content) {
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
};

// Live suggestions and corrections
const liveAssist = debounce(async () => {
  const q = input.value.trim();
  if (q.length < 2) {
    suggEl.innerHTML = "";
    corrEl.innerHTML = "";
    return;
  }

  const folderFilter = $("#folder-filter")?.value || "";

  try {
    let suggestionUrl = `/suggest?prefix=${encodeURIComponent(q)}`;
    if (folderFilter) {
      suggestionUrl += `&folder=${encodeURIComponent(folderFilter)}`;
    }
    
    const s = await fetchJSON(suggestionUrl);
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

// Enhanced info modal with technical details
function showInfoModal() {
  const modal = document.createElement("div");
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content info-modal">
      <div class="modal-header">
        <h2>🔎 Mini Search Engine - Technical Overview</h2>
        <span class="close">&times;</span>
      </div>
      <div class="info-content">
        <h3>🚀 Core Features</h3>
        <ul class="info-list">
          <li><strong>Smart Auto-Completion</strong> - Uses a Trie (prefix tree) data structure for O(k) lookup time, where k is the length of the prefix.</li>
          <li><strong>Fuzzy Spell Correction</strong> - BK-Tree with Levenshtein distance algorithm corrects typos within edit distance of 2.</li>
          <li><strong>TF-IDF Ranking</strong> - Results are ranked using Term Frequency-Inverse Document Frequency scoring for relevance.</li>
          <li><strong>Boolean Queries</strong> - Supports AND/OR operators for precise search refinement.</li>
          <li><strong>Folder Organization</strong> - Hierarchical document management with folder-based filtering.</li>
        </ul>

        <h3>🔧 Search Algorithms</h3>
        <ul class="info-list">
          <li><strong>Inverted Index</strong> - Maps terms to document IDs for O(1) term lookup.</li>
          <li><strong>TF-IDF Formula</strong> - <code>score = (1 + log(tf)) × log((N + 1) / (df + 1)) + 1</code></li>
          <li><strong>Tokenization</strong> - Case-insensitive, alphanumeric tokenization with stop-word filtering.</li>
          <li><strong>AND Operation</strong> - Set intersection of posting lists.</li>
          <li><strong>OR Operation</strong> - Set union of posting lists.</li>
        </ul>

        <h3>📊 Data Structures</h3>
        <ul class="info-list">
          <li><strong>Trie</strong> - 256-way branching for ASCII character support, enables prefix-based suggestions.</li>
          <li><strong>BK-Tree</strong> - Metric tree using edit distance for spell correction, prunes search space efficiently.</li>
          <li><strong>Hash Maps</strong> - Used for O(1) document ID to metadata lookups.</li>
          <li><strong>Posting Lists</strong> - Store document IDs and term frequencies for each vocabulary term.</li>
        </ul>

        <h3>💡 Usage Tips</h3>
        <ul class="info-list">
          <li>Use <code>word1 AND word2</code> to find documents containing both terms.</li>
          <li>Use <code>word1 OR word2</code> (or just <code>word1 word2</code>) for documents with either term.</li>
          <li>Suggestions appear as you type after 2 characters.</li>
          <li>Upload PDF, images, Word docs, and text files - content is auto-extracted.</li>
          <li>Organize documents into folders for better management.</li>
          <li>Edit and delete operations automatically re-index affected documents.</li>
        </ul>

        <h3>⚡ Performance</h3>
        <ul class="info-list">
          <li><strong>Index Build</strong> - Batch processing for optimal performance (~1-5ms per document).</li>
          <li><strong>Search Speed</strong> - Sub-millisecond queries on indexes up to 10,000 documents.</li>
          <li><strong>Memory Usage</strong> - Approximately 1KB per document in memory.</li>
          <li><strong>Persistence</strong> - JSON-based index storage with atomic write operations.</li>
        </ul>
      </div>
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

// Upload form handler - FIXED VERSION
$("#upload-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const fileInput = $("#upload-file");
  const filenameInput = $("#upload-filename");
  const contentInput = $("#upload-content");
  const folderInput = $("#folder-select");

  const folder = folderInput?.value || "";
  let filename = filenameInput.value.trim();
  let content = contentInput.value.trim();

  const submitBtn = e.target.querySelector('button[type="submit"]');
  const originalText = submitBtn.textContent;

  console.log("Upload attempt - Folder:", folder, "Filename:", filename);

  if (fileInput.files.length > 0) {
    const file = fileInput.files[0];
    filename = filename || file.name; // Use custom name or original
    
    const formData = new FormData();
    formData.append("file", file);
    formData.append("filename", filename); // Send custom filename
    if (folder) {
      formData.append("folder", folder); // Send folder
      console.log("Adding folder to formData:", folder);
    }

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
        `✅ "${data.filename}" uploaded successfully!${data.folder ? ` to folder "${data.folder}"` : ''} Document ID: ${data.id}`,
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
        `✅ "${data.filename}" created successfully!${data.folder ? ` in folder "${data.folder}"` : ''} Document ID: ${data.id}`,
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

// Load folders into dropdowns - FIXED VERSION
async function loadFolders() {
  try {
    const folders = await fetchJSON("/folders");
    availableFolders = folders; // Store globally
    console.log("Loaded folders:", availableFolders);
    
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

// Create folder function - FIXED to update stats
async function createFolder() {
  const folderName = $("#new-folder-name").value.trim();
  if (!folderName) {
    showNotification("Please enter a folder name", "warning");
    return;
  }

  try {
    const response = await fetch("/folders", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: folderName })
    });

    const data = await response.json();
    
    if (!response.ok || !data.success) {
      throw new Error(data.error || "Failed to create folder");
    }

    showNotification(`Folder "${folderName}" created successfully!`, "success");
    $("#new-folder-name").value = "";
    
    // Reload both folders and stats to update folder count
    await Promise.all([loadFolders(), loadStats()]);
    
  } catch (err) {
    showNotification("Create folder failed: " + err.message, "error");
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
    
    await Promise.all([loadFolders(), loadStats()]);
    
    suggEl.innerHTML = "";
    corrEl.innerHTML = "";
    
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

  const rebuildFreshBtn = $("#rebuild-fresh");
  if (rebuildFreshBtn) {
    rebuildFreshBtn.addEventListener("click", () => rebuildIndex('fresh'));
  }

  const rebuildUpdateBtn = $("#rebuild-update");
  if (rebuildUpdateBtn) {
    rebuildUpdateBtn.addEventListener("click", () => rebuildIndex('update'));
  }

  const createFolderBtn = $("#create-folder-btn");
  if (createFolderBtn) {
    createFolderBtn.addEventListener("click", createFolder);
  }

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
      const modals = $(".modal");
      modals.forEach(modal => modal.remove());
    }
  });

  // Check server connectivity
  try {
    await fetchJSON("/health");
    isOnline = true;
    showNotification("Connected to search engine", "success");
  } catch (e) {
    isOnline = false;
    showNotification("Cannot connect to server. Please check if it's running.", "error");
  }
});