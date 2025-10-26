#pragma once
#include "../third_party/httplib.h"
#include "document_manager.h"

namespace StatsHandlers {
    void handleHealth(const httplib::Request& req, httplib::Response& res);
    void handleStats(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res);
}