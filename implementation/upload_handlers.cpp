#include "upload_handlers.h"
#include "../third_party/json.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <future>     
#include <thread>
#include <sstream>
#include <mutex>
namespace {
    std::mutex consoleMutex; 

    //  Helper function for thread-safe logging
    void safePrint(const std::string& message) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << message << std::flush;
    }
}
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
    const std::unordered_set<std::string> NEEDS_EXTRACTION = {
        ".pdf", ".png", ".jpg", ".jpeg", ".tiff", ".bmp", ".gif",
        ".docx", ".doc", ".xlsx", ".xls", ".pptx", ".ppt"
    };
    
    const std::unordered_set<std::string> DIRECT_READ = {
        ".txt", ".md", ".csv", ".json", ".log", ".xml", ".html", ".htm"
    };
    
    bool needsExtraction(const std::string& filename) {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return NEEDS_EXTRACTION.count(ext) > 0;
    }
    
    bool canDirectRead(const std::string& filename) {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return DIRECT_READ.count(ext) > 0;
    }
}

void UploadHandlers::handleUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    try {
        std::string filename, content, folder;

        if (req.is_multipart_form_data()) {
            auto file = req.get_file_value("file");
            if (!file.filename.empty()) {
                filename = req.has_param("filename") && !req.get_param_value("filename").empty() 
                    ? req.get_param_value("filename") : file.filename;
                
                if (req.has_param("folder")) folder = req.get_param_value("folder");

                if (canDirectRead(filename)) {
                    content = file.content;
                    std::cout << "Fast path: " << filename << " (direct read)\n";
                } else if (needsExtraction(filename)) {
                    std::cout << "Extraction: " << filename << "\n";
                    
                    httplib::Client cli("127.0.0.1", 5000);
                    cli.set_connection_timeout(0, 300000);
                    cli.set_read_timeout(60, 0);
                    
                    httplib::MultipartFormDataItems items;
                    items.push_back({"file", file.content, file.filename, file.content_type});

                    auto extractorResponse = cli.Post("/extract", items);
                    if (!extractorResponse) {
                        res.status = 500;
                        res.set_content(R"({"error":"Extractor unreachable","success":false})", "application/json");
                        return;
                    }

                    if (extractorResponse->status != 200) {
                        res.status = 500;
                        res.set_content(R"({"error":"Extractor failed","success":false})", "application/json");
                        return;
                    }

                    auto extractorJson = json::parse(extractorResponse->body);
                    content = extractorJson.value("text", "");
                } else {
                    content = file.content;
                }
                
                if (content.empty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"Empty content","success":false})", "application/json");
                    return;
                }
            } else {
                res.status = 400;
                res.set_content(R"({"error":"No file","success":false})", "application/json");
                return;
            }
        } else {
            auto body = json::parse(req.body);
            filename = body.value("filename", "");
            content = body.value("content", "");
            folder = body.value("folder", "");
            
            if (filename.empty() || content.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Missing data","success":false})", "application/json");
                return;
            }
        }

        int newId = docManager.uploadDocument(filename, content, folder);
        
        res.set_content(json({
            {"status", "uploaded"},
            {"success", true},
            {"id", newId},
            {"filename", filename},
            {"folder", folder},
            {"extracted_chars", content.length()}
        }).dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", "Upload failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

void UploadHandlers::handleBatchUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    if (!req.is_multipart_form_data()) {
        res.status = 400;
        res.set_content(R"({"error":"Multipart required","success":false})", "application/json");
        return;
    }

    auto totalStart = std::chrono::steady_clock::now();

    try {
        std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
        
        std::vector<json> results;
        int successCount = 0, failCount = 0;
        auto files = req.files;
        
        if (files.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"No files","success":false})", "application/json");
            return;
        }

        std::cout << "\nBATCH: " << files.size() << " files\n";
        
        //  Enable both batch mode AND silent mode
        docManager.setBatchMode(true);
        docManager.setSilentMode(true);

        std::vector<std::pair<std::string, std::string>> directFiles, extractFiles;
        
        for (const auto& [fieldName, file] : files) {
            if (canDirectRead(file.filename))
                directFiles.push_back({file.filename, file.content});
            else if (needsExtraction(file.filename))
                extractFiles.push_back({file.filename, file.content});
            else
                directFiles.push_back({file.filename, file.content});
        }
        
        std::cout << "Direct: " << directFiles.size() << " | Extract: " << extractFiles.size() << "\n";

        // Process direct files
        for (const auto& [filename, content] : directFiles) {
            try {
                if (content.empty()) {
                    failCount++;
                    results.push_back({{"filename", filename}, {"status", "error"}, {"error", "Empty"}, {"success", false}});
                    continue;
                }
                
                int newId = docManager.uploadDocument(filename, content, folder);
                successCount++;
                results.push_back({{"filename", filename}, {"status", "success"}, {"id", newId}, {"method", "direct"}, {"success", true}});
            } catch (const std::exception& e) {
                failCount++;
                results.push_back({{"filename", filename}, {"status", "error"}, {"error", e.what()}, {"success", false}});
            }
        }

        // Process extraction files
        if (!extractFiles.empty()) {
            httplib::Client cli("127.0.0.1", 5000);
            cli.set_connection_timeout(0, 300000);
            cli.set_read_timeout(180, 0);
            
            httplib::MultipartFormDataItems items;
            for (const auto& [filename, content] : extractFiles)
                items.push_back({"files", content, filename, "application/octet-stream"});

            auto extractorResponse = cli.Post("/extract/batch", items);
            
            if (!extractorResponse || extractorResponse->status != 200) {
                failCount += extractFiles.size();
                for (const auto& [filename, _] : extractFiles)
                    results.push_back({{"filename", filename}, {"status", "error"}, {"error", "Extractor failed"}, {"success", false}});
            } else {
                auto batchResults = json::parse(extractorResponse->body);
                for (const auto& result : batchResults["results"]) {
                    std::string filename = result["filename"];
                    
                    if (result.value("success", false) && !result["text"].get<std::string>().empty()) {
                        try {
                            int newId = docManager.uploadDocument(filename, result["text"], folder);
                            successCount++;
                            results.push_back({{"filename", filename}, {"status", "success"}, {"id", newId}, {"method", "extracted"}, {"success", true}});
                        } catch (const std::exception& e) {
                            failCount++;
                            results.push_back({{"filename", filename}, {"status", "error"}, {"error", e.what()}, {"success", false}});
                        }
                    } else {
                        failCount++;
                        results.push_back({{"filename", filename}, {"status", "error"}, {"error", result.value("error", "Failed")}, {"success", false}});
                    }
                }
            }
        }

        //  Disable silent mode before finalize (so we see the finalize output)
        docManager.setSilentMode(false);
        docManager.setBatchMode(false);
        docManager.finalizeBatch();
        
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - totalStart).count();
        
        std::cout << "Done: " << successCount << " success, " << failCount << " failed in " << totalTime << "ms\n\n";

        json response;
        response["total_files"] = files.size();
        response["successful"] = successCount;
        response["failed"] = failCount;
        response["results"] = results;
        response["timing"] = {{"total_ms", totalTime}};
        response["success"] = (successCount > 0);

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        docManager.setSilentMode(false);  //  Reset on error
        docManager.setBatchMode(false);
        res.status = 500;
        res.set_content(json({{"error", "Batch failed"}, {"details", e.what()}, {"success", false}}).dump(), "application/json");
    }
}

