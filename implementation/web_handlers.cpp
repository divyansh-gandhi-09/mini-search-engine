#include "web_handlers.h"
#include "../third_party/json.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

WebHandlers::WebHandlers(DocumentManager& manager) : docManager(manager) {}

void WebHandlers::setupRoutes(httplib::Server& svr) {
    setupCORS(svr);
    svr.set_mount_point("/", "./public");

    svr.Get("/health", [this](const auto& req, auto& res) { handleHealth(req, res); });
    svr.Get("/stats", [this](const auto& req, auto& res) { handleStats(req, res); });
    svr.Get("/folders", [this](const auto& req, auto& res) { handleFolders(req, res); });
    svr.Post("/folders", [this](const auto& req, auto& res) { handleCreateFolder(req, res); });
    svr.Get("/suggest", [this](const auto& req, auto& res) { handleSuggest(req, res); });
    svr.Get("/correct", [this](const auto& req, auto& res) { handleCorrect(req, res); });
    svr.Get("/search", [this](const auto& req, auto& res) { handleSearch(req, res); });
    svr.Get("/document", [this](const auto& req, auto& res) { handleDocument(req, res); });
    svr.Post("/upload", [this](const auto& req, auto& res) { handleUpload(req, res); });
    svr.Put(R"(/edit/(\d+))", [this](const auto& req, auto& res) { handleEdit(req, res); });
    svr.Delete(R"(/delete/(\d+))", [this](const auto& req, auto& res) { handleDelete(req, res); });
    svr.Get("/documents", [this](const auto& req, auto& res) { handleDocuments(req, res); });
    svr.Post("/rebuild", [this](const auto& req, auto& res) { handleRebuild(req, res); });
    svr.Put(R"(/documents/(\d+)/folder)", [this](const auto& req, auto& res) { handleMoveToFolder(req, res); });
}

void WebHandlers::setupCORS(httplib::Server& svr) {
    svr.set_pre_routing_handler([](const auto&, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    svr.Options(".*", [](const auto&, auto&) { return; });
}

void WebHandlers::handleHealth(const httplib::Request&, httplib::Response& res) {
    res.set_content(R"({"status":"ok"})", "application/json");
}

void WebHandlers::handleStats(const httplib::Request&, httplib::Response& res) {
    json stats;
    
    stats["total_documents"] = docManager.getDocID();
    stats["indexed_documents"] = docManager.getDocIdToPath().size();
    stats["vocabulary_size"] = docManager.getVocabCount().size();
    
    std::unordered_map<std::string, int> folderCounts;
    int rootDocuments = 0;
    
    for (const auto& [id, folder] : docManager.getDocIdToFolder()) {
        if (folder.empty()) {
            rootDocuments++;
        } else {
            folderCounts[folder]++;
        }
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
            if (fs::exists(path)) {
                totalSize += fs::file_size(path);
            }
        } catch (...) {}
    }
    stats["total_content_size"] = totalSize;
    stats["content_in_memory"] = docManager.getDocIdToContent().size();
    
    res.set_content(stats.dump(), "application/json");
}

void WebHandlers::handleFolders(const httplib::Request&, httplib::Response& res) {
    std::unordered_set<std::string> uniqueFolders;
    
    for (const auto& [id, folder] : docManager.getDocIdToFolder()) {
        if (!folder.empty()) uniqueFolders.insert(folder);
    }
    
    try {
        if (fs::exists("./data")) {
            for (const auto& entry : fs::directory_iterator("./data")) {
                if (entry.is_directory()) {
                    uniqueFolders.insert(entry.path().filename().string());
                }
            }
        }
    } catch (...) {}
    
    json j = json::array();
    for (const auto& folder : uniqueFolders) {
        j.push_back(folder);
    }
    res.set_content(j.dump(), "application/json");
}

void WebHandlers::handleCreateFolder(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        std::string folderName = body.value("name", "");
        
        if (folderName.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Folder name is required","success":false})", "application/json");
            return;
        }
        
        if (folderName.find("..") != std::string::npos || 
            folderName.find("/") != std::string::npos || 
            folderName.find("\\") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid folder name","success":false})", "application/json");
            return;
        }
        
        std::string folderPath = "./data/" + folderName;
        
        if (fs::exists(folderPath)) {
            res.status = 409;
            res.set_content(R"({"error":"Folder already exists","success":false})", "application/json");
            return;
        }
        
        fs::create_directories(folderPath);
        
        res.set_content(json({
            {"success", true},
            {"message", "Folder created successfully"},
            {"folder", folderName}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Failed to create folder"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
    }
}

void WebHandlers::handleCorrect(const httplib::Request& req, httplib::Response& res) {
    auto word = req.get_param_value("word");
    int maxResults = req.has_param("max") ? std::stoi(req.get_param_value("max")) : 3;
    auto corrections = docManager.getCorrections(word, maxResults);
    res.set_content(json(corrections).dump(), "application/json");
}

void WebHandlers::handleSuggest(const httplib::Request& req, httplib::Response& res) {
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
                if (!token.empty()) {
                    folderTerms.insert(token);
                }
            }
        }
    }
    
    std::vector<std::string> filteredSuggestions;
    for (const auto& suggestion : allSuggestions) {
        if (folderTerms.count(suggestion)) {
            filteredSuggestions.push_back(suggestion);
        }
    }
    
    res.set_content(json(filteredSuggestions).dump(), "application/json");
}

