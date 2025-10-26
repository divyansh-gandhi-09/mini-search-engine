#include "document_handlers.h"
#include "query_handlers.h"
#include "../third_party/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
    std::string getQuickPreview(const std::string& filepath, size_t maxChars) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) return "";
        
        std::vector<char> buffer(maxChars);
        file.read(buffer.data(), maxChars);
        std::streamsize bytesRead = file.gcount();
        
        if (bytesRead == 0) return "";
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            if (buffer[i] == 0) return "[Binary]";
        }
        
        return std::string(buffer.data(), bytesRead);
    }
}

void DocumentHandlers::handleDocuments(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    std::string folderFilter = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    json j = json::array();
    std::vector<int> ids;
    ids.reserve(docManager.getDocIdToRel().size());
    
    for (const auto& [id, _] : docManager.getDocIdToRel()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        std::string folder = docManager.getFolder(id);
        if (!folderFilter.empty() && folder != folderFilter) continue;
        
        auto pathIt = docManager.getDocIdToPath().find(id);
        if (pathIt == docManager.getDocIdToPath().end()) continue;
        
        std::string preview = getQuickPreview(pathIt->second, 200);
        size_t fileSize = 0;
        try {
            if (fs::exists(pathIt->second)) fileSize = fs::file_size(pathIt->second);
        } catch (...) {}
        
        j.push_back({
            {"id", id}, {"filename", docManager.getDocIdToRel().at(id)},
            {"path", pathIt->second}, {"folder", folder},
            {"preview", preview}, {"size", fileSize}
        });
    }
    
    res.set_content(j.dump(), "application/json");
}

void DocumentHandlers::handleDocument(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("id")) { 
        res.status = 400; 
        res.set_content(R"({"error":"ID required"})", "application/json");
        return; 
    }
    
    int id;
    try {
        id = std::stoi(req.get_param_value("id"));
    } catch (const std::exception&) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid ID"})", "application/json");
        return;
    }
    
    if (!docManager.getDocIdToPath().count(id)) { 
        res.status = 404; 
        res.set_content(R"({"error":"Not found"})", "application/json");
        return; 
    }

    std::string content = docManager.getDocumentContent(id);
    
    res.set_content(json({
        {"id", id}, {"path", docManager.getDocIdToPath().at(id)},
        {"url", docManager.getDocIdToRel().at(id)}, {"folder", docManager.getFolder(id)},
        {"content", content}, {"size", content.length()}
    }).dump(), "application/json");
}

void DocumentHandlers::handleEdit(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        
        if (!body.contains("content")) {
            res.status = 400;
            res.set_content(R"({"error":"Content required"})", "application/json");
            return;
        }
        
        if (!docManager.editDocument(id, body["content"])) {
            res.status = 404;
            res.set_content(R"({"error":"Not found"})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "updated"}, {"success", true}, {"id", id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Edit failed"}, {"details", e.what()}}).dump(), "application/json");
    }
}

void DocumentHandlers::handleDelete(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        if (!docManager.deleteDocument(id)) {
            res.status = 404;
            res.set_content(R"({"error":"Not found"})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "deleted"}, {"success", true}, {"id", id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Delete failed"}, {"details", e.what()}}).dump(), "application/json");
    }
}

void DocumentHandlers::handleMoveToFolder(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::string newFolder = body.value("folder", "");
        
        if (!docManager.getDocIdToPath().count(id)) {
            res.status = 404;
            res.set_content(R"({"error":"Not found"})", "application/json");
            return;
        }
        
        std::string filename = docManager.getDocIdToRel().at(id);
        std::string content = docManager.getDocumentContent(id);
        std::string oldFolder = docManager.getFolder(id);
        
        if (oldFolder == newFolder) {
            res.set_content(json({{"status", "unchanged"}, {"success", true}}).dump(), "application/json");
            return;
        }
        
        int newId = docManager.uploadDocument(filename, content, newFolder);
        docManager.deleteDocument(id);
        
        res.set_content(json({
            {"status", "moved"}, {"success", true},
            {"old_id", id}, {"new_id", newId},
            {"old_folder", oldFolder}, {"new_folder", newFolder}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Move failed"}, {"details", e.what()}}).dump(), "application/json");
    }
}

void DocumentHandlers::handleRebuild(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    try {
        std::string action = "fresh";
        if (!req.body.empty()) {
            auto body = json::parse(req.body);
            action = body.value("action", "fresh");
        }
        
        if (action == "fresh") {
            docManager.buildFreshIndex();
        } else if (action == "update") {
            docManager.updateExistingIndex();
        } else {
            res.status = 400;
            res.set_content(R"({"error":"Invalid action"})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "completed"}, {"success", true}, {"action", action}}).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Rebuild failed"}, {"details", e.what()}}).dump(), "application/json");
    }
}