#include <iostream>
#include <filesystem>
#include "../third_party/httplib.h"
#include "document_manager.h"
#include "web_handlers.h"

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";

    // Create necessary directories if they don't exist
    if (!std::filesystem::exists("./data")) {
        std::filesystem::create_directory("./data");
        std::cout << "Created ./data directory\n";
    }
    
    if (!std::filesystem::exists("./uploads")) {
        std::filesystem::create_directory("./uploads");
        std::cout << "Created ./uploads directory\n";
    }

    DocumentManager docManager;
    if (!docManager.initialize()) {
        std::cout << "No existing index found. Will build fresh index on first document addition.\n";
    } else {
        std::cout << "Loaded existing index successfully.\n";
    }

    httplib::Server svr;
    WebHandlers handlers(docManager);
    handlers.setupRoutes(svr);

    std::cout << "Server running at http://localhost:8080\n";
    std::cout << "Make sure the Python extractor is running on port 5000\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}