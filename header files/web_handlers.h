#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"

class WebHandlers {
private:
    DocumentManager& docManager;
    void setupCORS(httplib::Server& svr);

public:
    explicit WebHandlers(DocumentManager& manager);
    void setupRoutes(httplib::Server& svr);
};