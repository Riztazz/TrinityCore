#include "EpochLaunchlog.hpp"
#include "TSGlobal.h"
#include "Player.h"

#include <fmt/format.h>

#include <vector>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <cstring>

static std::unordered_map<std::string, std::vector<char>> logEntries;
static std::mutex logMutex;

void LogEpochLaunchEntry(std::string const& name, char const* data, size_t size) {
    std::lock_guard lock(logMutex);
    auto& log = logEntries[name];
    size_t oldSize = log.size();
    log.resize(log.size() + size + sizeof(size_t));
    std::memcpy(log.data() + oldSize, data, size);
    size_t curTime = GetUnixTime();
    std::memcpy(log.data() + oldSize + size, &curTime, sizeof(size_t));
}

EpochLaunchPlayerData GetEpochLaunchPlayerData(Player* player) {
    return EpochLaunchPlayerData{.guid{player->GetGUID().GetRawValue()},
                                 .x{player->GetPosition().GetPositionX()},
                                 .y{player->GetPosition().GetPositionY()},
                                 .z{player->GetPosition().GetPositionZ()},
                                 .mapId{player->GetMapId()}};
}

void WriteEpochLaunchLog() {
    std::unordered_map<std::string, std::vector<char>> entries;
    {
        std::lock_guard lock(logMutex);
        entries = std::move(logEntries);
    }

    for (auto const& [key, value] : entries)
    {
        std::ofstream file(fmt::format("epoch_launch_log_{}", key), std::ios::binary | std::ios::app);
        file.write(value.data(), value.size());
    }
}
