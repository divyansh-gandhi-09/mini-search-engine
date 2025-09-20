#include "persistence.h"
#include <filesystem>
#include <fstream>
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
    json j;
    j["docID"] = docID;
    j["docIdToPath"] = docIdToPath;
    j["docIdToRel"] = docIdToRel;
    j["index"] = indexer.getIndex();
    j["docTokens"] = docTokens;
    j["docMeta"] = docMeta;
    j["vocabCount"] = vocabCount;
    j["docIdToFolder"] = docIdToFolder;

    std::ofstream out("index.json.tmp", std::ios::trunc | std::ios::binary);
    std::string jsonStr = j.dump();
    out.write(jsonStr.c_str(), jsonStr.size());
    out.close();
    fs::rename("index.json.tmp", "index.json");
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
    if (!fs::exists("index.json")) return false;
    std::ifstream in("index.json");
    if (!in.is_open()) return false;

    json j; in >> j;
    try {
        docID = j["docID"].get<int>();
        docIdToPath = j["docIdToPath"].get<std::unordered_map<int, std::string>>();
        docIdToRel = j["docIdToRel"].get<std::unordered_map<int, std::string>>();
        docTokens = j["docTokens"].get<std::unordered_map<int, std::vector<std::string>>>();
        docMeta = j["docMeta"].get<std::unordered_map<int, std::string>>();
        vocabCount = j["vocabCount"].get<std::unordered_map<std::string, int>>();
        if (j.contains("docIdToFolder"))
            docIdToFolder = j["docIdToFolder"].get<std::unordered_map<int, std::string>>();
        else
            docIdToFolder.clear();
        indexer.setIndex(
            j["index"].get<std::unordered_map<std::string, std::unordered_map<int, int>>>()
        );
    } catch (...) { return false; }
    return true;
}
