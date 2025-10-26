// batch-upload.js - Enhanced Batch Upload Integration
// This seamlessly integrates with your existing app.js

document.addEventListener('DOMContentLoaded', () => {
    // Wait for main app to initialize
    setTimeout(initBatchUpload, 500);

    let batchFiles = [];
    let folderFiles = [];
    let folderStructureMap = new Map();

    function initBatchUpload() {
        console.log('🚀 Initializing batch upload system...');
        
        // Setup mode switching
        setupModeToggle();
        
        // Setup single file upload with smart routing
        setupSingleUpload();
        
        // Setup batch upload
        setupBatchUpload();
        
        // Setup folder upload
        setupFolderUpload();
        
        // Load folders
        loadBatchFolders();
        
        console.log('✅ Batch upload system ready');
    }

    // ========================================
    // MODE TOGGLE
    // ========================================
    function setupModeToggle() {
        document.querySelectorAll('.mode-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const mode = btn.dataset.mode;
                
                // Update active button
                document.querySelectorAll('.mode-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                
                // Show corresponding content
                document.querySelectorAll('.upload-mode-content').forEach(c => c.classList.remove('active'));
                const modeContent = document.getElementById(`${mode}-mode`);
                if (modeContent) {
                    modeContent.classList.add('active');
                }
            });
        });
    }

    // ========================================
    // SINGLE FILE UPLOAD WITH SMART ROUTING
    // ========================================
    function setupSingleUpload() {
        const dropzone = document.getElementById('singleDropzone');
        const fileInput = document.getElementById('upload-file');
        
        if (!dropzone || !fileInput) return;

        // Dropzone click handler
        dropzone.addEventListener('click', (e) => {
            if (e.target.tagName !== 'BUTTON') {
                fileInput.click();
            }
        });

        // Drag and drop
        dropzone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropzone.classList.add('dragover');
        });

        dropzone.addEventListener('dragleave', () => {
            dropzone.classList.remove('dragover');
        });

        dropzone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropzone.classList.remove('dragover');
            
            if (e.dataTransfer.files.length > 0) {
                fileInput.files = e.dataTransfer.files;
                updateDropzoneText(dropzone, e.dataTransfer.files[0].name);
            }
        });

        // File selection handler
        fileInput.addEventListener('change', () => {
            if (fileInput.files.length > 0) {
                updateDropzoneText(dropzone, fileInput.files[0].name);
            }
        });
    }

    function updateDropzoneText(dropzone, filename) {
        const h3 = dropzone.querySelector('h3');
        if (h3) {
            h3.textContent = `✓ Selected: ${filename}`;
        }
    }

    // ========================================
    // BATCH UPLOAD
    // ========================================
    function setupBatchUpload() {
        const dropzone = document.getElementById('batchDropzone');
        const fileInput = document.getElementById('batchFileInput');
        
        if (!dropzone || !fileInput) return;

        // Dropzone interactions
        dropzone.addEventListener('click', () => fileInput.click());
        
        dropzone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropzone.classList.add('dragover');
        });
        
        dropzone.addEventListener('dragleave', () => {
            dropzone.classList.remove('dragover');
        });
        
        dropzone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropzone.classList.remove('dragover');
            handleBatchFiles(Array.from(e.dataTransfer.files));
        });

        // File input handler
        fileInput.addEventListener('change', (e) => {
            handleBatchFiles(Array.from(e.target.files));
        });

        // Action buttons
        const uploadBtn = document.getElementById('batchUploadBtn');
        const clearBtn = document.getElementById('batchClearBtn');

        if (uploadBtn) uploadBtn.addEventListener('click', executeBatchUpload);
        if (clearBtn) clearBtn.addEventListener('click', clearBatchQueue);
    }

    function handleBatchFiles(files) {
        if (files.length === 0) return;
        
        //  Add warning for very large uploads instead:
        if (files.length > 1000) {
            if (!confirm(`You're uploading ${files.length} files. This may take a while. Continue?`)) {
                return;
            }
        }

        batchFiles = files;
        displayBatchQueue();
        
        // Show UI elements
        showElement('batchProgressSection');
        showElement('fileQueueContainer');
        showElement('batchActionButtons');
        
        // Reset stats
        updateStat('batchStatTotal', files.length);
        updateStat('batchStatSuccess', 0);
        updateStat('batchStatFailed', 0);
        updateProgress('batchProgressFill', 'progressPercentage', 0);
    }

    function displayBatchQueue() {
        const queue = document.getElementById('fileQueue');
        if (!queue) return;
        
        queue.innerHTML = '';
        
        batchFiles.forEach((file, index) => {
            const item = createQueueItem(file.name, file.size, index, 'queue');
            queue.appendChild(item);
        });
    }

    async function executeBatchUpload() {
    const btn = document.getElementById('batchUploadBtn');
    if (!btn) return;
    
    btn.disabled = true;
    btn.textContent = '⏳ Uploading...';
    
    const folder = document.getElementById('batch-folder-select')?.value || '';
    
    try {
        const formData = new FormData();
        
        batchFiles.forEach(file => {
            formData.append('files', file);
        });
        
        if (folder) formData.append('folder', folder);

        // ✅ REMOVED: Fake progress loop
        // Just show initial state
        updateProgress('batchProgressFill', 'progressPercentage', 0);
        
        batchFiles.forEach((_, i) => {
            updateQueueItemStatus(i, 'processing', 'Uploading...');
        });

        // ✅ Actually upload (the C++ side is fast!)
        console.time('Batch Upload');
        const response = await fetch('/upload/batch', {
            method: 'POST',
            body: formData
        });
        console.timeEnd('Batch Upload');

        const result = await response.json();
        
        // Update stats
        updateStat('batchStatSuccess', result.successful || 0);
        updateStat('batchStatFailed', result.failed || 0);
        
        // Update individual items
        if (result.results) {
            result.results.forEach((fileResult, index) => {
                const status = fileResult.success ? 'success' : 'error';
                const text = fileResult.success 
                    ? '✓ Success' 
                    : `✗ ${fileResult.error || 'Failed'}`;
                updateQueueItemStatus(index, status, text);
            });
        }

        updateProgress('batchProgressFill', 'progressPercentage', 100);
        
        const message = result.failed === 0
            ? `✅ All ${result.successful} files uploaded successfully!`
            : `⚠️ Upload complete: ${result.successful} succeeded, ${result.failed} failed`;
        
        showNotification(message, result.failed === 0 ? 'success' : 'warning');
        
        window.dispatchEvent(new CustomEvent('refreshStats'));
        
    } catch (error) {
        console.error('Batch upload error:', error);
        showNotification('❌ Upload error: ' + error.message, 'error');
    } finally {
        btn.disabled = false;
        btn.textContent = '🚀 Upload All Files';
    }
}

    function clearBatchQueue() {
        batchFiles = [];
        const queue = document.getElementById('fileQueue');
        if (queue) queue.innerHTML = '';
        
        hideElement('batchProgressSection');
        hideElement('fileQueueContainer');
        hideElement('batchActionButtons');
        
        const fileInput = document.getElementById('batchFileInput');
        if (fileInput) fileInput.value = '';
        
        updateProgress('batchProgressFill', 'progressPercentage', 0);
    }

    // ========================================
    // FOLDER UPLOAD
    // ========================================
    function setupFolderUpload() {
        const dropzone = document.getElementById('folderDropzone');
        const folderInput = document.getElementById('folderInput');
        
        if (!dropzone || !folderInput) return;

        dropzone.addEventListener('click', () => folderInput.click());
        
        dropzone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropzone.classList.add('dragover');
        });
        
        dropzone.addEventListener('dragleave', () => {
            dropzone.classList.remove('dragover');
        });
        
        dropzone.addEventListener('drop', async (e) => {
            e.preventDefault();
            dropzone.classList.remove('dragover');
            
            const items = e.dataTransfer.items;
            const files = [];
            
            if (items) {
                for (let i = 0; i < items.length; i++) {
                    const item = items[i].webkitGetAsEntry();
                    if (item) {
                        await traverseFileTree(item, files);
                    }
                }
                handleFolderFiles(files);
            }
        });

        folderInput.addEventListener('change', (e) => {
            const files = Array.from(e.target.files);
            files.forEach(file => {
                const relativePath = file.webkitRelativePath || file.name;
                folderStructureMap.set(file, relativePath);
            });
            handleFolderFiles(files);
        });

        // Action buttons
        const uploadBtn = document.getElementById('folderUploadBtn');
        const clearBtn = document.getElementById('folderClearBtn');

        if (uploadBtn) uploadBtn.addEventListener('click', executeFolderUpload);
        if (clearBtn) clearBtn.addEventListener('click', clearFolderQueue);
    }

    async function traverseFileTree(item, files, path = '') {
        if (item.isFile) {
            return new Promise((resolve) => {
                item.file((file) => {
                    const relativePath = path + file.name;
                    folderStructureMap.set(file, relativePath);
                    files.push(file);
                    resolve();
                });
            });
        } else if (item.isDirectory) {
            const dirReader = item.createReader();
            return new Promise((resolve) => {
                dirReader.readEntries(async (entries) => {
                    for (const entry of entries) {
                        await traverseFileTree(entry, files, path + item.name + '/');
                    }
                    resolve();
                });
            });
        }
    }

    function handleFolderFiles(files) {
        if (files.length === 0) return;
        
        folderFiles = files;
        displayFolderQueue();
        
        showElement('folderProgressSection');
        showElement('folderQueueContainer');
        showElement('folderActionButtons');
        
        updateStat('folderStatTotal', files.length);
        updateStat('folderStatSuccess', 0);
        updateStat('folderStatFailed', 0);
        updateProgress('folderProgressFill', 'folderProgressPercentage', 0);
    }

    function displayFolderQueue() {
        const queue = document.getElementById('folderQueue');
        if (!queue) return;
        
        queue.innerHTML = '';
        
        folderFiles.forEach((file, index) => {
            const relativePath = folderStructureMap.get(file) || file.name;
            const item = createQueueItem(relativePath, file.size, index, 'folder');
            queue.appendChild(item);
        });
    }

    async function executeFolderUpload() {
    const btn = document.getElementById('folderUploadBtn');
    if (!btn) return;
    
    btn.disabled = true;
    btn.textContent = '⏳ Uploading Folder...';
    
    try {
        const formData = new FormData();
        
        folderFiles.forEach(file => {
            const relativePath = folderStructureMap.get(file) || file.name;
            formData.append(`file-${relativePath}`, file);
        });

        // ✅ REMOVED: Fake progress loop
        updateProgress('folderProgressFill', 'folderProgressPercentage', 0);
        
        folderFiles.forEach((_, i) => {
            updateFolderItemStatus(i, 'processing', 'Uploading...');
        });

        // ✅ Actually upload
        console.time('Folder Upload');
        const response = await fetch('/upload/folder', {
            method: 'POST',
            body: formData
        });
        console.timeEnd('Folder Upload');

        const result = await response.json();
        
        updateStat('folderStatSuccess', result.successful || 0);
        updateStat('folderStatFailed', result.failed || 0);
        
        if (result.results) {
            result.results.forEach((fileResult, index) => {
                const status = fileResult.success ? 'success' : 'error';
                const text = fileResult.success 
                    ? '✓ Success' 
                    : `✗ ${fileResult.error || 'Failed'}`;
                updateFolderItemStatus(index, status, text);
            });
        }

        updateProgress('folderProgressFill', 'folderProgressPercentage', 100);

        const message = result.failed === 0
            ? `✅ Folder uploaded: ${result.successful} files successful!`
            : `⚠️ Folder upload complete: ${result.successful} succeeded, ${result.failed} failed`;
        
        showNotification(message, result.failed === 0 ? 'success' : 'warning');
        
        window.dispatchEvent(new CustomEvent('refreshStats'));
        
    } catch (error) {
        console.error('Folder upload error:', error);
        showNotification('❌ Upload error: ' + error.message, 'error');
    } finally {
        btn.disabled = false;
        btn.textContent = '🚀 Upload Folder';
    }
}
    


    function clearFolderQueue() {
        folderFiles = [];
        folderStructureMap.clear();
        
        const queue = document.getElementById('folderQueue');
        if (queue) queue.innerHTML = '';
        
        hideElement('folderProgressSection');
        hideElement('folderQueueContainer');
        hideElement('folderActionButtons');
        
        const folderInput = document.getElementById('folderInput');
        if (folderInput) folderInput.value = '';
        
        updateProgress('folderProgressFill', 'folderProgressPercentage', 0);
    }

    // ========================================
    // UTILITY FUNCTIONS
    // ========================================
    async function loadBatchFolders() {
        try {
            const response = await fetch('/folders');
            const folders = await response.json();
            
            const selects = ['folder-select', 'batch-folder-select'];
            selects.forEach(selectId => {
                const select = document.getElementById(selectId);
                if (select) {
                    select.innerHTML = '<option value="">Root (No Folder)</option>';
                    folders.forEach(f => {
                        const option = document.createElement('option');
                        option.value = f;
                        option.textContent = f;
                        select.appendChild(option);
                    });
                }
            });
        } catch (error) {
            console.error('Failed to load folders:', error);
        }
    }

    function createQueueItem(name, size, index, prefix) {
        const item = document.createElement('div');
        item.className = 'file-queue-item';
        item.id = `${prefix}-item-${index}`;
        item.innerHTML = `
            <div class="file-info">
                <div class="file-name">${escapeHtml(name)}</div>
                <div class="file-meta">${formatFileSize(size)}</div>
            </div>
            <div class="file-status status-pending" id="${prefix}-status-${index}">Pending</div>
        `;
        return item;
    }

    function updateQueueItemStatus(index, status, text) {
        const itemEl = document.getElementById(`queue-item-${index}`);
        const statusEl = document.getElementById(`queue-status-${index}`);
        
        if (itemEl && statusEl) {
            itemEl.classList.remove('processing', 'success', 'error');
            if (status !== 'pending') itemEl.classList.add(status);
            
            statusEl.className = `file-status status-${status}`;
            statusEl.textContent = text;
        }
    }

    function updateFolderItemStatus(index, status, text) {
        const itemEl = document.getElementById(`folder-item-${index}`);
        const statusEl = document.getElementById(`folder-status-${index}`);
        
        if (itemEl && statusEl) {
            itemEl.classList.remove('processing', 'success', 'error');
            if (status !== 'pending') itemEl.classList.add(status);
            
            statusEl.className = `file-status status-${status}`;
            statusEl.textContent = text;
        }
    }

    function updateProgress(fillId, percentId, value) {
        const fill = document.getElementById(fillId);
        const percent = document.getElementById(percentId);
        
        if (fill) fill.style.width = `${value}%`;
        if (percent) percent.textContent = `${Math.round(value)}%`;
    }

    function updateStat(statId, value) {
        const stat = document.getElementById(statId);
        if (stat) stat.textContent = value;
    }

    function showElement(id) {
        const el = document.getElementById(id);
        if (el) el.style.display = 'block';
    }

    function hideElement(id) {
        const el = document.getElementById(id);
        if (el) el.style.display = 'none';
    }

    function formatFileSize(bytes) {
        if (!bytes && bytes !== 0) return '0 B';
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
    }

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    function showNotification(message, type = 'info') {
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.innerHTML = `
            <span>${escapeHtml(message)}</span>
            <button class="close-btn" onclick="this.parentElement.remove()">×</button>
        `;

        let container = document.getElementById('notifications');
        if (!container) {
            container = document.createElement('div');
            container.id = 'notifications';
            container.style.cssText = 'position:fixed;top:20px;right:20px;z-index:10000;display:flex;flex-direction:column;gap:10px;max-width:400px;';
            document.body.appendChild(container);
        }

        container.appendChild(notification);

        setTimeout(() => {
            if (notification.parentElement) notification.remove();
        }, 5000);
    }
});