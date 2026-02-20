#include "app_handlers.h"
#include "message_router.h"
#include "message_handler.h"
#include "platform_impl.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <sqlite3.h>
#include <unordered_map>

struct BookInfo {
    std::string title;
    std::string category;
    std::string author;
};

static std::unordered_map<int, BookInfo> loadBookInfo(const std::string& mainDbPath) {
    std::unordered_map<int, BookInfo> result;
    sqlite3* db = nullptr;
    if (sqlite3_open(mainDbPath.c_str(), &db) != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
        }
        return result;
    }

    std::unordered_map<int, std::string> authors;
    sqlite3* dbSpecial = nullptr;
    std::string specialPath;
    try {
        namespace fs = std::filesystem;
        fs::path p(mainDbPath);
        fs::path s = p;
        s.replace_filename("special.sqlite");
        specialPath = s.string();
    } catch (...) {
    }
    if (!specialPath.empty()) {
        if (sqlite3_open(specialPath.c_str(), &dbSpecial) == SQLITE_OK) {
            const char* sqlAuth = "SELECT authid, auth FROM Auth";
            sqlite3_stmt* stmtAuth = nullptr;
            if (sqlite3_prepare_v2(dbSpecial, sqlAuth, -1, &stmtAuth, nullptr) == SQLITE_OK) {
                while (true) {
                    int rc = sqlite3_step(stmtAuth);
                    if (rc == SQLITE_ROW) {
                        int id = sqlite3_column_int(stmtAuth, 0);
                        const unsigned char* name = sqlite3_column_text(stmtAuth, 1);
                        if (name) {
                            authors[id] = reinterpret_cast<const char*>(name);
                        }
                    } else if (rc == SQLITE_DONE) {
                        break;
                    } else {
                        break;
                    }
                }
                sqlite3_finalize(stmtAuth);
            }
            sqlite3_close(dbSpecial);
        } else if (dbSpecial) {
            sqlite3_close(dbSpecial);
        }
    }

    const char* sql = "SELECT b.bkid, b.bk, b.authno, b.auth, c.name FROM \"0bok\" b LEFT JOIN \"0cat\" c ON b.cat = c.id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return result;
    }
    while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int bkid = sqlite3_column_int(stmt, 0);
            const unsigned char* bk = sqlite3_column_text(stmt, 1);
            int authId = sqlite3_column_int(stmt, 2);
            const unsigned char* bkAuth = sqlite3_column_text(stmt, 3);
            const unsigned char* cat = sqlite3_column_text(stmt, 4);
            BookInfo info;
            if (bk) {
                info.title = reinterpret_cast<const char*>(bk);
            }
            if (cat) {
                info.category = reinterpret_cast<const char*>(cat);
            }
            auto itAuth = authors.find(authId);
            if (itAuth != authors.end()) {
                info.author = itAuth->second;
            } else if (bkAuth) {
                info.author = reinterpret_cast<const char*>(bkAuth);
            }
            result[bkid] = info;
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            break;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

static bool parseBkidFromStem(const std::string& stem, int& outBkid) {
    if (stem.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        int value = std::stoi(stem, &idx);
        if (idx != stem.size()) {
            return false;
        }
        outBkid = value;
        return true;
    } catch (...) {
        return false;
    }
}

class MdbSelectFolderHandler : public MessageHandler {
public:
    bool canHandle(const std::string& messageType) const override {
        return messageType == "mdbSelectFolder";
    }

