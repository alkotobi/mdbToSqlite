#include "app_handlers.h"
#include "message_router.h"
#include "message_handler.h"
#include "platform_impl.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>

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
        result["folder"] = folderPath;

        namespace fs = std::filesystem;
        nlohmann::json files = nlohmann::json::array();
        try {
            for (auto it = fs::recursive_directory_iterator(folderPath);
                 it != fs::recursive_directory_iterator();
                 ++it) {
                if (!it->is_regular_file()) {
                    continue;
                }
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ext == ".mdb" || ext == ".accdb") {
                    files.push_back(it->path().string());
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

