#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>

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

// ---------------- Persistence ----------------
void saveIndexToFile(const Indexer& indexer,
                     const std::unordered_map<int, std::string>& docIdToPath,
                     const std::unordered_map<int, std::string>& docIdToRel,
                     const std::unordered_map<int, std::vector<std::string>>& docTokens,
                     int docID) {
    json j;
    j["docID"] = docID;
    j["docIdToPath"] = docIdToPath;
    j["docIdToRel"]  = docIdToRel;
    j["index"]       = indexer.getIndex();
    j["docTokens"]   = docTokens;

    // compact dump (faster, smaller)
    std::ofstream out("index.json.tmp", std::ios::trunc | std::ios::binary);
    std::string jsonStr = j.dump();  // No pretty printing for speed
    out.write(jsonStr.c_str(), jsonStr.size());
    out.close();
    fs::rename("index.json.tmp", "index.json");
}

bool loadIndexFromFile(Indexer& indexer,
                       std::unordered_map<int, std::string>& docIdToPath,
                       std::unordered_map<int, std::string>& docIdToRel,
                       std::unordered_map<int, std::vector<std::string>>& docTokens,
                       int& docID) {
    if (!fs::exists("index.json")) return false;
    std::ifstream in("index.json");
    if (!in.is_open()) return false;
    json j; in >> j;

    try {
        docID       = j["docID"].get<int>();
        docIdToPath = j["docIdToPath"].get<std::unordered_map<int,std::string>>();
        docIdToRel  = j["docIdToRel"].get<std::unordered_map<int,std::string>>();
        docTokens   = j["docTokens"].get<std::unordered_map<int,std::vector<std::string>>>();
        indexer.setIndex(j["index"].get<std::unordered_map<std::string,std::unordered_map<int,int>>>());
    } catch (...) {
        return false;
    }
    return true;
}

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";

    // in-memory structures
    std::unordered_map<int, std::vector<std::string>> docTokens;
    std::unordered_map<int, std::string> docIdToContent; //  cache for file contents
    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unordered_map<int, std::string> docIdToPath;
    std::unordered_map<int, std::string> docIdToRel;
    int docID = 0;

    std::unique_ptr<SearchEngine> engine;

    // ---------------- Load or Index ----------------
    bool freshBuild = false;
    if (!loadIndexFromFile(indexer, docIdToPath, docIdToRel, docTokens, docID)) {
        std::cout << "No index.json found, indexing ./data …\n";
        freshBuild = true;
    } else {
        std::unordered_set<std::string> actualFiles;
        if (fs::exists("./data")) {
            for (auto& entry : fs::directory_iterator("./data")) {
                if (entry.is_regular_file()) actualFiles.insert(entry.path().string());
            }
        }
        std::unordered_set<std::string> indexedFiles;
        for (auto& [id, path] : docIdToPath) indexedFiles.insert(path);

        if (actualFiles != indexedFiles) {
            std::cout << "Data folder changed (files added/removed). Will rebuild from ./data.\n";
            freshBuild = true;
        }
    }

    
