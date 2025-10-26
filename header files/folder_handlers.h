#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"

namespace FolderHandlers {
    void handleFolders(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
    void handleCreateFolder(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
}