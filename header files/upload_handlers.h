#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"

namespace UploadHandlers {
    void handleBatchUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleFolderUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
}