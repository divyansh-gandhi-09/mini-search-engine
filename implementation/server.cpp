#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "../third_party/httplib.h"
#include "../third_party/json.hpp"

#include "parser.h"
#include "indexer.h"
#include "search.h"
#include "ranker.h"
#include "trie.h"
#include "bk_tree.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper function to highlight search terms in text
std::string highlightText(const std::string& text, const std::string& query) {
    if (query.empty()) return text;
    
    std::vector<std::string> reserved = {"and", "or"};
    std::istringstream iss(query);
    std::string term;
    std::vector<std::string> terms;
    
    while (iss >> term) {
        std::transform(term.begin(), term.end(), term.begin(), ::tolower);
        if (std::find(reserved.begin(), reserved.end(), term) == reserved.end()) {
            terms.push_back(term);
        }
    }
    
    std::string result = text;
    for (const auto& t : terms) {
        size_t pos = 0;
        while ((pos = result.find(t, pos)) != std::string::npos) {
            result.replace(pos, t.length(), "<b><u>" + t + "</u></b>");
            pos += t.length() + 7; // length of tags
        }
    }
    
    return result;
}

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";
    std::cout << "Indexing documents from './data/'...\n";

    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unordered_map<int, std::string> docIdToPath;
    std::unordered_map<int, std::string> docIdToRel;
    int docID = 0;

    // ---------------- Helper: index or reindex ----------------
    auto indexSingleDocument = [&](int id, const std::string& path, const std::string& content) {
        indexer.indexDocument(id, content);

        auto tokens = Parser::tokenize(content);
        for (const auto& w : tokens) {
            autoComplete.insert(w);
            typoCorrector.insert(w);
        }
        docIdToPath[id] = path;
        docIdToRel[id]  = fs::path(path).filename().string();
    };

    // ---------------- Initial Indexing ----------------
    // Create data directory if it doesn't exist
    if (!fs::exists("./data")) {
        fs::create_directory("./data");
        std::cout << "Created ./data directory\n";
    }

    for (const auto& entry : fs::directory_iterator("./data")) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        std::string content = Parser::readFile(path);

        if (!content.empty()) {
            indexSingleDocument(docID, path, content);
            ++docID;
        }
    }

    auto index = indexer.getIndex();
    SearchEngine engine(index, docID);

    std::cout << "Indexing complete. " << docID << " documents indexed.\n";

    httplib::Server svr;

    // Enable CORS for all routes
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Handle preflight requests
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        return;
    });

    // Serve static files
    svr.set_mount_point("/", "./public");

    // ---------------- Health Check ----------------
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ---------------- Auto-complete suggestions ----------------
    svr.Get("/suggest", [&](const httplib::Request& req, httplib::Response& res) {
        auto prefix = req.get_param_value("prefix");
        if (prefix.empty()) {
            res.set_content("[]", "application/json");
            return;
        }

        auto suggestions = autoComplete.suggest(prefix); // Remove second parameter
        json j = suggestions;
        res.set_content(j.dump(), "application/json");
    });

    // ---------------- Typo correction ----------------
    svr.Get("/correct", [&](const httplib::Request& req, httplib::Response& res) {
        auto word = req.get_param_value("word");
        auto maxStr = req.get_param_value("max");
        int maxResults = maxStr.empty() ? 3 : std::stoi(maxStr);

        if (word.empty()) {
            res.set_content("[]", "application/json");
            return;
        }

        auto corrections = typoCorrector.search(word, 2); // Use correct method name and parameters
        
        // Limit results if needed
        if (corrections.size() > maxResults) {
            corrections.resize(maxResults);
        }
        
        json j = corrections;
        res.set_content(j.dump(), "application/json");
    });

    // ---------------- Search ----------------
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        auto query = req.get_param_value("query");
        if (query.empty()) {
            res.set_content("[]", "application/json");
            return;
        }

        try {
            Ranker ranker; // Create ranker instance
            auto results = engine.search(query, ranker); // Pass ranker as second parameter
            json j = json::array();

            for (const auto& result : results) {
                std::string content = Parser::readFile(docIdToPath[result.first]); // result.first is docId
                std::string preview = content.length() > 200 ? 
                    content.substr(0, 200) + "..." : content;

                json resultObj = {
                    {"id", result.first},
                    {"score", result.second},
                    {"path", docIdToPath[result.first]},
                    {"url", docIdToRel[result.first]},
                    {"preview", preview}
                };
                j.push_back(resultObj);
            }

            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // ---------------- Get full document ----------------
    svr.Get("/document", [&](const httplib::Request& req, httplib::Response& res) {
        auto idStr = req.get_param_value("id");
        if (idStr.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"missing id parameter"})", "application/json");
            return;
        }

        try {
            int id = std::stoi(idStr);
            if (docIdToPath.find(id) == docIdToPath.end()) {
                res.status = 404;
                res.set_content(R"({"error":"document not found"})", "application/json");
                return;
            }

            std::string content = Parser::readFile(docIdToPath[id]);
            json response = {
                {"id", id},
                {"path", docIdToPath[id]},
                {"url", docIdToRel[id]},
                {"content", content}
            };

            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // ---------------- Upload new file ----------------
    svr.Post("/upload", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            if (!body.contains("filename") || !body.contains("content")) {
                res.status = 400;
                res.set_content(R"({"error":"missing filename or content"})", "application/json");
                return;
            }

            std::string filename = body["filename"];
            std::string content = body["content"];

            // Validate filename
            if (filename.empty() || content.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"filename and content cannot be empty"})", "application/json");
                return;
            }

            // Ensure data directory exists
            if (!fs::exists("./data")) {
                fs::create_directory("./data");
            }

            std::string path = "./data/" + filename;
            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                res.status = 500;
                res.set_content(R"({"error":"failed to create file"})", "application/json");
                return;
            }

            ofs << content;
            ofs.close();

            int newId = docID++;
            indexSingleDocument(newId, path, content);

            json response = {
                {"status", "uploaded"},
                {"id", newId},
                {"filename", filename}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json error = {{"error", std::string("upload failed: ") + e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // ---------------- Edit existing file ----------------
    svr.Put(R"(/edit/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            if (docIdToPath.find(id) == docIdToPath.end()) {
                res.status = 404;
                res.set_content(R"({"error":"document not found"})", "application/json");
                return;
            }

            auto body = json::parse(req.body);
            if (!body.contains("content")) {
                res.status = 400;
                res.set_content(R"({"error":"missing content"})", "application/json");
                return;
            }

            std::string newContent = body["content"];
            
            if (newContent.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"content cannot be empty"})", "application/json");
                return;
            }

            // Overwrite file
            std::ofstream ofs(docIdToPath[id]);
            if (!ofs.is_open()) {
                res.status = 500;
                res.set_content(R"({"error":"failed to write file"})", "application/json");
                return;
            }

            ofs << newContent;
            ofs.close();

            // Re-index the document
            indexSingleDocument(id, docIdToPath[id], newContent);

            json response = {
                {"status", "updated"},
                {"id", id}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json error = {{"error", std::string("edit failed: ") + e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });

    std::cout << "Server running at http://localhost:8080\n";
    std::cout << "Access the web interface at: http://localhost:8080/index.html\n";
    
    // Start the server
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server on port 8080\n";
        return 1;
    }

    return 0;
}