// ========================================
// COMPLETE FIX for handleFolderUpload in upload_handlers.cpp
// Handles: root files + subfolders + files in subfolders
// ========================================

void UploadHandlers::handleFolderUpload(DocumentManager& docManager, const httplib::Request& req, httplib::Response& res) {
    //  Set proper headers for long uploads
    res.set_header("Connection", "keep-alive");
    res.set_header("Keep-Alive", "timeout=600");
    
    if (!req.is_multipart_form_data()) {
        res.status = 400;
        res.set_content(R"({"error":"Multipart required","success":false})", "application/json");
        return;
    }
    
    auto totalStart = std::chrono::steady_clock::now();
    auto receiveStart = std::chrono::steady_clock::now();
    
    auto files = req.files;
    
    auto receiveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - receiveStart).count();
    
    std::cout << "\n============================================\n";
    std::cout << "📂 FOLDER UPLOAD STARTED\n";
    std::cout << "   Files received: " << files.size() << " in " << receiveTime << "ms\n";
    std::cout << "============================================\n";

    if (files.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"No files provided","success":false})", "application/json");
        return;
    }

    try {
        std::vector<json> results;
        int successCount = 0;
        int failCount = 0;
        std::unordered_set<std::string> createdFolders;

        // Enable batch mode
        docManager.setBatchMode(true);
        docManager.setSilentMode(true);

        // Separate files by type AND store with proper paths
        std::vector<std::tuple<std::string, std::string, std::string>> directFiles;
        std::vector<std::tuple<std::string, std::string, std::string>> extractFiles;

        //  : Better path extraction with detailed logging
        std::cout << "📋 Processing file paths...\n";
        int skippedFiles = 0;
        
        for (const auto& [fieldName, file] : files) {
            std::string relativePath = fieldName;
            
            // Remove "file-" prefix if present
            const std::string prefix = "file-";
            if (relativePath.compare(0, prefix.length(), prefix) == 0) {
                relativePath = relativePath.substr(prefix.length());
            }
            
            // Normalize ALL path separators FIRST
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
            
            //  Remove leading path artifacts more robustly
            bool changed = true;
            while (changed && !relativePath.empty()) {
                changed = false;
                
                // Remove leading slashes
                if (relativePath[0] == '/') {
                    relativePath = relativePath.substr(1);
                    changed = true;
                    continue;
                }
                
                // Remove "./" prefix
                if (relativePath.size() >= 2 && relativePath[0] == '.' && relativePath[1] == '/') {
                    relativePath = relativePath.substr(2);
                    changed = true;
                    continue;
                }
                
                // Remove single "."
                if (relativePath.size() == 1 && relativePath[0] == '.') {
                    relativePath = "";
                    changed = true;
                    break;
                }
            }
            
            // Remove trailing slashes
            while (!relativePath.empty() && relativePath.back() == '/') {
                relativePath.pop_back();
            }
            
            //  Validate path after cleaning
            if (relativePath.empty()) {
                std::cerr << "⚠️ Skipping empty path from field: " << fieldName << "\n";
                skippedFiles++;
                continue;
            }
            
            //  Check for invalid characters
            if (relativePath.find("..") != std::string::npos) {
                std::cerr << "⚠️ Skipping path with '..' : " << relativePath << "\n";
                skippedFiles++;
                continue;
            }
            
            // Extract folder and filename
            std::string folder = "";
            std::string filename = relativePath;
            
            size_t lastSlash = relativePath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                folder = relativePath.substr(0, lastSlash);
                filename = relativePath.substr(lastSlash + 1);
            }
            
            //  Validate filename is not empty
            if (filename.empty()) {
                std::cerr << "⚠️ Skipping empty filename from path: " << relativePath << "\n";
                skippedFiles++;
                continue;
            }
            
            // Clean up folder path (no trailing slashes)
            while (!folder.empty() && folder.back() == '/') {
                folder.pop_back();
            }
            
            //  Track folders more carefully
            if (!folder.empty()) {
                // Add the full folder path
                createdFolders.insert(folder);
                
                // Add all parent folders
                std::string parentPath = "";
                std::istringstream iss(folder);
                std::string part;
                
                while (std::getline(iss, part, '/')) {
                    if (!part.empty() && part != "." && part != "..") {
                        if (!parentPath.empty()) {
                            parentPath += "/";
                        }
                        parentPath += part;
                        createdFolders.insert(parentPath);
                    }
                }
            }
            
            //  Log the mapping for debugging
            if (files.size() <= 20) {  // Only log for small uploads
                std::cout << "   📄 " << filename 
                          << (folder.empty() ? " (root)" : " → " + folder) << "\n";
            }
            
            // Categorize by file type
            if (canDirectRead(filename)) {
                directFiles.push_back({filename, folder, file.content});
            } else if (needsExtraction(filename)) {
                extractFiles.push_back({filename, folder, file.content});
            } else {
                directFiles.push_back({filename, folder, file.content});
            }
        }
        
        if (skippedFiles > 0) {
            std::cout << "⚠️ Skipped " << skippedFiles << " invalid paths\n";
        }

        std::cout << "📄 Direct read: " << directFiles.size() << "\n";
        std::cout << "📦 Extraction needed: " << extractFiles.size() << "\n";
        std::cout << "📁 Unique folders: " << createdFolders.size() << "\n";
        std::cout << "--------------------------------------------\n";

        //  Process direct files with CONTROLLED parallelism