// ✅ OPTIMIZED: Read only first 500 chars for preview directly from file
std::string getQuickPreview(const std::string& filepath, size_t maxChars = 500) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";
    
    std::string preview;
    preview.resize(maxChars);
    file.read(&preview[0], maxChars);
    preview.resize(file.gcount());
    
    return preview;
}

void WebHandlers::handleSearch(const httplib::Request& req, httplib::Response& res) {
    auto query = req.get_param_value("query");
    std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    if (query.empty()) { 
        res.set_content("[]", "application/json"); 
        return; 
    }

    auto results = docManager.search(query);
    
    json j = json::array();
    j.get_ref<json::array_t&>().reserve(std::min<size_t>(results.size(), 100));
    
    for (const auto& [id, score] : results) {
        if (!folder.empty() && docManager.getFolder(id) != folder) continue;
        
        auto pathIt = docManager.getDocIdToPath().find(id);
        if (pathIt == docManager.getDocIdToPath().end()) continue;
        
        // ✅ CRITICAL FIX: Use quick preview that reads only 500 chars from file
        std::string preview = getQuickPreview(pathIt->second, 500);
        if (!preview.empty()) {
            preview = createPreview(preview, query);
        }
        
        // Get file size without loading content
        size_t fileSize = 0;
        try {
            if (fs::exists(pathIt->second)) {
                fileSize = fs::file_size(pathIt->second);
            }
        } catch (...) {}
        
        j.push_back({
            {"id", id}, 
            {"score", score},
            {"path", pathIt->second},
            {"url", docManager.getDocIdToRel().at(id)},
            {"folder", docManager.getFolder(id)},
            {"preview", preview},
            {"size", fileSize}
        });
    }
    
    res.set_content(j.dump(), "application/json");
}

void WebHandlers::handleDocument(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("id")) { 
        res.status = 400; 
        res.set_content(R"({"error":"Document ID required"})", "application/json");
        return; 
    }
    
    int id;
    try {
        id = std::stoi(req.get_param_value("id"));
    } catch (const std::exception&) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid document ID"})", "application/json");
        return;
    }
    
    if (!docManager.getDocIdToPath().count(id)) { 
        res.status = 404; 
        res.set_content(R"({"error":"Document not found"})", "application/json");
        return; 
    }

    std::string content = docManager.getDocumentContent(id);
    
    res.set_content(json({
        {"id", id},
        {"path", docManager.getDocIdToPath().at(id)},
        {"url", docManager.getDocIdToRel().at(id)},
        {"folder", docManager.getFolder(id)},
        {"content", content},
        {"size", content.length()}
    }).dump(), "application/json");
}

