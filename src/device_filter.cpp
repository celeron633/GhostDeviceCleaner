#include "device_filter.h"

#include <algorithm>
#include <cwctype>

namespace {

std::wstring lower_copy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return value;
}

bool contains_case_insensitive(const std::wstring& value, const std::wstring& query) {
    return lower_copy(value).find(lower_copy(query)) != std::wstring::npos;
}

} // namespace

bool device_matches(const DeviceInfo& device, const DeviceFilter& filter) {
    if (filter.ghosts_only && device.present) {
        return false;
    }
    if (!filter.class_name.empty() && _wcsicmp(device.class_name.c_str(), filter.class_name.c_str()) != 0) {
        return false;
    }
    if (filter.query.empty()) {
        return true;
    }
    return contains_case_insensitive(device.friendly_name, filter.query)
        || contains_case_insensitive(device.hardware_id, filter.query)
        || contains_case_insensitive(device.instance_id, filter.query);
}

std::vector<std::wstring> collect_device_classes(const std::vector<DeviceInfo>& devices) {
    std::vector<std::wstring> classes;
    classes.reserve(devices.size());
    for (const auto& device : devices) {
        if (std::ranges::find(classes, device.class_name) == classes.end()) {
            classes.push_back(device.class_name);
        }
    }
    std::ranges::sort(classes, [](const auto& lhs, const auto& rhs) {
        return _wcsicmp(lhs.c_str(), rhs.c_str()) < 0;
    });
    return classes;
}

