#include <iostream>
#include <filesystem>
#include "../third_party/httplib.h"
#include "document_manager.h"
#include "web_handlers.h"

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";

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
    
    // ✅ FIX: Set timeouts for large uploads
    svr.set_read_timeout(1200, 0);   // 10 minutes for reading request
    svr.set_write_timeout(1200, 0);  // 10 minutes for writing response
    svr.set_keep_alive_max_count(100);  // Allow keep-alive connections
    svr.set_keep_alive_timeout(60);  // 10 seconds keep-alive timeout
    svr.set_payload_max_length(1024 * 1024 * 1024); // 1GB max payload
    
    WebHandlers handlers(docManager);
    handlers.setupRoutes(svr);

    std::cout << "Server running at http://localhost:8080\n";
    std::cout << "Timeouts: Read/Write = 600s, Keep-Alive = 5s\n";
    std::cout << "Max payload: 1GB\n";
    std::cout << "Make sure the Python extractor is running on port 5000\n";
    
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server on port 8080\n";
        return 1;
    }

    return 0;
}