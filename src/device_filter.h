#pragma once

#include "device_manager.h"

#include <string>
#include <vector>

struct DeviceFilter {
    std::wstring query;
    std::wstring class_name;
    bool ghosts_only = true;
};

[[nodiscard]] bool device_matches(const DeviceInfo& device, const DeviceFilter& filter);
[[nodiscard]] std::vector<std::wstring> collect_device_classes(const std::vector<DeviceInfo>& devices);

