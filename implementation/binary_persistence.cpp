#include "binary_persistence.h"
#include "persistence.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

// ========================================
// BINARY PERSISTENCE HELPER METHODS
// ========================================

void BinaryPersistence::writeInt(std::ofstream& out, int value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(int));
}

int BinaryPersistence::readInt(std::ifstream& in) {
    int value;
    in.read(reinterpret_cast<char*>(&value), sizeof(int));
    return value;
}

void BinaryPersistence::writeString(std::ofstream& out, const std::string& str) {
    int len = static_cast<int>(str.size());
    writeInt(out, len);
    out.write(str.data(), len);
}

std::string BinaryPersistence::readString(std::ifstream& in) {
    int len = readInt(in);
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}

void BinaryPersistence::writeStringVector(std::ofstream& out, const std::vector<std::string>& vec) {
    writeInt(out, static_cast<int>(vec.size()));
    for (const auto& s : vec) {
        writeString(out, s);
    }
}

std::vector<std::string> BinaryPersistence::readStringVector(std::ifstream& in) {
    int size = readInt(in);
    std::vector<std::string> vec;
    vec.reserve(size);
    for (int i = 0; i < size; ++i) {
        vec.push_back(readString(in));
    }
    return vec;
}

// ========================================
// SAVE BINARY INDEX
// ========================================

void BinaryPersistence::saveBinary(
    const Indexer& indexer,
    const std::unordered_map<int, std::string>& docIdToPath,
    const std::unordered_map<int, std::string>& docIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& docTokens,
    const std::unordered_map<int, std::string>& docMeta,
    const std::unordered_map<std::string, int>& vocabCount,
    const std::unordered_map<int, std::string>& docIdToFolder,
    int docID
) {
    auto startTime = std::chrono::steady_clock::now();
    std::cout << "Saving binary index...\n";

    std::ofstream out("index.bin.tmp", std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot create index.bin.tmp");
    }

    // Write magic number for validation
    const int MAGIC = 0x494E4458; // "INDX" in hex
    writeInt(out, MAGIC);
    
    // Write version for future compatibility
    const int VERSION = 1;
    writeInt(out, VERSION);

    // Write docID
    writeInt(out, docID);

    // Write docIdToPath
    writeInt(out, static_cast<int>(docIdToPath.size()));
    for (const auto& [id, path] : docIdToPath) {
        writeInt(out, id);
        writeString(out, path);
    }

    // Write docIdToRel
    writeInt(out, static_cast<int>(docIdToRel.size()));
    for (const auto& [id, rel] : docIdToRel) {
        writeInt(out, id);
        writeString(out, rel);
    }

    // Write docTokens
    writeInt(out, static_cast<int>(docTokens.size()));
    for (const auto& [id, tokens] : docTokens) {
        writeInt(out, id);
        writeStringVector(out, tokens);
    }

    // Write docMeta
    writeInt(out, static_cast<int>(docMeta.size()));
    for (const auto& [id, meta] : docMeta) {
        writeInt(out, id);
        writeString(out, meta);
    }

    // Write vocabCount
    writeInt(out, static_cast<int>(vocabCount.size()));
    for (const auto& [word, count] : vocabCount) {
        writeString(out, word);
        writeInt(out, count);
    }

    // Write docIdToFolder
    writeInt(out, static_cast<int>(docIdToFolder.size()));
    for (const auto& [id, folder] : docIdToFolder) {
        writeInt(out, id);
        writeString(out, folder);
    }

    // Write inverted index
    const auto& index = indexer.getIndex();
    writeInt(out, static_cast<int>(index.size()));
    for (const auto& [term, postings] : index) {
        writeString(out, term);
        writeInt(out, static_cast<int>(postings.size()));
        for (const auto& [docId, freq] : postings) {
            writeInt(out, docId);
            writeInt(out, freq);
        }
    }

    out.close();

    if (!out.good()) {
        fs::remove("index.bin.tmp");
        throw std::runtime_error("Failed to write binary index");
    }

    // Atomic rename
    std::error_code ec;
    fs::rename("index.bin.tmp", "index.bin", ec);
    if (ec) {
        throw std::runtime_error("Failed to rename index file: " + ec.message());
    }

    auto fileSize = fs::file_size("index.bin");
    auto saveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    std::cout << "Binary index saved: " << (fileSize / 1024.0 / 1024.0) 
              << " MB in " << saveTime << "ms\n";
}

// ========================================
// LOAD BINARY INDEX
// ========================================

