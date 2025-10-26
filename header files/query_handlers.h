#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"
#include <string>

namespace QueryHandlers {
    void handleSearch(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleSuggest(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleCorrect(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    std::string createPreview(const std::string& content, const std::string& query);
}