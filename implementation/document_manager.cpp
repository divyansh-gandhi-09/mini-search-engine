#include "document_manager.h"
#include "binary_persistence.h"
#include "parser.h"
#include "ranker.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <thread>
namespace fs = std::filesystem;

DocumentManager::DocumentManager() : docID(0) {}

bool DocumentManager::initialize() {
    std::unordered_map<int, std::string> loadedDocIdToPath;
    std::unordered_map<int, std::string> loadedDocIdToRel;
    std::unordered_map<int, std::vector<std::string>> loadedDocTokens;
    std::unordered_map<int, std::string> loadedDocMeta;
    std::unordered_map<std::string, int> loadedVocabCount;
    std::unordered_map<int, std::string> loadedDocIdToFolder;
    int loadedDocID=0;

    //  NEW: Uses PersistenceManager which tries binary first, then JSON
    if (::loadIndex(indexer, loadedDocIdToPath, loadedDocIdToRel,
                                      loadedDocTokens, loadedDocMeta,
                                      loadedVocabCount, loadedDocIdToFolder, loadedDocID)) {
        updateFromPersistence(loadedDocIdToPath, loadedDocIdToRel,
                              loadedDocTokens, loadedDocMeta,
                              loadedVocabCount, loadedDocIdToFolder, loadedDocID);
    
        //  OPTIMIZED: Don't load content into memory (lazy loading)
        rebuildSearchStructures(true); // true = clear content for lazy loading
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        
        std::cout<<"==================================\n";
        std::cout << " Loaded index with " << docID << " documents\n";
        
        std::unordered_set<std::string> folders;
        for (const auto& [id, folder] : docIdToFolder) {
            if (!folder.empty()) folders.insert(folder);
        }
        if (!folders.empty()) {
            std::cout << " Found " << folders.size() << " folders\n";
            std::cout<<"==================================\n";
        }
        
        return true;
    }
    return false;
}

