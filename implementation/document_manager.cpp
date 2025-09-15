#include "document_manager.h"
#include "persistence.h"
#include "parser.h"
#include "ranker.h"
#include <filesystem>
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

DocumentManager::DocumentManager() : docID(0) {}

bool DocumentManager::initialize() {
    // Try to load existing index
    std::unordered_map<int, std::string> loadedDocIdToPath;
    std::unordered_map<int, std::string> loadedDocIdToRel;
    std::unordered_map<int, std::vector<std::string>> loadedDocTokens;
    std::unordered_map<int, std::string> loadedDocMeta;
    std::unordered_map<std::string, int> loadedVocabCount;
    int loadedDocID;

    if (!PersistenceManager::loadIndexFromFile(indexer, loadedDocIdToPath, loadedDocIdToRel, 
                                              loadedDocTokens, loadedDocMeta, loadedVocabCount, loadedDocID)) {
        std::cout << "No index.json found, indexing ./data …\n";
        buildFreshIndex();
        return true;
    }

    std::cout << "Loaded " << loadedDocIdToPath.size() << " docs from index.json\n";
    updateFromPersistence(loadedDocIdToPath, loadedDocIdToRel, loadedDocTokens, 
                         loadedDocMeta, loadedVocabCount, loadedDocID);
    updateExistingIndex();
    rebuildSearchStructures();
    
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    return false; // false = not fresh build
}

void DocumentManager::buildFreshIndex() {
    indexer.clear();
    autoComplete.clear();
    typoCorrector.clear();
    docTokens.clear();
    docIdToPath.clear();
    docIdToRel.clear();
    docIdToContent.clear();
    docID = 0;

    if (!fs::exists("./data")) fs::create_directory("./data");

    std::vector<fs::directory_entry> files;
    for (const auto& entry : fs::directory_iterator("./data")) {
        if (entry.is_regular_file()) files.push_back(entry);
    }

    std::cout << "Indexing " << files.size() << " files from ./data folder...\n";

    for (size_t i = 0; i < files.size(); ++i) {
        const auto& entry = files[i];
        std::string path = entry.path().string();
        std::string content = Parser::readFile(path);

        if (content.empty()) continue;

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

        ++docID;

        if ((i + 1) % 200 == 0 || i == files.size() - 1) {
            std::cout << "Progress: " << (i + 1) << "/" << files.size() << " files processed\n";
        }
    }

    std::cout << "Indexed " << docID << " files from ./data folder.\n";
    saveIndex();
    
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
}

void DocumentManager::updateExistingIndex() {
    std::unordered_set<std::string> currentFiles;
    for (auto& entry : fs::directory_iterator("./data")) {
        if (entry.is_regular_file()) currentFiles.insert(entry.path().string());
    }

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
        } else {
            auto ftime = fs::last_write_time(file).time_since_epoch().count();
            if (std::stoll(docMeta[id]) < ftime) needsReindex = true;
        }

        if (needsReindex) {
            std::string content = Parser::readFile(file);
            auto tokens = Parser::tokenize(content);

            if (docTokens.count(id))
                indexer.removeDocument(id, docTokens[id]);

            docTokens[id] = tokens;
            docIdToContent[id] = content;
            indexer.indexDocumentFromTokens(id, tokens);

            docIdToPath[id] = file;
            docIdToRel[id] = fs::path(file).filename().string();
            docMeta[id] = std::to_string(fs::last_write_time(file).time_since_epoch().count());

            for (auto& w : tokens)
                if (!w.empty()) vocabCount[w]++;
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
        indexer.removeDocument(id, docTokens[id]);
        for (auto& w : docTokens[id]) {
            if (--vocabCount[w] <= 0) {
                vocabCount.erase(w);
                autoComplete.remove(w);
                typoCorrector.markDeleted(w);
            }
        }
        docTokens.erase(id);
        docIdToPath.erase(id);
        docIdToRel.erase(id);
        docIdToContent.erase(id);
        docMeta.erase(id);
    }
}

void DocumentManager::rebuildSearchStructures() {
    std::cout << "Rebuilding autocomplete + corrections from saved tokens...\n";
    
    autoComplete.clear();
    typoCorrector.clear();
    docIdToContent.clear();

    // Reload content from files
    for (auto& [id, path] : docIdToPath) {
        docIdToContent[id] = Parser::readFile(path);
    }

    // Rebuild search structures
    std::unordered_set<std::string> vocab;
    for (auto& [id, tokens] : docTokens) {
        for (const auto& t : tokens) 
            if (!t.empty()) vocab.insert(t);
    }

    for (const auto& w : vocab) {
        autoComplete.insert(w);
        typoCorrector.insert(w);
    }
}

int DocumentManager::uploadDocument(const std::string& filename, const std::string& content) {
    std::string path = "./data/" + filename;
    std::ofstream(path) << content;

    int newId = docID++;
    docIdToPath[newId] = path;
    docIdToRel[newId] = filename;
    docIdToContent[newId] = content;

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

    docMeta[newId] = std::to_string(fs::last_write_time(path).time_since_epoch().count());
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    saveIndex();
    
    return newId;
}

bool DocumentManager::editDocument(int id, const std::string& newContent) {
    if (!docIdToPath.count(id)) return false;

    // Write to file
    std::ofstream(docIdToPath[id]) << newContent;

    // Remove old document
    if (docTokens.count(id)) {
        indexer.removeDocument(id, docTokens[id]);
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
    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    saveIndex();
    
    return true;
}

bool DocumentManager::deleteDocument(int id) {
    if (!docIdToPath.count(id)) return false;

    // Delete file from disk
    if (fs::exists(docIdToPath[id])) {
        if (!fs::remove(docIdToPath[id])) {
            return false;
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

    docIdToPath.erase(id);
    docIdToRel.erase(id);
    docIdToContent.erase(id);
    docMeta.erase(id);

    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
    saveIndex();
    
    return true;
}

std::vector<std::string> DocumentManager::getSuggestions(const std::string& prefix)  {
    return prefix.empty() ? std::vector<std::string>() : autoComplete.suggest(prefix);
}

std::vector<std::string> DocumentManager::getCorrections(const std::string& word, int maxResults)  {
    auto corrections = word.empty() ? std::vector<std::string>() : typoCorrector.search(word, 2);
    if ((int)corrections.size() > maxResults) corrections.resize(maxResults);
    return corrections;
}

std::vector<std::pair<int, double>> DocumentManager::search(const std::string& query) const {
    if (query.empty() || !engine) return {};
    
    Ranker ranker;
    return engine->search(query, ranker);
}

void DocumentManager::updateFromPersistence(
    const std::unordered_map<int, std::string>& loadedDocIdToPath,
    const std::unordered_map<int, std::string>& loadedDocIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& loadedDocTokens,
    const std::unordered_map<int, std::string>& loadedDocMeta,
    const std::unordered_map<std::string, int>& loadedVocabCount,
    int loadedDocID
) {
    docIdToPath = loadedDocIdToPath;
    docIdToRel = loadedDocIdToRel;
    docTokens = loadedDocTokens;
    docMeta = loadedDocMeta;
    vocabCount = loadedVocabCount;
    docID = loadedDocID;
}

void DocumentManager::saveIndex() const {
    PersistenceManager::saveIndexToFile(indexer, docIdToPath, docIdToRel, docTokens, docMeta, vocabCount, docID);
}