#include "folder_handlers.h"
#include "../third_party/json.hpp"
#include <filesystem>
#include <unordered_set>

using json = nlohmann::json;
namespace fs = std::filesystem;

void FolderHandlers::handleFolders(DocumentManager& docManager, const httplib::Request&, httplib::Response& res) {
    std::unordered_set<std::string> uniqueFolders;
    
    for (const auto& [id, folder] : docManager.getDocIdToFolder()) {
        if (!folder.empty()) uniqueFolders.insert(folder);
    }
    
    try {
        if (fs::exists("./data")) {
            for (const auto& entry : fs::directory_iterator("./data")) {
                if (entry.is_directory()) {
                    uniqueFolders.insert(entry.path().filename().string());
                }
            }
        }
    } catch (...) {}
    
    json j = json::array();
    for (const auto& folder : uniqueFolders) {
        j.push_back(folder);
    }
    res.set_content(j.dump(), "application/json");
}

void FolderHandlers::handleCreateFolder(DocumentManager&, const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        std::string folderName = body.value("name", "");
        
        if (folderName.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Folder name required"})", "application/json");
            return;
        }
        
        if (folderName.find("..") != std::string::npos || 
            folderName.find("/") != std::string::npos || 
            folderName.find("\\") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid folder name"})", "application/json");
            return;
        }
        
        std::string folderPath = "./data/" + folderName;
        
        if (fs::exists(folderPath)) {
            res.status = 409;
            res.set_content(R"({"error":"Folder exists"})", "application/json");
            return;
        }
        
        fs::create_directories(folderPath);
        
        res.set_content(json({
            {"success", true},
            {"message", "Folder created"},
            {"folder", folderName}
        }).dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({
            {"error", "Failed to create folder"},
            {"details", e.what()}
        }).dump(), "application/json");
    }
}