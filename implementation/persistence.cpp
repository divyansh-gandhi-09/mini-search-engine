#include "persistence.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "../third_party/json.hpp"
#include <iomanip>
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
        
        json j;
        j["docID"] = docID;
        j["docIdToPath"] = docIdToPath;
        j["docIdToRel"] = docIdToRel;
        j["index"] = indexer.getIndex();
        
        // ✅ SAVE TOKENS - Don't rebuild them on load!
        j["docTokens"] = docTokens;
        
        j["docMeta"] = docMeta;
        j["vocabCount"] = vocabCount;
        j["docIdToFolder"] = docIdToFolder;

        // Compact JSON
        std::string jsonStr;
        try {
            jsonStr = j.dump();
        } catch (const json::exception& e) {
            throw std::runtime_error(std::string("JSON serialization failed: ") + e.what());
        }

        std::ofstream out("index.json.tmp", std::ios::trunc | std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot create index.json.tmp");
        }

        out << jsonStr;
        out.flush();

        if (!out.good()) {
            out.close();
            fs::remove("index.json.tmp");
            throw std::runtime_error("Failed to write index.json.tmp");
        }
        
        out.close();

        if (!fs::exists("index.json.tmp")) {
            throw std::runtime_error("Temporary file was not created");
        }

        auto tmpSize = fs::file_size("index.json.tmp");
        if (tmpSize == 0) {
            fs::remove("index.json.tmp");
            throw std::runtime_error("Temporary file is empty");
        }

        std::error_code ec;
        fs::rename("index.json.tmp", "index.json", ec);
        if (ec) {
            throw std::runtime_error("Failed to rename index file: " + ec.message());
        }

        std::cout << "Index saved successfully (" << tmpSize << " bytes)\n";
        std::cout << "Index file size: " << std::fixed << std::setprecision(2) 
          << (tmpSize / 1024.0 / 1024.0) << " MB\n";
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR saving index: " << e.what() << "\n";
        if (fs::exists("index.json.tmp")) {
            fs::remove("index.json.tmp");
        }
        throw;
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

    auto startTime = std::chrono::steady_clock::now();
    std::ifstream in("index.json", std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "ERROR: Could not open index.json\n";
        return false;
    }

    try {
        json j;
        in >> j;
        in.close();

        if (!j.contains("docID") || !j.contains("index")) {
            std::cerr << "ERROR: Corrupted index.json - missing required fields\n";
            return false;
        }

        // ✅ OPTIMIZED: Load directly without expensive rebuilds
        docID = j["docID"].get<int>();
        
        // Pre-reserve hash maps for better performance
        size_t expectedDocs = static_cast<size_t>(docID);
        docIdToPath.reserve(expectedDocs);
        docIdToRel.reserve(expectedDocs);
        docTokens.reserve(expectedDocs);
        docMeta.reserve(expectedDocs);
        docIdToFolder.reserve(expectedDocs);
        
        if (j.contains("docIdToPath"))
            docIdToPath = j["docIdToPath"].get<std::unordered_map<int, std::string>>();
        
        if (j.contains("docIdToRel"))
            docIdToRel = j["docIdToRel"].get<std::unordered_map<int, std::string>>();
        
        // ✅ CRITICAL FIX: Just load tokens directly - NO REBUILD!
        if (j.contains("docTokens")) {
            docTokens = j["docTokens"].get<std::unordered_map<int, std::vector<std::string>>>();
            std::cout << "Loaded tokens for " << docTokens.size() << " documents\n";
        } else {
            // ✅ OPTIMIZED FALLBACK: If old format, rebuild efficiently
            std::cout << "Rebuilding tokens from inverted index (one-time migration)...\n";
            auto loadedIndex = j["index"].get<std::unordered_map<std::string, std::unordered_map<int, int>>>();
            
            // First pass: count total tokens per doc for proper reserve
            std::unordered_map<int, size_t> docTokenCounts;
            for (const auto& [term, postings] : loadedIndex) {
                for (const auto& [docId, freq] : postings) {
                    docTokenCounts[docId] += freq;
                }
            }
            
            // Pre-allocate all vectors
            for (const auto& [docId, count] : docTokenCounts) {
                docTokens[docId].reserve(count);
            }
            
            // Second pass: fill vectors (now no reallocation!)
            for (const auto& [term, postings] : loadedIndex) {
                for (const auto& [docId, freq] : postings) {
                    auto& tokens = docTokens[docId];
                    for (int i = 0; i < freq; ++i) {
                        tokens.push_back(term);
                    }
                }
            }
            std::cout << "Tokens rebuilt for " << docTokens.size() << " documents\n";
            std::cout << "⚠️  Recommend re-saving index to avoid rebuild next time\n";
        }
        
        if (j.contains("docMeta"))
            docMeta = j["docMeta"].get<std::unordered_map<int, std::string>>();
        
        if (j.contains("vocabCount"))
            vocabCount = j["vocabCount"].get<std::unordered_map<std::string, int>>();
        
        if (j.contains("docIdToFolder")) {
            docIdToFolder = j["docIdToFolder"].get<std::unordered_map<int, std::string>>();
        } else {
            docIdToFolder.clear();
            std::cout << "Note: Old index format (no folder information)\n";
        }

        // Load index
        auto loadedIndex = j["index"].get<std::unordered_map<std::string, std::unordered_map<int, int>>>();
        indexer.setIndex(std::move(loadedIndex));  // ✅ Use move semantics

        auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        std::cout << "Successfully loaded index with " << docID << " documents in " 
                  << loadTime << "ms (" << std::fixed << std::setprecision(2) 
                  << (loadTime / static_cast<double>(docID)) << "ms per doc)\n";
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