void WebHandlers::handleUpload(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string filename, content, folder;

        if (req.is_multipart_form_data()) {
            auto file = req.get_file_value("file");
            if (!file.filename.empty()) {
                if (req.has_param("filename") && !req.get_param_value("filename").empty()) {
                    filename = req.get_param_value("filename");
                } else {
                    filename = file.filename;
                }
                
                if (req.has_param("folder")) {
                    folder = req.get_param_value("folder");
                }

                httplib::Client cli("127.0.0.1", 5000);
                cli.set_connection_timeout(0, 300000);
                cli.set_read_timeout(60, 0);
                
                httplib::MultipartFormDataItems items;
                items.push_back({"file", file.content, file.filename, file.content_type});

                auto extractorResponse = cli.Post("/extract", items);
                if (!extractorResponse) {
                    res.status = 500;
                    res.set_content(R"({"error":"Could not reach extractor service","success":false})", "application/json");
                    return;
                }

                if (extractorResponse->status != 200) {
                    res.status = 500;
                    res.set_content(json({{"error", "Extractor service failed"}, {"success", false}}).dump(), "application/json");
                    return;
                }

                auto extractorJson = json::parse(extractorResponse->body);
                content = extractorJson.value("text", "");
                
                if (content.empty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"No text extracted","success":false})", "application/json");
                    return;
                }
            } else {
                res.status = 400;
                res.set_content(R"({"error":"No file provided","success":false})", "application/json");
                return;
            }
        } else {
            auto body = json::parse(req.body);
            filename = body.value("filename", "");
            content = body.value("content", "");
            folder = body.value("folder", "");
            
            if (filename.empty() || content.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Filename and content required","success":false})", "application/json");
                return;
            }
        }

        int newId = docManager.uploadDocument(filename, content, folder);
        
        res.set_content(json({
            {"status", "uploaded"},
            {"success", true},
            {"id", newId},
            {"filename", filename},
            {"folder", folder},
            {"extracted_chars", content.length()}
        }).dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Upload failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

void WebHandlers::handleEdit(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        
        if (!body.contains("content")) {
            res.status = 400;
            res.set_content(R"({"error":"Content required","success":false})", "application/json");
            return;
        }
        
        if (!docManager.editDocument(id, body["content"])) {
            res.status = 404;
            res.set_content(R"({"error":"Document not found","success":false})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "updated"}, {"success", true}, {"id", id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Edit failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

void WebHandlers::handleDelete(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        if (!docManager.deleteDocument(id)) {
            res.status = 404;
            res.set_content(R"({"error":"Document not found","success":false})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "deleted"}, {"success", true}, {"id", id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Delete failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

void WebHandlers::handleMoveToFolder(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        
        std::string newFolder = body.value("folder", "");
        
        if (!docManager.getDocIdToPath().count(id)) {
            res.status = 404;
            res.set_content(R"({"error":"Document not found","success":false})", "application/json");
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
            {"status", "moved"},
            {"success", true},
            {"old_id", id},
            {"new_id", newId},
            {"old_folder", oldFolder},
            {"new_folder", newFolder}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Move failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

// ✅ OPTIMIZED: Read only first 500 chars for listing preview
void WebHandlers::handleDocuments(const httplib::Request& req, httplib::Response& res) {
    std::string folderFilter = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    json j = json::array();
    std::vector<int> ids;
    ids.reserve(docManager.getDocIdToRel().size());
    
    for (const auto& [id, _] : docManager.getDocIdToRel()) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    
    j.get_ref<json::array_t&>().reserve(ids.size());
    
    for (int id : ids) {
        std::string folder = docManager.getFolder(id);
        
        if (!folderFilter.empty() && folder != folderFilter) {
            continue;
        }
        
        auto pathIt = docManager.getDocIdToPath().find(id);
        if (pathIt == docManager.getDocIdToPath().end()) continue;
        
        // ✅ CRITICAL FIX: Read only 200 chars for preview instead of full content
        std::string preview = getQuickPreview(pathIt->second, 200);
        
        // Get file size without loading content
        size_t fileSize = 0;
        try {
            if (fs::exists(pathIt->second)) {
                fileSize = fs::file_size(pathIt->second);
            }
        } catch (...) {}
        
        j.push_back({
            {"id", id},
            {"filename", docManager.getDocIdToRel().at(id)},
            {"path", pathIt->second},
            {"folder", folder},
            {"preview", preview},
            {"size", fileSize}
        });
    }
    
    res.set_content(j.dump(), "application/json");
}

void WebHandlers::handleRebuild(const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(R"({"error":"Invalid action","success":false})", "application/json");
            return;
        }
        
        res.set_content(json({{"status", "completed"}, {"success", true}, {"action", action}}).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Rebuild failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

std::string WebHandlers::createPreview(const std::string& content, const std::string& query) {
    if (query.empty()) {
        return content.substr(0, std::min<size_t>(200, content.size()));
    }
    
    std::string lowerContent = content;
    std::string lowerQuery = query;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    auto pos = lowerContent.find(lowerQuery);
    if (pos != std::string::npos) {
        size_t start = (pos > 50) ? pos - 50 : 0;
        size_t length = std::min<size_t>(200, content.size() - start);
        return content.substr(start, length);
    }
    
    return content.substr(0, std::min<size_t>(200, content.size()));
}