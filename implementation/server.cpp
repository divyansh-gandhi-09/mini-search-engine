#include <iostream>
#include "../third_party/httplib.h"
#include "document_manager.h"
#include "web_handlers.h"

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";

    // Initialize document manager
    DocumentManager docManager;
    bool freshBuild = docManager.initialize();
    
    if (freshBuild) {
        std::cout << "Fresh index built successfully.\n";
    } else {
        std::cout << "Loaded existing index and updated.\n";
    }

    // Setup web server
    httplib::Server svr;
    WebHandlers handlers(docManager);
    handlers.setupRoutes(svr);

    std::cout << "🚀 Server running at http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}