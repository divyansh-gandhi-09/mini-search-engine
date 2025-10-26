#include "stats_handlers.h"
#include "../third_party/json.hpp"
#include <filesystem>
#include <unordered_map>

using json = nlohmann::json;
namespace fs = std::filesystem;

void StatsHandlers::handleHealth(const httplib::Request&, httplib::Response& res) {
    res.set_content(R"({"status":"ok"})", "application/json");
}

void StatsHandlers::handleStats(DocumentManager& docManager, const httplib::Request&, httplib::Response& res) {
    json stats;
    
    stats["total_documents"] = docManager.getDocID();
    stats["indexed_documents"] = docManager.getDocIdToPath().size();
    stats["vocabulary_size"] = docManager.getVocabCount().size();
    
    std::unordered_map<std::string, int> folderCounts;
    int rootDocuments = 0;
    
    for (const auto& [id, folder] : docManager.getDocIdToFolder()) {
        if (folder.empty()) rootDocuments++;
        else folderCounts[folder]++;
    }
    
    stats["folders"] = json::array();
    for (const auto& [folder, count] : folderCounts) {
        stats["folders"].push_back({{"name", folder}, {"document_count", count}});
    }
    stats["root_documents"] = rootDocuments;
    
    std::unordered_map<std::string, int> extensions;
    for (const auto& [id, path] : docManager.getDocIdToPath()) {
        fs::path p(path);
        std::string ext = p.extension().string();
        if (ext.empty()) ext = "no_extension";
        extensions[ext]++;
    }
    
    stats["file_types"] = json::array();
    for (const auto& [ext, count] : extensions) {
        stats["file_types"].push_back({{"extension", ext}, {"count", count}});
    }
    
    size_t totalSize = 0;
    for (const auto& [id, path] : docManager.getDocIdToPath()) {
        try {
            if (fs::exists(path)) totalSize += fs::file_size(path);
        } catch (...) {}
    }
    stats["total_content_size"] = totalSize;
    stats["content_in_memory"] = docManager.getDocIdToContent().size();
    
    res.set_content(stats.dump(), "application/json");
}