//  Process files in controlled batches with limited concurrency
auto directStart = std::chrono::steady_clock::now();

std::cout << " Processing " << directFiles.size() << " direct-read files...\n";

for (size_t j = 0; j < directFiles.size(); ++j) {
    const auto& [filename, folder, content] = directFiles[j];
    
    if (content.empty()) {
        failCount++;
        results.push_back({
            {"filename", filename}, {"folder", folder},
            {"status", "error"}, {"error", "Empty file"},
            {"success", false}
        });
        continue;
    }
    
    try {
        int newId = docManager.uploadDocument(filename, content, folder);
        successCount++;
        results.push_back({
            {"filename", filename}, {"folder", folder},
            {"status", "success"}, {"id", newId},
            {"method", "direct_read"}, {"success", true}
        });
    } catch (const std::exception& e) {
        failCount++;
        results.push_back({
            {"filename", filename}, {"folder", folder},
            {"status", "error"}, {"error", e.what()},
            {"success", false}
        });
    }
    
    // Progress log every 500 files
    if ((j + 1) % 500 == 0) {
        std::cout << "📊 Progress: " << (j + 1) << "/" 
                  << directFiles.size() << " files\n" << std::flush;
    }
}

auto directTime = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - directStart).count();
std::cout << " Direct-read done in " << directTime << "ms";
if (!directFiles.empty()) {
    std::cout << " (" << (directTime / static_cast<double>(directFiles.size())) << "ms/file)";
}
std::cout << "\n";

        // Process extraction files (if any)
        if (!extractFiles.empty()) {
            std::cout << "🔧 Sending " << extractFiles.size() << " files to extractor...\n";
            
            httplib::Client cli("127.0.0.1", 5000);
            cli.set_connection_timeout(0, 300000);
            cli.set_read_timeout(180, 0);

            httplib::MultipartFormDataItems items;
            for (const auto& [filename, folder, content] : extractFiles) {
                items.push_back({"files", content, filename, "application/octet-stream"});
            }

            auto extractorResponse = cli.Post("/extract/batch", items);

            if (!extractorResponse || extractorResponse->status != 200) {
                std::cerr << " Extractor service failed\n";
                failCount += extractFiles.size();
                for (const auto& [filename, folder, _] : extractFiles) {
                    results.push_back({
                        {"filename", filename}, 
                        {"folder", folder},
                        {"status", "error"}, 
                        {"error", "Extraction service failed"},
                        {"success", false}
                    });
                }
            } else {
                auto batchResults = json::parse(extractorResponse->body);
                auto extractedResults = batchResults["results"];
                std::cout << " Extractor returned " << extractedResults.size() << " results\n";

                for (const auto& result : extractedResults) {
                    std::string filename = result["filename"];
                    std::string folder = "";
                    
                    // Find folder for this file
                    for (const auto& [fname, fld, _] : extractFiles) {
                        if (fname == filename) { 
                            folder = fld; 
                            break; 
                        }
                    }

                    if (result.value("success", false)) {
                        std::string text = result["text"];
                        try {
                            int newId = docManager.uploadDocument(filename, text, folder);
                            successCount++;
                            results.push_back({
                                {"filename", filename}, 
                                {"folder", folder},
                                {"status", "success"}, 
                                {"id", newId},
                                {"method", "extracted"},
                                {"extracted_chars", text.length()},
                                {"success", true}
                            });
                        } catch (const std::exception& e) {
                            failCount++;
                            results.push_back({
                                {"filename", filename}, 
                                {"folder", folder},
                                {"status", "error"}, 
                                {"error", std::string(e.what())},
                                {"success", false}
                            });
                        }
                    } else {
                        failCount++;
                        results.push_back({
                            {"filename", filename}, 
                            {"folder", folder},
                            {"status", "error"},
                            {"error", result.value("error", "Extraction failed")},
                            {"success", false}
                        });
                    }
                }
            }
        }
        
        // Finalize batch
        auto saveStart = std::chrono::steady_clock::now();
        std::cout << "💾 Finalizing batch (building structures + saving)...\n";
        docManager.setSilentMode(false);
        docManager.setBatchMode(false);
        docManager.finalizeBatch();
        auto saveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - saveStart).count();
        std::cout << " Finalized in " << saveTime << "ms\n";

        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - totalStart).count();

        std::cout << "🎉 Folder upload complete: " << successCount << " , "
                  << failCount << " ❌ in " << totalTime << "ms\n";
        
        if (directFiles.size() + extractFiles.size() > 0) {
            std::cout << "   Average: " 
                      << (totalTime / static_cast<double>(directFiles.size() + extractFiles.size())) 
                      << "ms per file\n";
        }
        std::cout << "============================================\n\n";

        json response;
        response["total_files"] = files.size();
        response["processed_files"] = directFiles.size() + extractFiles.size();
        response["skipped_files"] = skippedFiles;
        response["successful"] = successCount;
        response["failed"] = failCount;
        response["folders_created"] = std::vector<std::string>(createdFolders.begin(), createdFolders.end());
        response["results"] = results;
        response["success"] = (successCount > 0);
        response["timing"] = {
            {"total_ms", totalTime},
            {"receive_ms", receiveTime},
            {"processing_ms", directTime},
            {"save_ms", saveTime},
            {"total_seconds", totalTime / 1000.0}
        };
        response["stats"] = {
            {"total_documents", docManager.getDocID()},
            {"indexed_documents", docManager.getDocIdToPath().size()},
            {"vocabulary_size", docManager.getVocabCount().size()}
        };

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        docManager.setBatchMode(false);
        docManager.setSilentMode(false);
        
        std::cerr << " CRITICAL ERROR in folder upload: " << e.what() << "\n";
        
        res.status = 500;
        res.set_content(json({
            {"error", "Folder upload failed"},
            {"details", e.what()},
            {"success", false}
        }).dump(), "application/json");
    }
}