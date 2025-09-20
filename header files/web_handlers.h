#pragma once

#include "../third_party/httplib.h"
#include "document_manager.h"

class WebHandlers {
private:
    DocumentManager& docManager;

public:
    explicit WebHandlers(DocumentManager& manager);
    
    // Setup handlers
    void setupRoutes(httplib::Server& svr);
    
private:
    // Handler methods
    void handleHealth(const httplib::Request& req, httplib::Response& res);
    void handleStats(const httplib::Request& req, httplib::Response& res);
    void handleFolders(const httplib::Request& req, httplib::Response& res);
    void handleCreateFolder(const httplib::Request& req, httplib::Response& res);
    void handleSuggest(const httplib::Request& req, httplib::Response& res);
    void handleCorrect(const httplib::Request& req, httplib::Response& res);
    void handleSearch(const httplib::Request& req, httplib::Response& res);
    void handleDocument(const httplib::Request& req, httplib::Response& res);
    void handleUpload(const httplib::Request& req, httplib::Response& res);
    void handleEdit(const httplib::Request& req, httplib::Response& res);
    void handleDelete(const httplib::Request& req, httplib::Response& res);
    void handleDocuments(const httplib::Request& req, httplib::Response& res);
    void handleRebuild(const httplib::Request& req, httplib::Response& res);
    void handleMoveToFolder(const httplib::Request& req, httplib::Response& res);
    // Helper methods
    void setupCORS(httplib::Server& svr);
    std::string createPreview(const std::string& content, const std::string& query);
};