std::string DocumentManager::extractFolderFromPath(const std::string& filepath) {
    fs::path p(filepath);
    
    if (!fs::exists("./data")) {
        return "";
    }
    
    try {
        fs::path dataDir = fs::canonical("./data");
        
        fs::path absPath;
        if (fs::exists(p)) {
            absPath = fs::canonical(p);
        } else {
            absPath = fs::absolute(p);
        }
        
        auto [it1, it2] = std::mismatch(dataDir.begin(), dataDir.end(), 
                                        absPath.begin(), absPath.end());
        if (it1 != dataDir.end()) {
            return "";
        }
        
        fs::path relativePath = fs::relative(absPath, dataDir);
        if (relativePath.parent_path() != ".") {
            return relativePath.parent_path().string();
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not extract folder from path " << filepath 
                  << ": " << e.what() << "\n";
    }
    
    return "";
}

void DocumentManager::buildFreshIndex() {
    std::cout << "\n==========Building fresh index...==========\n";
    
    indexer.clear();
    autoComplete.clear();
    typoCorrector.clear();
    docTokens.clear();
    docIdToPath.clear();
    docIdToRel.clear();
    docIdToContent.clear();  // Will be lazy-loaded
    docIdToFolder.clear();
    vocabCount.clear();
    docID = 0;

    if (!fs::exists("./data")) {
        fs::create_directories("./data");
        std::cout << "Created ./data directory\n";
    }

    std::vector<fs::directory_entry> files;
    std::function<void(const fs::path&)> collectFiles = [&](const fs::path& dir) {
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry);
                } else if (entry.is_directory()) {
                    collectFiles(entry.path());
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not access directory " << dir << ": " << e.what() << "\n";
        }
    };
    
    collectFiles("./data");

    if (files.empty()) {
        std::cout << "No files found in ./data folder.\n";
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        return;
    }

    std::cout << "Indexing " << files.size() << " files from ./data folder...\n";

    auto startTime = std::chrono::steady_clock::now();
    size_t totalChars = 0;
    std::unordered_set<std::string> folders;

    // Pre-allocate all maps
    docIdToPath.reserve(files.size());
    docIdToRel.reserve(files.size());
    // ✅ DON'T pre-allocate docIdToContent - lazy load only
    docIdToFolder.reserve(files.size());
    docTokens.reserve(files.size());
    docMeta.reserve(files.size());

    const size_t BATCH_SIZE = 100;
    for (size_t batch_start = 0; batch_start < files.size(); batch_start += BATCH_SIZE) {
        size_t batch_end = std::min(batch_start + BATCH_SIZE, files.size());
        
        for (size_t i = batch_start; i < batch_end; ++i) {
            const auto& entry = files[i];
            std::string path = entry.path().string();
            
            try {
                std::string content = Parser::readFile(path);
                if (content.empty()) {
                    std::cout << "Warning: Empty or unreadable file: " << path << "\n";
                    continue;
                }

                auto tokens = Parser::tokenize(content);
                if (tokens.empty()) continue;

                docTokens[docID] = std::move(tokens);
                // ✅ DON'T store content in memory - will lazy load
                // docIdToContent[docID] = std::move(content);  // REMOVED
                docIdToPath[docID] = path;
                docIdToRel[docID] = entry.path().filename().string();
                
                std::string folder = extractFolderFromPath(path);
                docIdToFolder[docID] = folder;
                if (!folder.empty()) folders.insert(folder);

                try {
                    docMeta[docID] = std::to_string(fs::last_write_time(path).time_since_epoch().count());
                } catch (const std::exception&) {
                    docMeta[docID] = std::to_string(std::time(nullptr));
                }

                totalChars += content.size();  // Just count, don't store
                ++docID;

            } catch (const std::exception& e) {
                std::cout << "Error processing file " << path << ": " << e.what() << "\n";
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        std::cout << "Progress: " << batch_end << "/" << files.size() 
                  << " files (" << std::fixed << std::setprecision(1) 
                  << (100.0 * batch_end / files.size()) << "%) - "
                  << elapsed << "ms\n";
    }
    std:: cout<<"=============================================\n";
    std::cout << "Building inverted index...\n";               
    auto index_start = std::chrono::steady_clock::now();
    
    std::unordered_map<std::string, std::unordered_map<int,int>> tempIndex;
    std::unordered_set<std::string> allTerms;
    
    size_t estimatedVocab = 0;
    for (const auto& [docId, tokens] : docTokens) {
        estimatedVocab += tokens.size();
    }
    estimatedVocab = std::min(estimatedVocab / 3, estimatedVocab);
    allTerms.reserve(estimatedVocab);
    
    for (const auto& [docId, tokens] : docTokens) {
        std::unordered_map<std::string, int> termFreq;
        termFreq.reserve(tokens.size() / 2);
        
        for (const auto& token : tokens) {
            if (!token.empty()) {
                termFreq[token]++;
                allTerms.insert(token);
            }
        }
        
        for (const auto& [term, freq] : termFreq) {
            tempIndex[term][docId] = freq;
            vocabCount[term] += freq;
        }
    }
    
    indexer.setIndex(std::move(tempIndex));
    
    auto index_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - index_start).count();
    std::cout << "Inverted index built in " << index_time << "ms\n";

    std::cout << "Building search structures...\n";
    auto structures_start = std::chrono::steady_clock::now();
    
    for (const auto& term : allTerms) {
        autoComplete.insert(term);
        typoCorrector.insert(term);
    }
    
    auto structures_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - structures_start).count();
    std::cout << "Search structures built in " << structures_time << "ms\n";

    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    std::cout << "\n=== Index Build Complete ===\n";
    std::cout << "Documents indexed: " << docID << "\n";
    std::cout << "Total characters: " << totalChars << "\n";
    std::cout << "Vocabulary size: " << vocabCount.size() << " unique terms\n";
    if (!folders.empty()) {
        std::cout << "Folders found: " << folders.size() << "\n";
    }
    std::cout << "Time taken: " << totalTime << "ms\n";
    std::cout << "Average: " << (totalTime / static_cast<double>(docID)) << "ms per document\n";
    std::cout << "================================\n\n";
    
    saveIndex();
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
}

