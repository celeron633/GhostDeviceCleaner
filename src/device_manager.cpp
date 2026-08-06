#include "device_manager.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <devpkey.h>

#include <algorithm>
#include <cwctype>
#include <unordered_set>
#include <vector>

namespace {

struct DeviceInfoSet {
    HDEVINFO value = INVALID_HANDLE_VALUE;

    ~DeviceInfoSet() {
        if (value != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(value);
        }
    }

    DeviceInfoSet(const DeviceInfoSet&) = delete;
    DeviceInfoSet& operator=(const DeviceInfoSet&) = delete;
    DeviceInfoSet() = default;
};

std::wstring format_windows_error(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring message = length && buffer ? std::wstring(buffer, length) : L"Unknown error";
    if (buffer) {
        LocalFree(buffer);
    }
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return message;
}

std::wstring get_instance_id(HDEVINFO set, SP_DEVINFO_DATA& data) {
    DWORD required = 0;
    SetupDiGetDeviceInstanceIdW(set, &data, nullptr, 0, &required);
    if (required == 0) {
        return {};
    }

    std::vector<wchar_t> buffer(required);
    if (!SetupDiGetDeviceInstanceIdW(set, &data, buffer.data(), required, nullptr)) {
        return {};
    }
    return buffer.data();
}

std::wstring get_string_property(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property) {
    DWORD type = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, nullptr, 0, &required);
    if (required == 0) {
        return {};
    }

    std::vector<BYTE> buffer(required + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(
            set, &data, property, &type, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr)) {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(buffer.data());
}

std::wstring get_device_property(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& property) {
    DEVPROPTYPE type = 0;
    DWORD required = 0;
    SetupDiGetDevicePropertyW(set, &data, &property, &type, nullptr, 0, &required, 0);
    if (required == 0) {
        return {};
    }

    std::vector<BYTE> buffer(required + sizeof(wchar_t), 0);
    if (!SetupDiGetDevicePropertyW(set, &data, &property, &type, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr, 0)) {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(buffer.data());
}

OperationResult open_device(
    const std::wstring& instance_id, DeviceInfoSet& set, SP_DEVINFO_DATA& data) {
    set.value = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set.value == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return {false, L"Unable to open the device list: " + format_windows_error(error)};
    }

    data.cbSize = sizeof(data);
    if (!SetupDiOpenDeviceInfoW(set.value, instance_id.c_str(), nullptr, 0, &data)) {
        const DWORD error = GetLastError();
        return {false, L"Device no longer exists: " + format_windows_error(error)};
    }
    return {true, {}};
}

std::wstring upper_copy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towupper(c));
    });
    return value;
}

OperationResult collect_present_ids(std::unordered_set<std::wstring>& ids) {
    DeviceInfoSet set;
    set.value = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set.value == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return {false, L"Unable to enumerate connected devices: " + format_windows_error(error)};
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(set.value, index, &data)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) {
                break;
            }
            return {false, L"Connected-device enumeration failed: " + format_windows_error(error)};
        }

        auto id = get_instance_id(set.value, data);
        if (!id.empty()) {
            ids.insert(upper_copy(std::move(id)));
        }
    }
    return {true, {}};
}

} // namespace

OperationResult DeviceManager::enumerate(std::vector<DeviceInfo>& devices) const {
    devices.clear();

    std::unordered_set<std::wstring> present_ids;
    if (auto result = collect_present_ids(present_ids); !result.success) {
        return result;
    }

    DeviceInfoSet set;
    set.value = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set.value == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return {false, L"Unable to enumerate devices: " + format_windows_error(error)};
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(set.value, index, &data)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) {
                break;
            }
            return {false, L"Device enumeration failed: " + format_windows_error(error)};
        }

        DeviceInfo device;
        device.instance_id = get_instance_id(set.value, data);
        if (device.instance_id.empty()) {
            continue;
        }
        device.friendly_name = get_string_property(set.value, data, SPDRP_FRIENDLYNAME);
        if (device.friendly_name.empty()) {
            device.friendly_name = get_string_property(set.value, data, SPDRP_DEVICEDESC);
        }
        if (device.friendly_name.empty()) {
            device.friendly_name = L"Unnamed device";
        }
        device.hardware_id = get_string_property(set.value, data, SPDRP_HARDWAREID);
        device.class_name = get_string_property(set.value, data, SPDRP_CLASS);
        if (device.class_name.empty()) {
            device.class_name = L"Unknown";
        }
        device.present = present_ids.contains(upper_copy(device.instance_id));
        devices.push_back(std::move(device));
    }

    std::ranges::sort(devices, [](const DeviceInfo& lhs, const DeviceInfo& rhs) {
        if (lhs.present != rhs.present) {
            return !lhs.present;
        }
        return _wcsicmp(lhs.friendly_name.c_str(), rhs.friendly_name.c_str()) < 0;
    });
    return {true, L"Device scan completed"};
}

OperationResult DeviceManager::remove(const std::wstring& instance_id) const {
    DeviceInfoSet set;
    SP_DEVINFO_DATA data{};
    if (auto result = open_device(instance_id, set, data); !result.success) {
        return result;
    }

    if (!SetupDiRemoveDevice(set.value, &data)) {
        const DWORD error = GetLastError();
        return {false, L"Removal failed: " + format_windows_error(error)};
    }
    return {true, L"Device removed successfully"};
}

OperationResult DeviceManager::get_driver_package_inf(
    const std::wstring& instance_id, std::wstring& inf_name) const {
    inf_name.clear();
    DeviceInfoSet set;
    SP_DEVINFO_DATA data{};
    if (auto result = open_device(instance_id, set, data); !result.success) {
        return result;
    }

    inf_name = get_device_property(set.value, data, DEVPKEY_Device_DriverInfPath);
    if (inf_name.empty()) {
        return {false, L"No installed driver package was found for this device"};
    }
    return {true, L"Driver package found: " + inf_name};
}

OperationResult DeviceManager::uninstall_driver_package(const std::wstring& inf_name) const {
    if (inf_name.empty()) {
        return {false, L"The driver package name is empty"};
    }
    if (!SetupUninstallOEMInfW(inf_name.c_str(), 0, nullptr)) {
        const DWORD error = GetLastError();
        return {false, L"Driver package " + inf_name + L" was kept: " + format_windows_error(error)};
    }
    return {true, L"Driver package " + inf_name + L" removed successfully"};
}
