#pragma once

#include "Define.h"

#include <array>
#include <typeinfo>
#include <chrono>

class Player;

// update whenever schema changes
constexpr size_t CurEpochLaunchLogVersion = 0;

#pragma pack(push, 1)
struct EpochLaunchPlayerData {
    uint64 guid;
    float x;
    float y;
    float z;
    uint32 mapId;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct HighPlayerRelocationDiff {
    EpochLaunchPlayerData player;
    uint8 diff;
};
#pragma pack(pop)

void LogEpochLaunchEntry(std::string const& name, char const* data, size_t size);
void WriteEpochLaunchLog();

EpochLaunchPlayerData GetEpochLaunchPlayerData(Player* player);

template <typename T>
void LogEpochLaunchEntry(T const& value) {
    LogEpochLaunchEntry(typeid(value).name(), reinterpret_cast<char const*>(&value), sizeof(T));
}

void OnSlowerThan(double time, auto f, auto fv) {
    auto start_time = std::chrono::high_resolution_clock::now();
    f();
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end_time - start_time;
    if (diff.count() >= time)
    {
        fv(diff.count());
    }
}