std::string DocumentManager::getDocumentContent(int docId) {
    // Check if already in cache
    auto it = docIdToContent.find(docId);
    if (it != docIdToContent.end()) {
        return it->second;
    }
    
    // Not in cache - load from disk
    auto pathIt = docIdToPath.find(docId);
    if (pathIt == docIdToPath.end()) {
        return "";
    }
    
    try {
        std::string content = Parser::readFile(pathIt->second);
        // Cache for future use (optional - can remove to save memory)
        // docIdToContent[docId] = content;
        return content;
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not load content for doc " << docId 
                  << ": " << e.what() << "\n";
        return "";
    }
}
void DocumentManager::updateExistingIndex() {
    std::cout << "Updating existing index...\n";
    if (docIdToPath.empty() && indexer.getIndex().empty()) {
        std::cout << "Index is empty - running fresh build instead\n";
        buildFreshIndex();
        return;
    }
    
    std::unordered_set<std::string> currentFiles;
    
    std::function<void(const fs::path&)> collectFiles = [&](const fs::path& dir) {
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    currentFiles.insert(entry.path().string());
                } else if (entry.is_directory()) {
                    collectFiles(entry.path());
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not access directory " << dir << ": " << e.what() << "\n";
        }
    };
    
    if (fs::exists("./data")) {
        collectFiles("./data");
    }
    std::unordered_map<std::string, int> pathToDocId;
    pathToDocId.reserve(docIdToPath.size());
    for (const auto& [id, path] : docIdToPath) {
        pathToDocId[path] = id;
    }


    int newFiles = 0, modifiedFiles = 0, deletedFiles = 0;

    for (const auto& file : currentFiles) {
    bool needsReindex = false;

    auto it = pathToDocId.find(file);
    int id = (it != pathToDocId.end()) ? it->second : -1;

    if (id == -1) {
        needsReindex = true;
        id = docID;
        newFiles++;
    }  else {
            try {
                auto ftime = fs::last_write_time(file).time_since_epoch().count();
                if (std::stoll(docMeta[id]) < ftime) {
                    needsReindex = true;
                    modifiedFiles++;
                }
            }  catch (const std::exception& e) {
            std::cout << "Warning: Could not check modification time for " << file << "\n";
            needsReindex = true;
        }
        }

        if (needsReindex) {
            try {
                std::string content = Parser::readFile(file);
                if (content.empty()) continue;
                
                auto tokens = Parser::tokenize(content);

                //  OPTIMIZED: Remove old tokens efficiently
                if (docTokens.count(id)) {
                    indexer.removeDocument(id, docTokens[id]);
                    for (const auto& w : docTokens[id]) {
                        auto it = vocabCount.find(w);
                        if (it != vocabCount.end()) {
                            if (--it->second <= 0) {
                                vocabCount.erase(it);
                                autoComplete.remove(w);
                                typoCorrector.markDeleted(w);
                            }
                        }
                    }
                }

                docTokens[id] = std::move(tokens);
                //docIdToContent[id] = std::move(content);
                indexer.indexDocumentFromTokens(id, docTokens[id]);

                docIdToPath[id] = file;
                docIdToRel[id] = fs::path(file).filename().string();
                docMeta[id] = std::to_string(fs::last_write_time(file).time_since_epoch().count());
                
                std::string folder = extractFolderFromPath(file);
                docIdToFolder[id] = folder;

                std::unordered_set<std::string> uniqueWords(docTokens[id].begin(), docTokens[id].end());
                for (const auto& w : uniqueWords) {
                    if (!w.empty()) {
                        autoComplete.insert(w);
                        typoCorrector.insert(w);
                        vocabCount[w]++;
                    }
                }
                if (id == docID) docID++;
            } catch (const std::exception& e) {
                std::cout << "Error updating file " << file << ": " << e.what() << "\n";
            }
        }
    }

    std::vector<int> toDelete;
    for (auto& [id, path] : docIdToPath) {
        if (!currentFiles.count(path)) {
            toDelete.push_back(id);
        }
    }

    for (int id : toDelete) {
        try {
            if (docTokens.count(id)) {
                indexer.removeDocument(id, docTokens[id]);
                for (auto& w : docTokens[id]) {
                    if (--vocabCount[w] <= 0) {
                        vocabCount.erase(w);
                        autoComplete.remove(w);
                        typoCorrector.markDeleted(w);
                    }
                }
                docTokens.erase(id);
            }
            docIdToPath.erase(id);
            docIdToRel.erase(id);
            docIdToContent.erase(id);
            docMeta.erase(id);
            docIdToFolder.erase(id);
            deletedFiles++;
        } catch (const std::exception& e) {
            std::cout << "Error deleting document " << id << ": " << e.what() << "\n";
        }
    }

    if (newFiles > 0 || modifiedFiles > 0 || deletedFiles > 0) {
        std::cout << "Index update complete: " << newFiles << " new, " 
                  << modifiedFiles << " modified, " << deletedFiles << " deleted files\n";
        saveIndex();
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    } else {
        std::cout << "No changes detected in document collection.\n";
    }
}