    nlohmann::json handle(const nlohmann::json& payload, const std::string& requestId) override {
        (void)payload;
        (void)requestId;
        nlohmann::json result;
        std::string folderPath;
        std::string title = "Select folder containing MDB files";
        std::string filter = "DIRECTORY";
        bool selected = platform::showOpenFileDialog(nullptr, title, filter, folderPath);
        if (!selected || folderPath.empty()) {
            result["success"] = false;
            result["error"] = "No folder selected";
            return result;
        }
        namespace fs = std::filesystem;
        nlohmann::json files = nlohmann::json::array();
        std::unordered_map<int, BookInfo> bookInfo;
        std::string usedMainPath;
        try {
            fs::path current = fs::path(folderPath);
            for (int depth = 0; depth < 5 && !current.empty(); ++depth) {
                fs::path candidate = current / "main.sqlite";
                if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
                    auto tmp = loadBookInfo(candidate.string());
                    if (!tmp.empty()) {
                        usedMainPath = candidate.string();
                        bookInfo = std::move(tmp);
                        break;
                    }
                }
                current = current.parent_path();
            }
        } catch (...) {
        }
        if (bookInfo.empty()) {
            usedMainPath = "main.sqlite";
            bookInfo = loadBookInfo("main.sqlite");
        }
        result["debug_mainPath"] = usedMainPath;
        result["debug_bookCount"] = static_cast<int>(bookInfo.size());
        result["folder"] = folderPath;
        try {
            for (auto it = fs::recursive_directory_iterator(folderPath);
                 it != fs::recursive_directory_iterator();
                 ++it) {
                if (!it->is_regular_file()) {
                    continue;
                }
                fs::path p = it->path();
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ext == ".mdb" || ext == ".accdb") {
                    nlohmann::json fileJson;
                    std::string fullPath = p.string();
                    fileJson["path"] = fullPath;
                    if (!bookInfo.empty()) {
                        int bkid = 0;
                        std::string stem = p.stem().string();
                        if (parseBkidFromStem(stem, bkid)) {
                            auto itInfo = bookInfo.find(bkid);
                            if (itInfo != bookInfo.end()) {
                                fileJson["bkid"] = bkid;
                                fileJson["bookName"] = itInfo->second.title;
                                fileJson["categoryName"] = itInfo->second.category;
                                if (!itInfo->second.author.empty()) {
                                    fileJson["authorName"] = itInfo->second.author;
                                }
                            }
                        }
                    }
                    files.push_back(fileJson);
                }
            }
        } catch (...) {
            result["success"] = false;
            result["error"] = "Failed to scan folder";
            return result;
        }

        result["success"] = true;
        result["files"] = files;
        return result;
    }

    std::vector<std::string> getSupportedTypes() const override {
        return {"mdbSelectFolder"};
    }
};

static std::string buildSqlitePath(const std::string& mdbPath, const std::string& outputDir) {
    namespace fs = std::filesystem;
    fs::path p(mdbPath);
    std::string stem = p.stem().string();
    fs::path outDir = outputDir.empty() ? p.parent_path() : fs::path(outputDir);
    return (outDir / (stem + ".sqlite")).string();
}

class MdbConvertHandler : public MessageHandler {
public:
    bool canHandle(const std::string& messageType) const override {
        return messageType == "mdbConvert";
    }

    nlohmann::json handle(const nlohmann::json& payload, const std::string& requestId) override {
        (void)requestId;
        nlohmann::json result;

        if (!payload.contains("files") || !payload["files"].is_array()) {
            result["success"] = false;
            result["error"] = "Missing files array";
            return result;
        }

        std::string outputDir;
        if (payload.contains("outputDir") && payload["outputDir"].is_string()) {
            outputDir = payload["outputDir"].get<std::string>();
        }

        nlohmann::json items = nlohmann::json::array();
        bool allOk = true;

        for (const auto& f : payload["files"]) {
            if (!f.is_string()) {
                continue;
            }

            std::string mdbPath = f.get<std::string>();
            std::string sqlitePath = buildSqlitePath(mdbPath, outputDir);

            int rc = std::system(
                (std::string("sh -lc 'mdb-schema \"") + mdbPath +
                 "\" sqlite | sqlite3 \"" + sqlitePath +
                 "\" && for t in $(mdb-tables -1 \"" + mdbPath +
                 "\"); do mdb-export -I sqlite \"" + mdbPath +
                 "\" \"$t\" | sqlite3 \"" + sqlitePath + "\"; done'")
                    .c_str());

            nlohmann::json item;
            item["mdb"] = mdbPath;
            item["sqlite"] = sqlitePath;
            item["exitCode"] = rc;
            if (rc != 0) {
                allOk = false;
            }
            items.push_back(item);
        }

        result["success"] = allOk;
        result["items"] = items;
        return result;
    }

    std::vector<std::string> getSupportedTypes() const override {
        return {"mdbConvert"};
    }
};

extern "C" void registerAppHandlers(MessageRouter* router) {
    if (!router) {
        return;
    }
    router->registerHandler(std::make_shared<MdbSelectFolderHandler>());
    router->registerHandler(std::make_shared<MdbConvertHandler>());
}
