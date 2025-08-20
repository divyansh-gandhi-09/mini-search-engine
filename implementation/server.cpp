#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

#include "../third_party/httplib.h"      // cpp-httplib single header
#include "../third_party/json.hpp"       // nlohmann/json single header

#include "parser.h"
#include "indexer.h"
#include "search.h"
#include "ranker.h"
#include "trie.h"
#include "bk_tree.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

int main() {
    std::cout << "\n------ Mini Search Engine (Web) ------\n";
    std::cout << "Indexing documents from './data/'...\n";

    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unordered_map<int, std::string> docIdToPath;
    std::unordered_map<int, std::string> docIdToRel;
    int docID = 0;

    for (const auto& entry : fs::directory_iterator("./data")) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        std::string content = Parser::readFile(path);

        indexer.indexDocument(docID, content);

        auto tokens = Parser::tokenize(content);
        for (const auto& w : tokens) {
            autoComplete.insert(w);
            typoCorrector.insert(w);
        }

        docIdToPath[docID] = path;
        docIdToRel[docID]  = entry.path().filename().string();
        ++docID;
    }

    auto index = indexer.getIndex();
    SearchEngine engine(index, docID);

    std::cout << "Indexing complete. " << docID << " documents indexed.\n";

    httplib::Server svr;

    svr.set_mount_point("/", "./public");
    svr.set_mount_point("/data", "./data");
    svr.set_file_extension_and_mimetype_mapping("js", "text/javascript");
    svr.set_file_extension_and_mimetype_mapping("css", "text/css");
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Access-Control-Allow-Methods", "GET, OPTIONS"}
    });
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res){
        res.status = 200;
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res){
        res.set_content("OK", "text/plain");
    });

    svr.Get("/suggest", [&](const httplib::Request& req, httplib::Response& res){
        if (!req.has_param("prefix")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'prefix'"})", "application/json");
            return;
        }
        std::string prefix = req.get_param_value("prefix");
        auto list = autoComplete.suggest(prefix);
        if (list.size() > 8) list.resize(8);
        json j = list;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/correct", [&](const httplib::Request& req, httplib::Response& res){
        if (!req.has_param("word")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'word'"})", "application/json");
            return;
        }
        std::string word = req.get_param_value("word");
        int maxd = 2;
        if (req.has_param("max")) {
            try { maxd = std::stoi(req.get_param_value("max")); } catch (...) {}
        }
        auto list = typoCorrector.search(word, maxd);
        if (list.size() > 5) list.resize(5);
        json j = list;
        res.set_content(j.dump(), "application/json");
    });

    // 🔹 Search with preview
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res){
        if (!req.has_param("query")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'query'"})", "application/json");
            return;
        }
        std::string q = req.get_param_value("query");
        auto ranked = engine.search(q, Ranker());

        json arr = json::array();
        for (auto& [id, score] : ranked) {
            json item;
            item["id"]    = id;
            item["path"]  = docIdToPath[id];
            item["url"]   = std::string("/data/") + docIdToRel[id];
            item["score"] = score;

            // Preview snippet (first 200 chars)
            std::string content = Parser::readFile(docIdToPath[id]);
            if (content.size() > 200) content = content.substr(0, 200) + "...";
            item["preview"] = content;

            arr.push_back(item);
        }
        res.set_content(arr.dump(), "application/json");
    });

    // 🔹 Full document fetch
    svr.Get("/document", [&](const httplib::Request& req, httplib::Response& res){
        if (!req.has_param("id")) {
            res.status = 400;
            res.set_content(R"({"error":"missing 'id'"})", "application/json");
            return;
        }
        int id = std::stoi(req.get_param_value("id"));
        if (docIdToPath.find(id) == docIdToPath.end()) {
            res.status = 404;
            res.set_content(R"({"error":"document not found"})", "application/json");
            return;
        }

        std::string content = Parser::readFile(docIdToPath[id]);
        json j;
        j["id"] = id;
        j["path"] = docIdToPath[id];
        j["url"] = std::string("/data/") + docIdToRel[id];
        j["content"] = content;

        res.set_content(j.dump(), "application/json");
    });

    std::cout << "Server running at http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
