#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "indexer.h"

class PersistenceManager {
public:
    static void saveIndexToFile(
        const Indexer& indexer,
        const std::unordered_map<int, std::string>& docIdToPath,
        const std::unordered_map<int, std::string>& docIdToRel,
        const std::unordered_map<int, std::vector<std::string>>& docTokens,
        const std::unordered_map<int, std::string>& docMeta,
        const std::unordered_map<std::string, int>& vocabCount,
        const std::unordered_map<int, std::string>& docIdToFolder, // NEW
        int docID
    );

    static bool loadIndexFromFile(
        Indexer& indexer,
        std::unordered_map<int, std::string>& docIdToPath,
        std::unordered_map<int, std::string>& docIdToRel,
        std::unordered_map<int, std::vector<std::string>>& docTokens,
        std::unordered_map<int, std::string>& docMeta,
        std::unordered_map<std::string, int>& vocabCount,
        std::unordered_map<int, std::string>& docIdToFolder, // NEW
        int& docID
    );
};
