#include "web_handlers.h"
#include "../third_party/json.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <sstream>

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
    
    // Document counts
    stats["total_documents"] = docManager.getDocID();
    stats["indexed_documents"] = docManager.getDocIdToPath().size();
    stats["vocabulary_size"] = docManager.getVocabCount().size();
    
    // Folder stats
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
    
    // File type distribution
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
    
    // Size stats
    size_t totalSize = 0;
    for (const auto& [id, content] : docManager.getDocIdToContent()) {
        totalSize += content.size();
    }
    stats["total_content_size"] = totalSize;
    
    res.set_content(stats.dump(), "application/json");
}

void WebHandlers::handleFolders(const httplib::Request&, httplib::Response& res) {
    std::unordered_set<std::string> uniqueFolders;
    
    // Get folders from documents
    for (const auto& [id, folder] : docManager.getDocIdToFolder()) {
        if (!folder.empty()) uniqueFolders.insert(folder);
    }
    
    // Also scan data directory for empty folders
    try {
        if (fs::exists("./data")) {
            for (const auto& entry : fs::directory_iterator("./data")) {
                if (entry.is_directory()) {
                    std::string folderName = entry.path().filename().string();
                    uniqueFolders.insert(folderName);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not scan data directory: " << e.what() << "\n";
    }
    
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
        
        // Validate folder name
        if (folderName.find("..") != std::string::npos || 
            folderName.find("/") != std::string::npos || 
            folderName.find("\\") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid folder name. Cannot contain '..' or path separators","success":false})", "application/json");
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
// Added this new method to WebHandlers class in web_handlers.cpp

void WebHandlers::handleSuggest(const httplib::Request& req, httplib::Response& res) {
    auto prefix = req.get_param_value("prefix");
    std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    if (prefix.empty()) {
        res.set_content("[]", "application/json");
        return;
    }
    
    // Get all suggestions first
    auto allSuggestions = docManager.getSuggestions(prefix);
    
    // If no folder filter is specified, return all suggestions
    if (folder.empty()) {
        res.set_content(json(allSuggestions).dump(), "application/json");
        return;
    }
    
    // Filter suggestions based on folder context
    std::unordered_set<std::string> folderTerms;
    
    // Collect all terms from documents in the specified folder
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
    
    // Filter suggestions to only include terms that exist in the folder
    std::vector<std::string> filteredSuggestions;
    for (const auto& suggestion : allSuggestions) {
        if (folderTerms.count(suggestion)) {
            filteredSuggestions.push_back(suggestion);
        }
    }
    
    res.set_content(json(filteredSuggestions).dump(), "application/json");
}

void WebHandlers::handleSearch(const httplib::Request& req, httplib::Response& res) {
    auto query = req.get_param_value("query");
    std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
    if (query.empty()) { 
        res.set_content("[]", "application/json"); 
        return; 
    }

    auto results = docManager.search(query);
    std::cout << "Search for '" << query << "' returned " << results.size() << " results\n";
    if (!results.empty()) {  
    auto [id, score] = *results.begin();   // take the first (best) result
    std::cout << "  Best matched Document " << id << ": score = " << score << "\n"; 
    }
    json j = json::array();
    
    for (const auto& [id, score] : results) {
        // Apply folder filter if specified
        if (!folder.empty() && docManager.getFolder(id) != folder) continue;
        
        // Skip if document no longer exists
        if (!docManager.getDocIdToContent().count(id)) {
            continue;
        }
        std::string content = docManager.getDocumentContent(id);
        if (content.empty()) {
            std::cout << "DEBUG: Content is empty for doc " << id << "\n";
            continue;
        }
        std::string preview = createPreview(content, query);
        
        j.push_back({
            {"id", id}, 
            {"score", score},
            {"path", docManager.getDocIdToPath().at(id)},
            {"url", docManager.getDocIdToRel().at(id)},
            {"folder", docManager.getFolder(id)},
            {"preview", preview},
            {"size", content.length()}
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

    res.set_content(json({
        {"id", id},
        {"path", docManager.getDocIdToPath().at(id)},
        {"url", docManager.getDocIdToRel().at(id)},
        {"folder", docManager.getFolder(id)},
        {"content", docManager.getDocumentContent(id)},
        {"size", docManager.getDocIdToContent().at(id).length()}
    }).dump(), "application/json");
}
// Fix for web_handlers.cpp - handleUpload function
// ensures custom filename and folder are properly received from FormData

void WebHandlers::handleUpload(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string filename, content, folder;

        // Handle multipart form-data (file upload)
        if (req.is_multipart_form_data()) {
            auto file = req.get_file_value("file");
            if (!file.filename.empty()) {
                // Get custom filename from form parameter OR use original filename
                if (req.has_param("filename") && !req.get_param_value("filename").empty()) {
                    filename = req.get_param_value("filename");
                    std::cout << "Using custom filename: " << filename << "\n";
                } else {
                    filename = file.filename;
                    std::cout << "Using original filename: " << filename << "\n";
                }
                
                // Get folder parameter
                if (req.has_param("folder")) {
                    folder = req.get_param_value("folder");
                    std::cout << "Target folder: " << (folder.empty() ? "[root]" : folder) << "\n";
                }

                // Call Python extractor service
                httplib::Client cli("127.0.0.1", 5000);
                cli.set_connection_timeout(0, 300000);
                cli.set_read_timeout(60, 0);
                
                httplib::MultipartFormDataItems items;
                items.push_back({"file", file.content, file.filename, file.content_type});

                auto extractorResponse = cli.Post("/extract", items);
                if (!extractorResponse) {
                    res.status = 500;
                    res.set_content(R"({"error":"Could not reach extractor service. Make sure Python extractor is running on port 5000","success":false})", "application/json");
                    return;
                }

                if (extractorResponse->status != 200) {
                    res.status = 500;
                    json error_json = {
                        {"error", "Extractor service failed"},
                        {"details", extractorResponse->body},
                        {"success", false}
                    };
                    res.set_content(error_json.dump(), "application/json");
                    return;
                }

                try {
                    auto extractorJson = json::parse(extractorResponse->body);
                    content = extractorJson.value("text", "");
                    
                    if (content.empty()) {
                        res.status = 400;
                        res.set_content(R"({"error":"No text could be extracted from the file","success":false})", "application/json");
                        return;
                    }
                } catch (const json::parse_error& e) {
                    res.status = 500;
                    json error_json = {
                        {"error", "Invalid response from extractor service"},
                        {"details", e.what()},
                        {"success", false}
                    };
                    res.set_content(error_json.dump(), "application/json");
                    return;
                }
            } else {
                res.status = 400;
                res.set_content(R"({"error":"No file provided","success":false})", "application/json");
                return;
            }
        } 
        // Handle raw JSON (manual pasted content)
        else {
            try {
                auto body = json::parse(req.body);
                filename = body.value("filename", "");
                content = body.value("content", "");
                folder = body.value("folder", "");
                
                std::cout << "Manual upload - Filename: " << filename 
                          << ", Folder: " << (folder.empty() ? "[root]" : folder) << "\n";
                
                if (filename.empty() || content.empty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"Both filename and content are required","success":false})", "application/json");
                    return;
                }
            } catch (const json::parse_error& e) {
                res.status = 400;
                json error_json = {
                    {"error", "Invalid JSON in request body"},
                    {"details", e.what()},
                    {"success", false}
                };
                res.set_content(error_json.dump(), "application/json");
                return;
            }
        }

        // Upload document with specified folder
        int newId = docManager.uploadDocument(filename, content, folder);
        
         /*AUTO-UPDATE: Automatically update the index after upload
        std::cout << "Auto-updating index after upload..." << std::endl;
        docManager.updateExistingIndex();*/
        
        json response_json = {
            {"status", "uploaded"},
            {"success", true},
            {"id", newId},
            {"filename", filename},
            {"folder", folder},
            {"extracted_chars", content.length()},
            {"message", "Document uploaded and indexed successfully"}
        };
        res.set_content(response_json.dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 500;
        json error_json = {
            {"error", "Upload failed"},
            {"details", e.what()},
            {"success", false}
        };
        res.set_content(error_json.dump(), "application/json");
    }
}

void WebHandlers::handleEdit(const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        
        if (!body.contains("content")) {
            res.status = 400;
            res.set_content(R"({"error":"Content field is required","success":false})", "application/json");
            return;
        }
        
        if (!docManager.editDocument(id, body["content"])) {
            res.status = 404;
            res.set_content(R"({"error":"Document not found","success":false})", "application/json");
            return;
        }
        
        /* AUTO-UPDATE: Automatically update the index after edit
        std::cout << "Auto-updating index after edit..." << std::endl;
        docManager.updateExistingIndex();*/
        
        res.set_content(json({
            {"status", "updated"}, 
            {"success", true},
            {"id", id},
            {"message", "Document updated successfully"}
        }).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Edit failed"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
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
        
        /* AUTO-UPDATE: Automatically update the index after delete
        std::cout << "Auto-updating index after delete..." << std::endl;
        docManager.updateExistingIndex(); */
        
        res.set_content(json({
            {"status", "deleted"}, 
            {"success", true},
            {"id", id},
            {"message", "Document deleted successfully"}
        }).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Delete failed"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
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
        
        // Get current document info
        std::string filename = docManager.getDocIdToRel().at(id);
        std::string content = docManager.getDocumentContent(id);
        std::string oldFolder = docManager.getFolder(id);
        
        // Don't move if already in target folder
        if (oldFolder == newFolder) {
            res.set_content(json({
                {"status", "unchanged"},
                {"success", true},
                {"id", id},
                {"folder", newFolder},
                {"message", "Document already in target folder"}
            }).dump(), "application/json");
            return;
        }
        
        // Create new document first (BEFORE deleting old one)
        int newId;
        try {
            newId = docManager.uploadDocument(filename, content, newFolder);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({
                {"error", "Failed to create document in new location"},
                {"details", e.what()},
                {"success", false}
            }).dump(), "application/json");
            return;
        }
        
        // Only delete old if upload succeeds
        try {
            docManager.deleteDocument(id);
        } catch (const std::exception& e) {
            // If delete fails, try to clean up the new document
            std::cerr << "ERROR: Failed to delete old document after move, attempting rollback\n";
            try {
                docManager.deleteDocument(newId);
            } catch (...) {
                std::cerr << "ERROR: Rollback failed - manual cleanup may be needed\n";
            }
            
            res.status = 500;
            res.set_content(json({
                {"error", "Failed to delete old document after move"},
                {"details", e.what()},
                {"success", false}
            }).dump(), "application/json");
            return;
        }
        
        /* AUTO-UPDATE: Update index after move
        std::cout << "Auto-updating index after folder move..." << std::endl;
        docManager.updateExistingIndex();*/
        
        res.set_content(json({
            {"status", "moved"},
            {"success", true},
            {"old_id", id},
            {"new_id", newId},
            {"old_folder", oldFolder},
            {"new_folder", newFolder},
            {"message", "Document moved to folder successfully"}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Move failed"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
    }
}

void WebHandlers::handleDocuments(const httplib::Request& req, httplib::Response& res) {
    std::string folderFilter = req.has_param("folder") ? req.get_param_value("folder") : "";
    
    json j = json::array();
    std::vector<int> ids;
    for (const auto& [id, _] : docManager.getDocIdToRel()) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        std::string folder = docManager.getFolder(id);
        
        // Apply folder filter if specified
        if (!folderFilter.empty() && folder != folderFilter) {
            continue;
        }
        
        std::string content = docManager.getDocIdToContent().count(id)
            ? docManager.getDocIdToContent().at(id) : "";
        std::string preview = content.substr(0, std::min<size_t>(200, content.size()));
        
        j.push_back({
            {"id", id},
            {"filename", docManager.getDocIdToRel().at(id)},
            {"path", docManager.getDocIdToPath().at(id)},
            {"folder", folder},
            {"preview", preview},
            {"size", content.length()}
        });
    }
    res.set_content(j.dump(), "application/json");
}

void WebHandlers::handleRebuild(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string action = "fresh"; // default
        if (!req.body.empty()) {
            auto body = json::parse(req.body);
            action = body.value("action", "fresh");
        }
        
        if (action == "fresh") {
            std::cout << "Manual fresh index rebuild requested..." << std::endl;
            docManager.buildFreshIndex();
        } else if (action == "update") {
            std::cout << "Manual index update requested..." << std::endl;
            docManager.updateExistingIndex();
        } else {
            res.status = 400;
            res.set_content(R"({"error":"Invalid action. Use 'fresh' or 'update'","success":false})", "application/json");
            return;
        }
        
        res.set_content(json({
            {"status", "completed"},
            {"success", true},
            {"action", action},
            {"message", "Index rebuild completed successfully"}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Rebuild failed"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
    }
}

std::string WebHandlers::createPreview(const std::string& content, const std::string& query) {
    if (query.empty()) {
        return content.substr(0, std::min<size_t>(200, content.size()));
    }
    
    // Try to find query terms in content for better context
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