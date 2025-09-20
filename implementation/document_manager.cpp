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
        rebuildSearchStructures();
        
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
    fs::path dataDir = fs::path("./data");
    
    try {
        // Get relative path from data directory
        fs::path relativePath = fs::relative(p, dataDir);
        
        // If file is in a subdirectory, use that as folder name
        if (relativePath.parent_path() != ".") {
            return relativePath.parent_path().string();
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not extract folder from path " << filepath << ": " << e.what() << "\n";
    }
    
    return ""; // No folder (root level)
}

void DocumentManager::buildFreshIndex() {
    std::cout << "Building fresh index...\n";
    
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

    for (size_t i = 0; i < files.size(); ++i) {
        const auto& entry = files[i];
        std::string path = entry.path().string();
        
        try {
            std::string content = Parser::readFile(path);

            if (content.empty()) {
                std::cout << "Warning: Empty or unreadable file: " << path << "\n";
                continue;
            }

            auto tokens = Parser::tokenize(content);
            docTokens[docID] = std::move(tokens);
            docIdToContent[docID] = std::move(content);

            indexer.indexDocumentFromTokens(docID, docTokens[docID]);

            for (const auto& w : docTokens[docID]) {
                if (!w.empty()) {
                    autoComplete.insert(w);
                    typoCorrector.insert(w);
                    vocabCount[w]++;
                }
            }

            docIdToPath[docID] = path;
            docIdToRel[docID] = entry.path().filename().string();
            docMeta[docID] = std::to_string(fs::last_write_time(path).time_since_epoch().count());
            
            // Extract folder from path
            std::string folder = extractFolderFromPath(path);
            docIdToFolder[docID] = folder;
            if (!folder.empty()) folders.insert(folder);

            totalChars += docIdToContent[docID].size();
            ++docID;

            // Progress reporting
            if ((i + 1) % 50 == 0 || i == files.size() - 1) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                std::cout << "Progress: " << (i + 1) << "/" << files.size() 
                          << " files processed (" << std::fixed << std::setprecision(1) 
                          << (100.0 * (i + 1) / files.size()) << "%) - "
                          << elapsed << "ms elapsed\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error processing file " << path << ": " << e.what() << "\n";
        }
    }

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
    std::cout << "================================\n\n";
    
    saveIndex();
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
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

    // Handle new and modified files
    for (const auto& file : currentFiles) {
        bool needsReindex = false;
        int id = -1;

        // Find existing document
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
                needsReindex = true; // Re-index if we can't determine modification time
            }
        }

        if (needsReindex) {
            try {
                std::string content = Parser::readFile(file);
                if (content.empty()) continue;
                
                auto tokens = Parser::tokenize(content);

                if (docTokens.count(id)) {
                    indexer.removeDocument(id, docTokens[id]);
                    // Update vocabulary counts
                    for (const auto& w : docTokens[id]) {
                        if (--vocabCount[w] <= 0) {
                            vocabCount.erase(w);
                            autoComplete.remove(w);
                            typoCorrector.markDeleted(w);
                        }
                    }
                }

                docTokens[id] = tokens;
                docIdToContent[id] = content;
                indexer.indexDocumentFromTokens(id, tokens);

                docIdToPath[id] = file;
                docIdToRel[id] = fs::path(file).filename().string();
                docMeta[id] = std::to_string(fs::last_write_time(file).time_since_epoch().count());
                
                // Extract and set folder
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

    // Handle deleted files
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

void DocumentManager::rebuildSearchStructures() {
    std::cout << "Rebuilding autocomplete + corrections from saved tokens...\n";
    
    autoComplete.clear();
    typoCorrector.clear();
    docIdToContent.clear();

    // Reload content from files
    int successful = 0;
    for (auto& [id, path] : docIdToPath) {
        try {
            docIdToContent[id] = Parser::readFile(path);
            if (!docIdToContent[id].empty()) successful++;
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not reload content from " << path << ": " << e.what() << "\n";
        }
    }

    // Rebuild search structures
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
    
    std::cout << "Rebuilt search structures: " << successful << "/" << docIdToPath.size() 
              << " files reloaded, " << vocab.size() << " vocabulary terms\n";
}

int DocumentManager::uploadDocument(const std::string& filename, const std::string& content, const std::string& folder) {
    if (filename.empty() || content.empty()) {
        throw std::invalid_argument("Filename and content cannot be empty");
    }

    std::string sanitizedFolder = folder;
    // Basic sanitization
    if (!sanitizedFolder.empty()) {
        // Remove dangerous characters
        sanitizedFolder.erase(std::remove_if(sanitizedFolder.begin(), sanitizedFolder.end(), 
            [](char c) { return c == '/' || c == '\\' || c == '.' || c == ':'; }), sanitizedFolder.end());
        
        if (sanitizedFolder.empty()) {
            throw std::invalid_argument("Invalid folder name after sanitization");
        }
    }
    
    std::string folderPath = sanitizedFolder.empty() ? "./data/" : "./data/" + sanitizedFolder + "/";
    
    // Create folder directory if it doesn't exist
    if (!sanitizedFolder.empty() && !fs::exists(folderPath)) {
        try {
            fs::create_directories(folderPath);
            std::cout << "Created folder: " << sanitizedFolder << "\n";
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create folder: " + std::string(e.what()));
        }
    }
    
    std::string path = folderPath + filename;
    
    // Check if file already exists
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
        // Write to file
        std::ofstream file(docIdToPath[id], std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file << newContent;
        file.close();

        // Remove old document from index
        if (docTokens.count(id)) {
            indexer.removeDocument(id, docTokens[id]);
            // Update vocabulary counts
            for (const auto& w : docTokens[id]) {
                if (--vocabCount[w] <= 0) {
                    vocabCount.erase(w);
                    autoComplete.remove(w);
                    typoCorrector.markDeleted(w);
                }
            }
        }

        // Add new document
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
        // Note: folder is preserved (not changed during edit)
        
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
        // Delete file from disk
        if (fs::exists(docIdToPath[id])) {
            if (!fs::remove(docIdToPath[id])) {
                std::cout << "Warning: Could not delete file from disk: " << docIdToPath[id] << "\n";
            }
        }

        // Remove from index
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