bool BinaryPersistence::loadBinary(
    Indexer& indexer,
    std::unordered_map<int, std::string>& docIdToPath,
    std::unordered_map<int, std::string>& docIdToRel,
    std::unordered_map<int, std::vector<std::string>>& docTokens,
    std::unordered_map<int, std::string>& docMeta,
    std::unordered_map<std::string, int>& vocabCount,
    std::unordered_map<int, std::string>& docIdToFolder,
    int& docID
) {
    if (!fs::exists("index.bin")) {
        return false;
    }

    auto startTime = std::chrono::steady_clock::now();
    std::ifstream in("index.bin", std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open index.bin\n";
        return false;
    }

    try {
        // Validate magic number
        int magic = readInt(in);
        if (magic != 0x494E4458) {
            std::cerr << "Invalid binary index format\n";
            return false;
        }

        // Read version
        int version = readInt(in);
        if (version != 1) {
            std::cerr << "Unsupported index version: " << version << "\n";
            return false;
        }

        // Read docID
        docID = readInt(in);

        // Read docIdToPath
        int pathSize = readInt(in);
        docIdToPath.reserve(pathSize);
        for (int i = 0; i < pathSize; ++i) {
            int id = readInt(in);
            std::string path = readString(in);
            docIdToPath[id] = std::move(path);
        }

        // Read docIdToRel
        int relSize = readInt(in);
        docIdToRel.reserve(relSize);
        for (int i = 0; i < relSize; ++i) {
            int id = readInt(in);
            std::string rel = readString(in);
            docIdToRel[id] = std::move(rel);
        }

        // Read docTokens
        int tokensSize = readInt(in);
        docTokens.reserve(tokensSize);
        for (int i = 0; i < tokensSize; ++i) {
            int id = readInt(in);
            auto tokens = readStringVector(in);
            docTokens[id] = std::move(tokens);
        }

        // Read docMeta
        int metaSize = readInt(in);
        docMeta.reserve(metaSize);
        for (int i = 0; i < metaSize; ++i) {
            int id = readInt(in);
            std::string meta = readString(in);
            docMeta[id] = std::move(meta);
        }

        // Read vocabCount
        int vocabSize = readInt(in);
        vocabCount.reserve(vocabSize);
        for (int i = 0; i < vocabSize; ++i) {
            std::string word = readString(in);
            int count = readInt(in);
            vocabCount[std::move(word)] = count;
        }

        // Read docIdToFolder
        int folderSize = readInt(in);
        docIdToFolder.reserve(folderSize);
        for (int i = 0; i < folderSize; ++i) {
            int id = readInt(in);
            std::string folder = readString(in);
            docIdToFolder[id] = std::move(folder);
        }

        // Read inverted index
        std::unordered_map<std::string, std::unordered_map<int, int>> loadedIndex;
        int indexSize = readInt(in);
        loadedIndex.reserve(indexSize);
        
        for (int i = 0; i < indexSize; ++i) {
            std::string term = readString(in);
            int postingsSize = readInt(in);
            
            std::unordered_map<int, int> postings;
            postings.reserve(postingsSize);
            
            for (int j = 0; j < postingsSize; ++j) {
                int docId = readInt(in);
                int freq = readInt(in);
                postings[docId] = freq;
            }
            
            loadedIndex[std::move(term)] = std::move(postings);
        }

        indexer.setIndex(std::move(loadedIndex));

        auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        std::cout << "Binary index loaded: " << docID << " documents in " 
                  << loadTime << "ms (~" << (loadTime / static_cast<double>(docID)) 
                  << "ms per doc)\n";

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading binary index: " << e.what() << "\n";
        return false;
    }
}

// ========================================
// UNIFIED SAVE/LOAD INTERFACE
// Global functions that try binary first, fall back to JSON
// ========================================

void saveIndex(
    const Indexer& indexer,
    const std::unordered_map<int, std::string>& docIdToPath,
    const std::unordered_map<int, std::string>& docIdToRel,
    const std::unordered_map<int, std::vector<std::string>>& docTokens,
    const std::unordered_map<int, std::string>& docMeta,
    const std::unordered_map<std::string, int>& vocabCount,
    const std::unordered_map<int, std::string>& docIdToFolder,
    int docID
) {
    try {
        // Save as binary (fast)
        BinaryPersistence::saveBinary(indexer, docIdToPath, docIdToRel, docTokens, 
                                     docMeta, vocabCount, docIdToFolder, docID);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to save binary index: " << e.what() << "\n";
        throw;
    }
}

bool loadIndex(
    Indexer& indexer,
    std::unordered_map<int, std::string>& docIdToPath,
    std::unordered_map<int, std::string>& docIdToRel,
    std::unordered_map<int, std::vector<std::string>>& docTokens,
    std::unordered_map<int, std::string>& docMeta,
    std::unordered_map<std::string, int>& vocabCount,
    std::unordered_map<int, std::string>& docIdToFolder,
    int& docID
) {
    // Try binary first (fast)
    if (BinaryPersistence::loadBinary(indexer, docIdToPath, docIdToRel, docTokens,
                                     docMeta, vocabCount, docIdToFolder, docID)) {
        return true;
    }

    // Fallback to JSON
    std::cout << "Binary index not found, trying JSON...\n";
    return PersistenceManager::loadIndexFromFile(indexer, docIdToPath, docIdToRel, docTokens,
                                                 docMeta, vocabCount, docIdToFolder, docID);
}