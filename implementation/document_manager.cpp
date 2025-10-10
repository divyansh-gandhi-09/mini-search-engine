#include "document_manager.h"
#include "persistence.h"
#include "parser.h"
#include "ranker.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
namespace fs = std::filesystem;

DocumentManager::DocumentManager() : docID(0) {}

bool DocumentManager::initialize() {
    std::unordered_map<int, std::string> loadedDocIdToPath;
    std::unordered_map<int, std::string> loadedDocIdToRel;
    std::unordered_map<int, std::vector<std::string>> loadedDocTokens;
    std::unordered_map<int, std::string> loadedDocMeta;
    std::unordered_map<std::string, int> loadedVocabCount;
    std::unordered_map<int, std::string> loadedDocIdToFolder;
    int loadedDocID;

    if (PersistenceManager::loadIndexFromFile(indexer, loadedDocIdToPath, loadedDocIdToRel,
                                              loadedDocTokens, loadedDocMeta,
                                              loadedVocabCount, loadedDocIdToFolder, loadedDocID)) {
        updateFromPersistence(loadedDocIdToPath, loadedDocIdToRel,
                              loadedDocTokens, loadedDocMeta,
                              loadedVocabCount, loadedDocIdToFolder, loadedDocID);
        
        // ✅ Clear content for lazy loading since we're loading from disk
        rebuildSearchStructures(true);
        
        // ✅ FIX: Create search engine after loading
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        
        // Show loaded index stats
        std::cout << "Loaded index with " << docID << " documents\n";
        std::unordered_set<std::string> folders;
        for (const auto& [id, folder] : docIdToFolder) {
            if (!folder.empty()) folders.insert(folder);
        }
        if (!folders.empty()) {
            std::cout << "Found " << folders.size() << " folders: ";
            bool first = true;
            for (const auto& f : folders) {
                if (!first) std::cout << ", ";
                std::cout << f;
                first = false;
            }
            std::cout << "\n";
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
    std::cout << "Building fresh index...\n";
    
    // Clear all data structures
    indexer.clear();
    autoComplete.clear();
    typoCorrector.clear();
    docTokens.clear();
    docIdToPath.clear();
    docIdToRel.clear();
    docIdToContent.clear();
    docIdToFolder.clear();
    vocabCount.clear();
    docID = 0;

    if (!fs::exists("./data")) {
        fs::create_directories("./data");
        std::cout << "Created ./data directory\n";
    }

    // Collect all files first
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

    docIdToPath.reserve(files.size());
    docIdToRel.reserve(files.size());
    docIdToContent.reserve(files.size());
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

                // ✅ KEEP content in memory during fresh build
                docTokens[docID] = tokens;
                docIdToContent[docID] = content;
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

                totalChars += docIdToContent[docID].size();
                ++docID;

            } catch (const std::exception& e) {
                std::cout << "Error processing file " << path << ": " << e.what() << "\n";
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        std::cout << "Progress: " << batch_end << "/" << files.size() 
                  << " files processed (" << std::fixed << std::setprecision(1) 
                  << (100.0 * batch_end / files.size()) << "%) - "
                  << elapsed << "ms elapsed\n";
    }

    // Build indexes from collected data
    std::cout << "Building inverted index...\n";
    auto index_start = std::chrono::steady_clock::now();
    
    std::unordered_map<std::string, std::unordered_map<int,int>> tempIndex;
    std::unordered_set<std::string> allTerms;
    
    for (const auto& [docId, tokens] : docTokens) {
        std::unordered_map<std::string, int> termFreq;
        
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

    // Build autocomplete and typo corrector
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
        std::cout << "Folders found: " << folders.size() << " (";
        bool first = true;
        for (const auto& f : folders) {
            if (!first) std::cout << ", ";
            std::cout << f;
            first = false;
        }
        std::cout << ")\n";
    }
    std::cout << "Time taken: " << totalTime << "ms\n";
    std::cout << "Average: " << (totalTime / static_cast<double>(docID)) << "ms per document\n";
    std::cout << "Content in memory: " << docIdToContent.size() << " documents\n";
    std::cout << "================================\n\n";
    
    // ✅ Content is already in memory from the build process - don't clear it!
    // Search structures (autoComplete, typoCorrector) were already built above
    // Just save index and create search engine
    saveIndex();
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    
    std::cout << "Performing index validation search...\n";
    if (docID > 0) {
        try {
            Ranker ranker;
            auto testResults = engine->search("test", ranker);
            std::cout << "Index validation complete. Ready for queries.\n";
        } catch (const std::exception& e) {
            std::cout << "Warning: Index validation failed: " << e.what() << "\n";
        }
    }
}

std::string DocumentManager::getDocumentContent(int docId) {
    // Check if content is already in memory
    auto it = docIdToContent.find(docId);
    if (it != docIdToContent.end()) {
        return it->second;
    }
    
    // Lazy load from disk if not in memory
    auto pathIt = docIdToPath.find(docId);
    if (pathIt == docIdToPath.end()) {
        return "";
    }
    
    try {
        std::string content = Parser::readFile(pathIt->second);
        // Cache it for future use
        docIdToContent[docId] = content;
        return content;
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not load content for doc " << docId 
                  << ": " << e.what() << "\n";
        return "";
    }
}

void DocumentManager::updateExistingIndex() {
    std::cout << "Updating existing index...\n";
    
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

    int newFiles = 0, modifiedFiles = 0, deletedFiles = 0;

    for (const auto& file : currentFiles) {
        bool needsReindex = false;
        int id = -1;

        for (auto& [docId, path] : docIdToPath) {
            if (path == file) {
                id = docId;
                break;
            }
        }

        if (id == -1) {
            needsReindex = true;
            id = docID++;
            newFiles++;
        } else {
            try {
                auto ftime = fs::last_write_time(file).time_since_epoch().count();
                if (std::stoll(docMeta[id]) < ftime) {
                    needsReindex = true;
                    modifiedFiles++;
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not check modification time for " << file << "\n";
                needsReindex = true;
            }
        }

        if (needsReindex) {
            try {
                std::string content = Parser::readFile(file);
                if (content.empty()) continue;
                
                auto tokens = Parser::tokenize(content);

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

                docTokens[id] = tokens;
                docIdToContent[id] = content;
                indexer.indexDocumentFromTokens(id, tokens);

                docIdToPath[id] = file;
                docIdToRel[id] = fs::path(file).filename().string();
                docMeta[id] = std::to_string(fs::last_write_time(file).time_since_epoch().count());
                
                std::string folder = extractFolderFromPath(file);
                docIdToFolder[id] = folder;

                for (auto& w : tokens) {
                    if (!w.empty()) {
                        autoComplete.insert(w);
                        typoCorrector.insert(w);
                        vocabCount[w]++;
                    }
                }
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
    
    // ✅ FIX: Only clear content when explicitly requested (e.g., loading from disk)
    if (clearContent) {
        docIdToContent.clear();
        std::cout << "Content cleared - will be lazy-loaded on demand\n";
    } else {
        std::cout << "Keeping content in memory (" << docIdToContent.size() << " documents)\n";
    }

    // Rebuild search structures from tokens
    std::unordered_set<std::string> vocab;
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
    if (filename.empty() || content.empty()) {
        throw std::invalid_argument("Filename and content cannot be empty");
    }

    std::string sanitizedFolder = folder;
    
    if (!sanitizedFolder.empty()) {
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
            throw std::invalid_argument("Invalid folder name after sanitization");
        }
        
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
    
    std::string folderPath = sanitizedFolder.empty() ? "./data/" : "./data/" + sanitizedFolder + "/";
    
    if (!sanitizedFolder.empty()) {
        try {
            if (!fs::exists(folderPath)) {
                fs::create_directories(folderPath);
                std::cout << "Created folder: " << sanitizedFolder << "\n";
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create folder: " + std::string(e.what()));
        }
    }
    
    std::string path = folderPath + filename;
    
    if (fs::exists(path)) {
        throw std::runtime_error("File already exists: " + filename);
    }
    
    try {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Could not create file: " + path);
        }
        file << content;
        file.close();
        
        if (!fs::exists(path)) {
            throw std::runtime_error("File was not created successfully");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to write file: " + std::string(e.what()));
    }

    int newId = docID++;
    docIdToPath[newId] = path;
    docIdToRel[newId] = filename;
    docIdToContent[newId] = content;
    docIdToFolder[newId] = sanitizedFolder;
    
    auto tokens = Parser::tokenize(content);
    docTokens[newId] = tokens;
    indexer.indexDocumentFromTokens(newId, tokens);

    for (const auto& w : tokens) {
        if (!w.empty()) {
            autoComplete.insert(w);
            typoCorrector.insert(w);
            vocabCount[w]++;
        }
    }

    try {
        docMeta[newId] = std::to_string(fs::last_write_time(path).time_since_epoch().count());
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not get file timestamp: " << e.what() << "\n";
        docMeta[newId] = std::to_string(std::time(nullptr));
    }
    
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    saveIndex();
    
    std::cout << "Uploaded document: " << filename << " (ID: " << newId << ")" 
              << (sanitizedFolder.empty() ? "" : " to folder: " + sanitizedFolder) << "\n";
    
    return newId;
}

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
}

void DocumentManager::saveIndex() const {
    try {
        PersistenceManager::saveIndexToFile(
            indexer,
            docIdToPath,
            docIdToRel,
            docTokens,
            docMeta,
            vocabCount,
            docIdToFolder,
            docID
        );
        std::cout << "Index saved successfully.\n";
    } catch (const std::exception& e) {
        std::cout << "Error saving index: " << e.what() << "\n";
    }
}

std::string DocumentManager::getFolder(int docId) const {
    auto it = docIdToFolder.find(docId);
    return it != docIdToFolder.end() ? it->second : "";
}