void DocumentManager::rebuildSearchStructures(bool clearContent) {
    std::cout << "Rebuilding autocomplete + corrections from saved tokens...\n";
    
    autoComplete.clear();
    typoCorrector.clear();
    
    if (clearContent) {
        docIdToContent.clear();
        std::cout << "Content cleared - will be lazy-loaded on demand\n";
    } else {
        std::cout << "Keeping content in memory (" << docIdToContent.size() << " documents)\n";
    }

    std::unordered_set<std::string> vocab;
    vocab.reserve(vocabCount.size()); //  Reserve based on known vocab
    
    for (auto& [id, tokens] : docTokens) {
        for (const auto& t : tokens) {
            if (!t.empty()) vocab.insert(t);
        }
    }

    for (const auto& w : vocab) {
        autoComplete.insert(w);
        typoCorrector.insert(w);
    }
    
    std::cout << "Rebuilt search structures: " << vocab.size() << " vocabulary terms\n";
}


int DocumentManager::uploadDocument(const std::string& filename, const std::string& content, const std::string& folder) {
    //   Better validation with specific error messages
    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }
    
    if (content.empty()) {
        throw std::invalid_argument("Content cannot be empty for file: " + filename);
    }

    std::string sanitizedFolder = folder;
    
    //   Better folder sanitization with logging
    if (!sanitizedFolder.empty()) {
        std::string originalFolder = sanitizedFolder;
        
        // Remove dangerous characters
        sanitizedFolder.erase(
            std::remove_if(sanitizedFolder.begin(), sanitizedFolder.end(), 
                [](unsigned char c) { 
                    return c == '/' || c == '\\' || c == '.' || c == ':' || 
                           c == '<' || c == '>' || c == '|' || c == '*' || c == '?' ||
                           c == '"' || c == '\0' || std::iscntrl(c);
                }), 
            sanitizedFolder.end()
        );

        if (sanitizedFolder.empty()) {
            throw std::invalid_argument("Invalid folder name after sanitization (was: '" + originalFolder + "')");
        }
        
        // Check reserved names
        std::vector<std::string> reserved = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", 
                                             "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", 
                                             "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
        
        std::string upperFolder = sanitizedFolder;
        std::transform(upperFolder.begin(), upperFolder.end(), upperFolder.begin(), ::toupper);
        
        for (const auto& res : reserved) {
            if (upperFolder == res) {
                throw std::invalid_argument("Folder name is a reserved system name: " + sanitizedFolder);
            }
        }
        
        if (sanitizedFolder.length() > 200) {
            sanitizedFolder = sanitizedFolder.substr(0, 200);
        }
    }
    
    //  Create folder path with error handling
    std::string folderPath = sanitizedFolder.empty() ? "./data/" : "./data/" + sanitizedFolder + "/";
    
    if (!sanitizedFolder.empty()) {
        try {
            if (!fs::exists(folderPath)) {
                fs::create_directories(folderPath);
                if (!silentMode) {
                    std::cout << "Created folder: " << sanitizedFolder << "\n";
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error("Failed to create folder '" + sanitizedFolder + "': " + e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create folder '" + sanitizedFolder + "': " + std::string(e.what()));
        }
    }
    
    //  Handle path construction with validation
    std::string path = folderPath + filename;
    
    // Validate the final path
    if (path.length() > 255) {
        throw std::invalid_argument("Full path too long (max 255 chars): " + path);
    }
    
    //  Better duplicate file handling
    if (fs::exists(path)) {
        // Instead of throwing, append a number
        std::string baseName = filename;
        std::string extension = "";
        
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = filename.substr(0, dotPos);
            extension = filename.substr(dotPos);
        }
        
        int counter = 1;
        std::string newFilename;
        do {
            newFilename = baseName + "_" + std::to_string(counter) + extension;
            path = folderPath + newFilename;
            counter++;
        } while (fs::exists(path) && counter < 1000);
        
        if (counter >= 1000) {
            throw std::runtime_error("Too many duplicate files: " + filename);
        }
        
        if (!silentMode) {
            std::cout << " File exists, renamed to: " << newFilename << "\n";
        }
    }
    
    //  Write file with better error handling
    try {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Could not create file: " + path);
        }
        
        file << content;
        file.close();
        
        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + path);
        }
        
        if (!fs::exists(path)) {
            throw std::runtime_error("File was not created successfully: " + path);
        }
        
        // Verify file size
        auto fileSize = fs::file_size(path);
        if (fileSize != content.size()) {
            throw std::runtime_error("File size mismatch: expected " + 
                                   std::to_string(content.size()) + " bytes, got " + 
                                   std::to_string(fileSize) + " bytes");
        }
        
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("Filesystem error writing file '" + path + "': " + e.what());
    } catch (const std::ios_base::failure& e) {
        throw std::runtime_error("I/O error writing file '" + path + "': " + e.what());
    }

    // Thread-safe ID assignment
    int newId;
    {
        std::lock_guard<std::mutex> lock(docIdMutex);
        newId = docID++;
    }
    
    // Store metadata
    docIdToPath[newId] = path;
    docIdToRel[newId] = fs::path(path).filename().string();  // Use actual filename (may be renamed)
    // Don't store content in memory - lazy load only
    docIdToFolder[newId] = sanitizedFolder;
    
    // Tokenize with error handling
    std::vector<std::string> tokens;
    try {
        tokens = Parser::tokenize(content);
        if (tokens.empty()) {
            throw std::runtime_error("Tokenization produced no tokens for file: " + filename);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Tokenization failed for '" + filename + "': " + e.what());
    }
    
    docTokens[newId] = std::move(tokens);
    indexer.indexDocumentFromTokens(newId, docTokens[newId]);

    // Update vocabularies
    for (const auto& w : docTokens[newId]) {
        if (!w.empty()) {
            autoComplete.insert(w);
            typoCorrector.insert(w);
            vocabCount[w]++;
        }
    }

    // Get file timestamp
    try {
        docMeta[newId] = std::to_string(fs::last_write_time(path).time_since_epoch().count());
    } catch (const std::exception& e) {
        if (!silentMode) {
            std::cout << "Warning: Could not get file timestamp for " << filename << ": " << e.what() << "\n";
        }
        docMeta[newId] = std::to_string(std::time(nullptr));
    }
    
    // Save index if not in batch mode
    if (!batchMode) {
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        saveIndex();
    }
    
    if (!silentMode) {
        std::cout << "Uploaded document: " << docIdToRel[newId] << " (ID: " << newId << ")" 
                  << (sanitizedFolder.empty() ? "" : " to folder: " + sanitizedFolder);
        
        if (batchMode) {
            std::cout << " [BATCH MODE]";
        }
        std::cout << "\n";
    }
    
    return newId;
}
void DocumentManager::finalizeBatch() {
    auto startTime = std::chrono::steady_clock::now();
    
    std::cout << "\n=== Finalizing Batch Upload ===\n";
    std::cout << "Rebuilding search engine...\n";
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    
    auto rebuildTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::cout << " Search engine rebuilt in " << rebuildTime << "ms\n";
    
    //  Save index asynchronously so HTTP response isn't blocked
    std::cout << "💾 Saving index in background...\n";
    
    // Launch async save (don't wait for it)
    std::thread([this]() {
        try {
            auto saveStart = std::chrono::steady_clock::now();
            ::saveIndex(
                indexer,
                docIdToPath,
                docIdToRel,
                docTokens,
                docMeta,
                vocabCount,
                docIdToFolder,
                docID
            );
            auto saveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - saveStart).count();
            std::cout << " Index saved successfully in " << saveTime << "ms (background)\n";
        } catch (const std::exception& e) {
            std::cerr << " Background index save failed: " << e.what() << "\n";
        }
    }).detach();  // Detach so it runs independently
    
    auto finalizeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    
    std::cout << " Batch finalized (index saving in background)\n";
    std::cout << " Finalized in " << finalizeTime << "ms\n";
}
//  OPTIMIZED: Use move semantics for tokens
bool DocumentManager::editDocument(int id, const std::string& newContent) {
    if (!docIdToPath.count(id)) return false;
    if (newContent.empty()) return false;

    try {
        std::ofstream file(docIdToPath[id], std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file << newContent;
        file.close();

        if (docTokens.count(id)) {
            indexer.removeDocument(id, docTokens[id]);
            for (const auto& w : docTokens[id]) {
                if (--vocabCount[w] <= 0) {
                    vocabCount.erase(w);
                    autoComplete.remove(w);
                    typoCorrector.markDeleted(w);
                }
            }
        }

        auto tokens = Parser::tokenize(newContent);
        docTokens[id] = std::move(tokens);
        docIdToContent[id] = newContent;
        indexer.indexDocumentFromTokens(id, docTokens[id]);

        for (const auto& w : docTokens[id]) {
            if (!w.empty()) {
                autoComplete.insert(w);
                typoCorrector.insert(w);
                vocabCount[w]++;
            }
        }

        docMeta[id] = std::to_string(fs::last_write_time(docIdToPath[id]).time_since_epoch().count());
        
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        saveIndex();
        
        std::cout << "Edited document ID: " << id << " (" << docIdToRel[id] << ")\n";
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error editing document: " << e.what() << "\n";
        return false;
    }
}

bool DocumentManager::deleteDocument(int id) {
    if (!docIdToPath.count(id)) return false;

    try {
        if (fs::exists(docIdToPath[id])) {
            if (!fs::remove(docIdToPath[id])) {
                std::cout << "Warning: Could not delete file from disk: " << docIdToPath[id] << "\n";
            }
        }

        if (docTokens.count(id)) {
            indexer.removeDocument(id, docTokens[id]);
            for (auto& w : docTokens[id]) {
                if (--vocabCount[w] <= 0) {
                    vocabCount.erase(w);
                    autoComplete.remove(w);
                    typoCorrector.markDeleted(w);
                }
            }
            docTokens.erase(id);
        }

        std::string filename = docIdToRel.count(id) ? docIdToRel[id] : "unknown";
        std::string folder = docIdToFolder.count(id) ? docIdToFolder[id] : "";

        docIdToPath.erase(id);
        docIdToRel.erase(id);
        docIdToContent.erase(id);
        docMeta.erase(id);
        docIdToFolder.erase(id);

        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        saveIndex();
        
        std::cout << "Deleted document: " << filename << " (ID: " << id << ")"
                  << (folder.empty() ? "" : " from folder: " + folder) << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error deleting document: " << e.what() << "\n";
        return false;
    }
}

std::vector<std::string> DocumentManager::getSuggestions(const std::string& prefix) {
    if (prefix.empty()) return std::vector<std::string>();
    try {
        return autoComplete.suggest(prefix);
    } catch (const std::exception& e) {
        std::cout << "Error getting suggestions: " << e.what() << "\n";
        return std::vector<std::string>();
    }
}

std::vector<std::string> DocumentManager::getCorrections(const std::string& word, int maxResults) {
    if (word.empty()) return std::vector<std::string>();
    try {
        auto corrections = typoCorrector.search(word, 2);
        if ((int)corrections.size() > maxResults) corrections.resize(maxResults);
        return corrections;
    } catch (const std::exception& e) {
        std::cout << "Error getting corrections: " << e.what() << "\n";
        return std::vector<std::string>();
    }
}

std::vector<std::pair<int, double>> DocumentManager::search(const std::string& query) const {
    if (query.empty() || !engine) return {};
    
    try {
        Ranker ranker;
        return engine->search(query, ranker);
    } catch (const std::exception& e) {
        std::cout << "Error during search: " << e.what() << "\n";
        return {};
    }
}
void DocumentManager::updateFromPersistence(
    const std::unordered_map<int, std::string>& loadedDocIdToPath,
    const std::unordered_map<int, std::string>& loadedDocIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& loadedDocTokens,
    const std::unordered_map<int, std::string>& loadedDocMeta,
    const std::unordered_map<std::string, int>& loadedVocabCount,
    const std::unordered_map<int, std::string>& loadedDocIdToFolder,
    int loadedDocID
) {
    docIdToPath = loadedDocIdToPath;
    docIdToRel = loadedDocIdToRel;
    docTokens = loadedDocTokens;
    docMeta = loadedDocMeta;
    vocabCount = loadedVocabCount;
    docIdToFolder = loadedDocIdToFolder;
    docID = loadedDocID;
    //  Note: docIdToContent is NOT loaded - lazy loading!
}

void DocumentManager::saveIndex() const {
    try {
        //  NEW: Uses binary persistence (with JSON fallback)
        ::saveIndex(
            indexer,
            docIdToPath,
            docIdToRel,
            docTokens,
            docMeta,
            vocabCount,
            docIdToFolder,
            docID
        );
        std::cout << " Index saved successfully.\n";
    } catch (const std::exception& e) {
        std::cout << " Error saving index: " << e.what() << "\n";
    }
}

std::string DocumentManager::getFolder(int docId) const {
    auto it = docIdToFolder.find(docId);
    return it != docIdToFolder.end() ? it->second : "";
}