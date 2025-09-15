#include "web_handlers.h"
#include "../third_party/json.hpp"
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

WebHandlers::WebHandlers(DocumentManager& manager) : docManager(manager) {}

void WebHandlers::setupRoutes(httplib::Server& svr) {
    setupCORS(svr);
    
    // Static frontend
    svr.set_mount_point("/", "./public");
    
    // API routes
    svr.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
    });
    
    svr.Get("/suggest", [this](const httplib::Request& req, httplib::Response& res) {
        handleSuggest(req, res);
    });
    
    svr.Get("/correct", [this](const httplib::Request& req, httplib::Response& res) {
        handleCorrect(req, res);
    });
    
    svr.Get("/search", [this](const httplib::Request& req, httplib::Response& res) {
        handleSearch(req, res);
    });
    
    svr.Get("/document", [this](const httplib::Request& req, httplib::Response& res) {
        handleDocument(req, res);
    });
    
    svr.Post("/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpload(req, res);
    });
    
    svr.Put(R"(/edit/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleEdit(req, res);
    });
    
    svr.Delete(R"(/delete/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDelete(req, res);
    });
    
    svr.Get("/documents", [this](const httplib::Request& req, httplib::Response& res) {
        handleDocuments(req, res);
    });
}

void WebHandlers::setupCORS(httplib::Server& svr) {
    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    svr.Options(".*", [](const httplib::Request&, httplib::Response&) { return; });
}

void WebHandlers::handleHealth(const httplib::Request&, httplib::Response& res) {
    res.set_content(R"({"status":"ok"})", "application/json");
}

void WebHandlers::handleSuggest(const httplib::Request& req, httplib::Response& res) {
    auto prefix = req.get_param_value("prefix");
    auto suggestions = docManager.getSuggestions(prefix);
    res.set_content(json(suggestions).dump(), "application/json");
}

void WebHandlers::handleCorrect(const httplib::Request& req, httplib::Response& res) {
    auto word = req.get_param_value("word");
    int maxResults = req.has_param("max") ? std::stoi(req.get_param_value("max")) : 3;
    
    auto corrections = docManager.getCorrections(word, maxResults);
    res.set_content(json(corrections).dump(), "application/json");
}

void WebHandlers::handleSearch(const httplib::Request& req, httplib::Response& res) {
    auto query = req.get_param_value("query");
    if (query.empty()) {
        res.set_content("[]", "application/json");
        return;
    }
    
    try {
        auto results = docManager.search(query);
        const auto& docContent = docManager.getDocIdToContent();
        const auto& docPaths = docManager.getDocIdToPath();
        const auto& docRels = docManager.getDocIdToRel();
        
        json j = json::array();
        for (const auto& result : results) {
            std::string content = docContent.at(result.first);
            std::string preview = createPreview(content, query);
            
            j.push_back({
                {"id", result.first},
                {"score", result.second},
                {"path", docPaths.at(result.first)},
                {"url", docRels.at(result.first)},
                {"preview", preview}
            });
        }
        
        res.set_content(j.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", e.what()}}).dump(), "application/json");
    }
}

void WebHandlers::handleDocument(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("id")) {
        res.status = 400;
        return;
    }
    
    int id = std::stoi(req.get_param_value("id"));
    const auto& docPaths = docManager.getDocIdToPath();
    const auto& docRels = docManager.getDocIdToRel();
    const auto& docContent = docManager.getDocIdToContent();
    
    if (!docPaths.count(id)) {
        res.status = 404;
        return;
    }
    
    res.set_content(json({
        {"id", id},
        {"path", docPaths.at(id)},
        {"url", docRels.at(id)},
        {"content", docContent.at(id)}
    }).dump(), "application/json");
}

void WebHandlers::handleUpload(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        std::string filename = body["filename"];
        std::string content = body["content"];
        
        int newId = docManager.uploadDocument(filename, content);
        
        res.set_content(json({
            {"status", "uploaded"},
            {"id", newId},
            {"filename", filename}
        }).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", std::string("upload failed: ") + e.what()}
        }).dump(), "application/json");
    }
}

void WebHandlers::handleEdit(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::string newContent = body["content"];
        
        if (!docManager.editDocument(id, newContent)) {
            res.status = 404;
            res.set_content(R"({"error":"document not found"})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "updated"}, {"id", id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", std::string("edit failed: ") + e.what()}
        }).dump(), "application/json");
    }
}

void WebHandlers::handleDelete(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        
        if (!docManager.deleteDocument(id)) {
            res.status = 404;
            res.set_content(R"({"error":"document not found"})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "deleted"}, {"id", id}}).dump(), "application/json");
        std::cout << "[INFO] Deleted document ID=" << id << "\n";
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", std::string("delete failed: ") + e.what()}
        }).dump(), "application/json");
        std::cerr << "[ERROR] Delete failed: " << e.what() << "\n";
    }
}

void WebHandlers::handleDocuments(const httplib::Request&, httplib::Response& res) {
    const auto& docRels = docManager.getDocIdToRel();
    const auto& docPaths = docManager.getDocIdToPath();
    const auto& docContent = docManager.getDocIdToContent();  // Add this line
    
    json j = json::array();
    std::vector<int> ids;
    for (auto& [id, _] : docRels) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        // Get content and create preview
        std::string content = docContent.count(id) ? docContent.at(id) : "";
        std::string preview = content.empty() ? "" : 
            content.substr(0, std::min<size_t>(200, content.size()));
        
        j.push_back({
            {"id", id},
            {"filename", docRels.at(id)},
            {"path", docPaths.at(id)},
            {"preview", preview},           // Add preview
            {"content", content}            // Add full content (optional)
        });
    }
    
    res.set_content(j.dump(), "application/json");
}

std::string WebHandlers::createPreview(const std::string& content, const std::string& query) {
    auto pos = content.find(query);
    std::string preview;
    
    if (pos != std::string::npos) {
        size_t start = (pos > 50) ? pos - 50 : 0;
        preview = content.substr(start, 200);
    } else {
        preview = content.substr(0, std::min<size_t>(200, content.size()));
    }
    
    return preview;
}