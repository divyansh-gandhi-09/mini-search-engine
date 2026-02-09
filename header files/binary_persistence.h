#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "indexer.h"

// Binary persistence class - for fast index loading/saving
class BinaryPersistence {
public:
    // Save index in fast binary format 
    static void saveBinary(
        const Indexer& indexer,
        const std::unordered_map<int, std::string>& docIdToPath,
        const std::unordered_map<int, std::string>& docIdToRel,
        const std::unordered_map<int, std::vector<std::string>>& docTokens,
        const std::unordered_map<int, std::string>& docMeta,
        const std::unordered_map<std::string, int>& vocabCount,
        const std::unordered_map<int, std::string>& docIdToFolder,
        int docID
    );

    // Load binary index 
    static bool loadBinary(
        Indexer& indexer,
        std::unordered_map<int, std::string>& docIdToPath,
        std::unordered_map<int, std::string>& docIdToRel,
        std::unordered_map<int, std::vector<std::string>>& docTokens,
        std::unordered_map<int, std::string>& docMeta,
        std::unordered_map<std::string, int>& vocabCount,
        std::unordered_map<int, std::string>& docIdToFolder,
        int& docID
    );

private:
    // Helper methods for binary serialization
    static void writeString(std::ofstream& out, const std::string& str);
    static std::string readString(std::ifstream& in);
    static void writeInt(std::ofstream& out, int value);
    static int readInt(std::ifstream& in);
    static void writeStringVector(std::ofstream& out, const std::vector<std::string>& vec);
    static std::vector<std::string> readStringVector(std::ifstream& in);
};

//  GLOBAL FUNCTIONS - Unified save/load interface
// These try binary first, fall back to JSON
void saveIndex(
    const Indexer& indexer,
    const std::unordered_map<int, std::string>& docIdToPath,
    const std::unordered_map<int, std::string>& docIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& docTokens,
    const std::unordered_map<int, std::string>& docMeta,
    const std::unordered_map<std::string, int>& vocabCount,
    const std::unordered_map<int, std::string>& docIdToFolder,
    int docID
);

bool loadIndex(
    Indexer& indexer,
    std::unordered_map<int, std::string>& docIdToPath,
    std::unordered_map<int, std::string>& docIdToRel,
    std::unordered_map<int, std::vector<std::string>>& docTokens,
    std::unordered_map<int, std::string>& docMeta,
    std::unordered_map<std::string, int>& vocabCount,
    std::unordered_map<int, std::string>& docIdToFolder,
    int& docID
);