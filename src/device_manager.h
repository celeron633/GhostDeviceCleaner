#pragma once

#include <string>
#include <vector>

struct DeviceInfo {
    std::wstring friendly_name;
    std::wstring hardware_id;
    std::wstring class_name;
    std::wstring instance_id;
    bool present = false;
    bool selected = false;
};

struct OperationResult {
    bool success = false;
    std::wstring message;
};

class DeviceManager {
public:
    [[nodiscard]] OperationResult enumerate(std::vector<DeviceInfo>& devices) const;
    [[nodiscard]] OperationResult get_driver_package_inf(
        const std::wstring& instance_id, std::wstring& inf_name) const;
    [[nodiscard]] OperationResult remove(const std::wstring& instance_id) const;
    [[nodiscard]] OperationResult uninstall_driver_package(const std::wstring& inf_name) const;
};
