#include "web_handlers.h"
#include "upload_handlers.h"
#include "query_handlers.h"
#include "document_handlers.h"
#include "folder_handlers.h"
#include "stats_handlers.h"
#include "../third_party/json.hpp"

using json = nlohmann::json;

WebHandlers::WebHandlers(DocumentManager& manager) : docManager(manager) {}

void WebHandlers::setupCORS(httplib::Server& svr) {
    //  Pre-routing handler for ALL requests including errors
    svr.set_pre_routing_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Max-Age", "86400"); // 24 hours
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    //  Error handler to ensure CORS on error responses
    svr.set_error_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        
        // Provide proper JSON error response
        json error_response = {
            {"error", "Request failed"},
            {"status", res.status},
            {"success", false}
        };
        res.set_content(error_response.dump(), "application/json");
    });
    
    // Handle OPTIONS requests
    svr.Options(".*", [](const auto&, auto& res) { 
        res.status = 204; // No Content
        return; 
    });
}
void WebHandlers::setupRoutes(httplib::Server& svr) {
    setupCORS(svr);
    svr.set_mount_point("/", "./public");
    // Health & Stats
    svr.Get("/health", [](const auto& req, auto& res) { 
        StatsHandlers::handleHealth(req, res); 
    });
    svr.Get("/stats", [this](const auto& req, auto& res) { 
        StatsHandlers::handleStats(docManager, req, res); 
    });

    // Folders
    svr.Get("/folders", [this](const auto& req, auto& res) { 
        FolderHandlers::handleFolders(docManager, req, res); 
    });
    svr.Post("/folders", [this](const auto& req, auto& res) { 
        FolderHandlers::handleCreateFolder(docManager, req, res); 
    });

    // Query operations
    svr.Get("/suggest", [this](const auto& req, auto& res) { 
        QueryHandlers::handleSuggest(docManager, req, res); 
    });
    svr.Get("/correct", [this](const auto& req, auto& res) { 
        QueryHandlers::handleCorrect(docManager, req, res); 
    });
    svr.Get("/search", [this](const auto& req, auto& res) { 
        QueryHandlers::handleSearch(docManager, req, res); 
    });

    // Upload operations
    svr.Post("/upload", [this](const auto& req, auto& res) { 
        UploadHandlers::handleUpload(docManager, req, res); 
    });
    svr.Post("/upload/batch", [this](const auto& req, auto& res) { 
        UploadHandlers::handleBatchUpload(docManager, req, res); 
    });
    svr.Post("/upload/folder", [this](const auto& req, auto& res) { 
        UploadHandlers::handleFolderUpload(docManager, req, res); 
    });

    // Document CRUD
    svr.Get("/documents", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleDocuments(docManager, req, res); 
    });
    svr.Get("/document", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleDocument(docManager, req, res); 
    });
    svr.Put(R"(/edit/(\d+))", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleEdit(docManager, req, res); 
    });
    svr.Delete(R"(/delete/(\d+))", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleDelete(docManager, req, res); 
    });
    svr.Put(R"(/documents/(\d+)/folder)", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleMoveToFolder(docManager, req, res); 
    });
    svr.Post("/rebuild", [this](const auto& req, auto& res) { 
        DocumentHandlers::handleRebuild(docManager, req, res); 
    });
}