// 1. OPTIMIZE INITIAL INDEXING - Add progress and batch processing
if (freshBuild) {
    indexer.clear();
    autoComplete.clear();
    typoCorrector.clear();
    docTokens.clear();
    docIdToPath.clear();
    docIdToRel.clear();
    docIdToContent.clear();
    docID = 0;

    if (!fs::exists("./data")) fs::create_directory("./data");

    // Count files first for progress
    std::vector<fs::directory_entry> files;
    for (const auto& entry : fs::directory_iterator("./data")) {
        if (entry.is_regular_file()) files.push_back(entry);
    }
    
    std::cout << "Indexing " << files.size() << " files from ./data folder...\n";
    
    // Process with progress indicator
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& entry = files[i];
        std::string path = entry.path().string();
        std::string content = Parser::readFile(path);
        if (content.empty()) continue;

        auto tokens = Parser::tokenize(content);
        docTokens[docID] = std::move(tokens); // Use move semantics
        docIdToContent[docID] = std::move(content);
        indexer.indexDocumentFromTokens(docID, docTokens[docID]);
        
        // Batch insert for vocabulary (more efficient)
        for (const auto& w : docTokens[docID]) {
            if (!w.empty()) { 
                autoComplete.insert(w); 
                typoCorrector.insert(w); 
            }
        }
        
        docIdToPath[docID] = path;
        docIdToRel[docID] = entry.path().filename().string();
        ++docID;
        
        // Progress every 200 files ~1000 words
        if ((i + 1) % 200 == 0 || i == files.size() - 1) {
            std::cout << "Progress: " << (i + 1) << "/" << files.size() << " files processed\n";
        }
    }
    
    std::cout << "Indexed " << docID << " files from ./data folder.\n";
    std::cout << "Saving index to disk...\n";
    saveIndexToFile(indexer, docIdToPath, docIdToRel, docTokens, docID);

    
} else {
    std::cout << "Loaded " << docIdToPath.size() << " files from index.json.\n";
    
    std::cout << "Rebuilding autocomplete + corrections from saved tokens...\n";
    autoComplete.clear();
    typoCorrector.clear();
    docIdToContent.clear();

    for (auto& [id, path] : docIdToPath) {
        docIdToContent[id] = Parser::readFile(path);
    }
    std::unordered_set<std::string> vocab;
    for (auto& [id, tokens] : docTokens) {
        for (const auto& t : tokens) if (!t.empty()) vocab.insert(t);
    }
    for (const auto& w : vocab) {
        autoComplete.insert(w);
        typoCorrector.insert(w);
    }
}

    engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);

    auto persistAfterChange = [&]() {
        saveIndexToFile(indexer, docIdToPath, docIdToRel, docTokens, docID);
    };

    httplib::Server svr;

    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Options(".*", [](const httplib::Request&, httplib::Response&) { return; });
    svr.set_mount_point("/", "./public");

    // ---------------- Health ----------------
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ---------------- Suggestions ----------------
    svr.Get("/suggest", [&](const httplib::Request& req, httplib::Response& res) {
        auto prefix = req.get_param_value("prefix");
        auto suggestions = prefix.empty() ? std::vector<std::string>() : autoComplete.suggest(prefix);
        res.set_content(json(suggestions).dump(), "application/json");
    });

    // ---------------- Correct ----------------
    svr.Get("/correct", [&](const httplib::Request& req, httplib::Response& res) {
        auto word = req.get_param_value("word");
        int maxResults = req.has_param("max") ? std::stoi(req.get_param_value("max")) : 3;
        auto corrections = word.empty() ? std::vector<std::string>() : typoCorrector.search(word, 2);
        if ((int)corrections.size() > maxResults) corrections.resize(maxResults);
        res.set_content(json(corrections).dump(), "application/json");
    });

    // ---------------- Search ----------------
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        auto query = req.get_param_value("query");
        if (query.empty()) { res.set_content("[]","application/json"); return; }
        try {
            Ranker ranker;
            auto results = engine->search(query, ranker);
            json j = json::array();
            for (const auto& result : results) {
                std::string content = docIdToContent[result.first];
                auto pos = content.find(query);
                std::string preview;
                if (pos != std::string::npos) {
                    size_t start = (pos > 50) ? pos - 50 : 0;
                    preview = content.substr(start, 200);
                } else {
                    preview = content.substr(0, std::min<size_t>(200, content.size()));
                }
                j.push_back({
                    {"id", result.first},
                    {"score", result.second},
                    {"path", docIdToPath[result.first]},
                    {"url", docIdToRel[result.first]},
                    {"preview", preview}
                });
            }
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // ---------------- Document ----------------
    svr.Get("/document", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("id")) { res.status = 400; return; }
        int id = std::stoi(req.get_param_value("id"));
        if (!docIdToPath.count(id)) { res.status = 404; return; }
        res.set_content(json({
            {"id", id},
            {"path", docIdToPath[id]},
            {"url", docIdToRel[id]},
            {"content", docIdToContent[id]}
        }).dump(), "application/json");
    });

    // ---------------- Upload ----------------
    svr.Post("/upload", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string filename = body["filename"], content = body["content"];
            std::string path = "./data/" + filename;
            std::ofstream(path) << content;

            int newId = docID++;
            docIdToPath[newId] = path;
            docIdToRel[newId]  = filename;
            docIdToContent[newId] = content; // 🚀 cache

            auto tokens = Parser::tokenize(content);
            docTokens[newId] = tokens;
            indexer.indexDocumentFromTokens(newId, tokens);
            for (const auto& w : tokens) {
                if (!w.empty()) { autoComplete.insert(w); typoCorrector.insert(w); }
            }

            engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
            persistAfterChange();

            res.set_content(json({{"status","uploaded"},{"id",newId},{"filename",filename}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", std::string("upload failed: ") + e.what()}}).dump(), "application/json");
        }
    });

    // 2. OPTIMIZE EDIT OPERATION - Don't recreate SearchEngine, just update it
svr.Put(R"(/edit/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        if (!docIdToPath.count(id)) { 
            res.status = 404; 
            res.set_content(R"({"error":"document not found"})","application/json"); 
            return; 
        }
        auto body = json::parse(req.body);
        std::string newContent = body["content"];
        
        // Write file
        std::ofstream(docIdToPath[id]) << newContent;

        // Remove old document efficiently
        if (docTokens.count(id)) {
            indexer.removeDocument(id, docTokens[id]);
            // Don't remove from autocomplete/typo - other docs might use same words
        }
        
        // Add new document
        auto tokens = Parser::tokenize(newContent);
        docTokens[id] = std::move(tokens);
        docIdToContent[id] = std::move(newContent);
        indexer.indexDocumentFromTokens(id, docTokens[id]);
        
        // Add new words to vocabulary
        for (const auto& w : docTokens[id]) {
            if (!w.empty()) { 
                autoComplete.insert(w); 
                typoCorrector.insert(w); 
            }
        }
        engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
        
        // Save asynchronously (don't block response)
        std::thread([&]() {
            saveIndexToFile(indexer, docIdToPath, docIdToRel, docTokens, docID);
        }).detach();

        res.set_content(json({{"status","updated"},{"id",id}}).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(json({{"error", std::string("edit failed: ") + e.what()}}).dump(), "application/json");
    }
});

    // ---------------- Delete ----------------
    svr.Delete(R"(/delete/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            if (!docIdToPath.count(id)) { 
                res.status = 404; 
                res.set_content(R"({"error":"document not found"})","application/json"); 
                return; 
            }
            if (fs::exists(docIdToPath[id])) fs::remove(docIdToPath[id]);
            docIdToPath.erase(id);
            docIdToRel.erase(id);
            docIdToContent.erase(id); //  clear cache

            if (docTokens.count(id)) {
                indexer.removeDocument(id, docTokens[id]);
                for (const auto& w : docTokens[id]) {
                    if (!w.empty()) { autoComplete.remove(w); typoCorrector.markDeleted(w); }
                }
                docTokens.erase(id);
            }
            engine = std::make_unique<SearchEngine>(indexer.getIndex(), docID);
            persistAfterChange();

            res.set_content(json({{"status","deleted"},{"id",id}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", std::string("delete failed: ") + e.what()}}).dump(), "application/json");
        }
    });

    // ---------------- List ----------------
    svr.Get("/documents", [&](const httplib::Request&, httplib::Response& res) {
        json j = json::array();
        std::vector<int> ids;
        for (auto& [id, _] : docIdToRel) ids.push_back(id);
        std::sort(ids.begin(), ids.end());

        for (int id : ids) {
            j.push_back({
                {"id", id},
                {"url", docIdToRel[id]},
                {"content", docIdToContent[id].substr(0,200)} //  from cache
            });
        }
        res.set_content(j.dump(),"application/json");
    });

    std::cout << "Server running at http://localhost:8080\n";
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server on port 8080\n";
        return 1;
    }
    return 0;
}
