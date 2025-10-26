#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"

namespace DocumentHandlers {
    void handleDocuments(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleDocument(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleEdit(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleDelete(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleMoveToFolder(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleRebuild(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
}