#include "persistence.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "../third_party/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

void PersistenceManager::saveIndexToFile(
    const Indexer& indexer,
    const std::unordered_map<int, std::string>& docIdToPath,
    const std::unordered_map<int, std::string>& docIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& docTokens,
    const std::unordered_map<int, std::string>& docMeta,
    const std::unordered_map<std::string, int>& vocabCount,
    const std::unordered_map<int, std::string>& docIdToFolder,
    int docID
) {
    try {
        std::cout << "Saving index to file...\n";
        
        // Build JSON object with error checking
        json j;
        j["docID"] = docID;
        j["docIdToPath"] = docIdToPath;
        j["docIdToRel"] = docIdToRel;
        j["index"] = indexer.getIndex();
        j["docTokens"] = docTokens;
        j["docMeta"] = docMeta;
        j["vocabCount"] = vocabCount;
        j["docIdToFolder"] = docIdToFolder;

        // Serialize to string first (can throw)
        std::string jsonStr;
        try {
            jsonStr = j.dump(2); // Pretty print with indent
        } catch (const json::exception& e) {
            throw std::runtime_error(std::string("JSON serialization failed: ") + e.what());
        }

        // Write to temporary file
        std::ofstream out("index.json.tmp", std::ios::trunc | std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot create index.json.tmp");
        }

        out << jsonStr;
        out.flush();

        // Check for write errors
        if (!out.good()) {
            out.close();
            fs::remove("index.json.tmp");
            throw std::runtime_error("Failed to write index.json.tmp - disk full or permission denied");
        }
        
        out.close();

        // Verify file was written correctly
        if (!fs::exists("index.json.tmp")) {
            throw std::runtime_error("Temporary file was not created");
        }

        auto tmpSize = fs::file_size("index.json.tmp");
        if (tmpSize == 0) {
            fs::remove("index.json.tmp");
            throw std::runtime_error("Temporary file is empty");
        }

        // Atomic rename (overwrites existing index.json)
        std::error_code ec;
        fs::rename("index.json.tmp", "index.json", ec);
        if (ec) {
            throw std::runtime_error("Failed to rename index file: " + ec.message());
        }

        std::cout << "Index saved successfully (" << tmpSize << " bytes)\n";
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR saving index: " << e.what() << "\n";
        // Clean up temporary file if it exists
        if (fs::exists("index.json.tmp")) {
            fs::remove("index.json.tmp");
        }
        throw; // Re-throw to caller
    }
}

bool PersistenceManager::loadIndexFromFile(
    Indexer& indexer,
    std::unordered_map<int, std::string>& docIdToPath,
    std::unordered_map<int, std::string>& docIdToRel,
    std::unordered_map<int, std::vector<std::string>>& docTokens,
    std::unordered_map<int, std::string>& docMeta,
    std::unordered_map<std::string, int>& vocabCount,
    std::unordered_map<int, std::string>& docIdToFolder,
    int& docID
) {
    if (!fs::exists("index.json")) {
        std::cout << "No index.json found\n";
        return false;
    }

    std::ifstream in("index.json", std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "ERROR: Could not open index.json\n";
        return false;
    }

    try {
        json j;
        in >> j;
        in.close();

        // Validate required fields exist
        if (!j.contains("docID") || !j.contains("index")) {
            std::cerr << "ERROR: Corrupted index.json - missing required fields\n";
            return false;
        }

        // Load all fields with error checking
        docID = j["docID"].get<int>();
        
        if (j.contains("docIdToPath"))
            docIdToPath = j["docIdToPath"].get<std::unordered_map<int, std::string>>();
        
        if (j.contains("docIdToRel"))
            docIdToRel = j["docIdToRel"].get<std::unordered_map<int, std::string>>();
        
        if (j.contains("docTokens"))
            docTokens = j["docTokens"].get<std::unordered_map<int, std::vector<std::string>>>();
        
        if (j.contains("docMeta"))
            docMeta = j["docMeta"].get<std::unordered_map<int, std::string>>();
        
        if (j.contains("vocabCount"))
            vocabCount = j["vocabCount"].get<std::unordered_map<std::string, int>>();
        
        // Handle backward compatibility for docIdToFolder
        if (j.contains("docIdToFolder")) {
            docIdToFolder = j["docIdToFolder"].get<std::unordered_map<int, std::string>>();
        } else {
            docIdToFolder.clear();
            std::cout << "Note: Old index format detected (no folder information)\n";
        }

        // Load index
        auto loadedIndex = j["index"].get<std::unordered_map<std::string, std::unordered_map<int, int>>>();
        indexer.setIndex(loadedIndex);

        std::cout << "Successfully loaded index with " << docID << " documents\n";
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "ERROR: JSON parsing failed: " << e.what() << "\n";
        in.close();
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load index: " << e.what() << "\n";
        in.close();
        return false;
    }
}