#include "query_handlers.h"
#include "../third_party/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
    std::string getQuickPreview(const std::string& filepath, size_t maxChars = 500) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) return "";
        
        std::vector<char> buffer(maxChars);
        file.read(buffer.data(), maxChars);
        std::streamsize bytesRead = file.gcount();
        
        if (bytesRead == 0) return "";
        
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            if (buffer[i] == 0) return "[Binary file]";
        }
        
        return std::string(buffer.data(), bytesRead);
    }
}

std::string QueryHandlers::createPreview(const std::string& content, const std::string& query) {
    if (query.empty()) return content.substr(0, std::min<size_t>(200, content.size()));
    
    std::string lowerContent = content, lowerQuery = query;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    auto pos = lowerContent.find(lowerQuery);
    if (pos != std::string::npos) {
        size_t start = (pos > 50) ? pos - 50 : 0;
        return content.substr(start, std::min<size_t>(200, content.size() - start));
    }
    
    return content.substr(0, std::min<size_t>(200, content.size()));
}

void QueryHandlers::handleSearch(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    auto query = req.get_param_value("query");
    std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    if (query.empty()) { 
        res.set_content("[]", "application/json"); 
        return; 
    }

    auto results = docManager.search(query);
    json j = json::array();
    
    for (const auto& [id, score] : results) {
        if (!folder.empty() && docManager.getFolder(id) != folder) continue;
        
        auto pathIt = docManager.getDocIdToPath().find(id);
        if (pathIt == docManager.getDocIdToPath().end()) continue;
        
        std::string preview = getQuickPreview(pathIt->second, 500);
        if (!preview.empty()) preview = createPreview(preview, query);
        
        size_t fileSize = 0;
        try {
            if (fs::exists(pathIt->second)) fileSize = fs::file_size(pathIt->second);
        } catch (...) {}
        
        j.push_back({
            {"id", id}, {"score", score}, {"path", pathIt->second},
            {"url", docManager.getDocIdToRel().at(id)}, {"folder", docManager.getFolder(id)},
            {"preview", preview}, {"size", fileSize}
        });
    }
    
    res.set_content(j.dump(), "application/json");
}

void QueryHandlers::handleSuggest(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    auto prefix = req.get_param_value("prefix");
    std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    if (prefix.empty()) {
        res.set_content("[]", "application/json");
        return;
    }
    
    auto allSuggestions = docManager.getSuggestions(prefix);
    
    if (folder.empty()) {
        res.set_content(json(allSuggestions).dump(), "application/json");
        return;
    }
    
    std::unordered_set<std::string> folderTerms;
    for (const auto& [docId, docFolder] : docManager.getDocIdToFolder()) {
        if (docFolder == folder && docManager.getDocTokens().count(docId)) {
            const auto& tokens = docManager.getDocTokens().at(docId);
            for (const auto& token : tokens) {
                if (!token.empty()) folderTerms.insert(token);
            }
        }
    }
    
    std::vector<std::string> filteredSuggestions;
    for (const auto& suggestion : allSuggestions) {
        if (folderTerms.count(suggestion)) filteredSuggestions.push_back(suggestion);
    }
    
    res.set_content(json(filteredSuggestions).dump(), "application/json");
}

void QueryHandlers::handleCorrect(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    auto word = req.get_param_value("word");
    int maxResults = req.has_param("max") ? std::stoi(req.get_param_value("max")) : 3;
    auto corrections = docManager.getCorrections(word, maxResults);
    res.set_content(json(corrections).dump(), "application/json");
}