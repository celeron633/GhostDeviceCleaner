#include "application.h"

#include "admin_utils.h"
#include "text_utils.h"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <unordered_set>

namespace {

ImVec4 color_from_hex(unsigned int hex, float alpha = 1.0f) {
    return ImVec4(
        static_cast<float>((hex >> 16) & 0xff) / 255.0f,
        static_cast<float>((hex >> 8) & 0xff) / 255.0f,
        static_cast<float>(hex & 0xff) / 255.0f,
        alpha);
}

std::string display_value(const std::wstring& value) {
    return wide_to_utf8(value.empty() ? L"-" : value);
}

} // namespace

Application::Application() : elevated_(admin::is_elevated()) {
    scan_devices();
}

void Application::scan_devices() {
    const auto result = manager_.enumerate(devices_);
    classes_ = collect_device_classes(devices_);
    if (selected_class_ > static_cast<int>(classes_.size())) {
        selected_class_ = 0;
    }
    logs_.push_back({result.success, wide_to_utf8(result.message)});
}

int Application::selected_ghost_count() const {
    return static_cast<int>(std::ranges::count_if(devices_, [](const DeviceInfo& device) {
        return device.selected && !device.present;
    }));
}

int Application::visible_device_count() const {
    DeviceFilter filter{utf8_to_wide(search_.data()),
        selected_class_ > 0 ? classes_[selected_class_ - 1] : L"", ghosts_only_};
    return static_cast<int>(std::ranges::count_if(devices_, [&](const DeviceInfo& device) {
        return device_matches(device, filter);
    }));
}

void Application::draw_header() {
    const int ghost_count = static_cast<int>(std::ranges::count_if(devices_, [](const DeviceInfo& device) {
        return !device.present;
    }));

    ImGui::TextColored(color_from_hex(0x89B4FA), "GHOST DEVICE CLEANER");
    ImGui::SameLine();
    ImGui::TextDisabled("  SetupAPI / Win32");
    ImGui::Spacing();

    ImGui::BeginChild("summary", ImVec2(0, 82), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(18, 15));
    ImGui::TextDisabled("ALL DEVICES");
    ImGui::SetCursorPos(ImVec2(18, 39));
    ImGui::Text("%d", static_cast<int>(devices_.size()));
    ImGui::SameLine(180);
    ImGui::BeginGroup();
    ImGui::TextDisabled("GHOST DEVICES");
    ImGui::TextColored(color_from_hex(0xF9E2AF), "%d", ghost_count);
    ImGui::EndGroup();
    ImGui::SameLine(370);
    ImGui::BeginGroup();
    ImGui::TextDisabled("VISIBLE");
    ImGui::Text("%d", visible_device_count());
    ImGui::EndGroup();
    ImGui::SameLine(520);
    ImGui::BeginGroup();
    ImGui::TextDisabled("PRIVILEGES");
    ImGui::TextColored(elevated_ ? color_from_hex(0xA6E3A1) : color_from_hex(0xF38BA8),
        "%s", elevated_ ? "Administrator" : "Read only");
    ImGui::EndGroup();
    ImGui::EndChild();
}

void Application::draw_filters() {
    ImGui::SetNextItemWidth(330.0f);
    ImGui::InputTextWithHint("##search", "Search name, hardware ID or instance ID", search_.data(), search_.size());
    ImGui::SameLine();

    const std::string preview = selected_class_ == 0 ? "All classes" : wide_to_utf8(classes_[selected_class_ - 1]);
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::BeginCombo("##class", preview.c_str())) {
        if (ImGui::Selectable("All classes", selected_class_ == 0)) {
            selected_class_ = 0;
        }
        for (int i = 0; i < static_cast<int>(classes_.size()); ++i) {
            const auto label = wide_to_utf8(classes_[i]);
            if (ImGui::Selectable(label.c_str(), selected_class_ == i + 1)) {
                selected_class_ = i + 1;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Ghosts only", &ghosts_only_);
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        scan_devices();
    }

    DeviceFilter filter{utf8_to_wide(search_.data()),
        selected_class_ > 0 ? classes_[selected_class_ - 1] : L"", ghosts_only_};
    if (ImGui::Button("Select visible ghosts")) {
        for (auto& device : devices_) {
            if (!device.present && device_matches(device, filter)) {
                device.selected = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear selection")) {
        for (auto& device : devices_) {
            device.selected = false;
        }
    }
    ImGui::SameLine();
    const int selected = selected_ghost_count();
    ImGui::BeginDisabled(selected == 0 || !elevated_);
    if (ImGui::Button(("Remove selected (" + std::to_string(selected) + ")").c_str())) {
        confirmation_requested_ = true;
    }
    ImGui::EndDisabled();
    if (!elevated_) {
        ImGui::SameLine();
        if (ImGui::Button("Restart as administrator")) {
            if (admin::restart_elevated()) {
                PostQuitMessage(0);
            } else {
                logs_.push_back({false, "Elevation was cancelled or failed"});
            }
        }
    }
}

void Application::draw_device_table() {
    const float log_height = 128.0f;
    if (!ImGui::BeginTable("devices", 6,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
                | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, -log_height))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("##select", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 34.0f);
    ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch, 2.1f);
    ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 0.8f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 85.0f);
    ImGui::TableSetupColumn("Hardware ID", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Instance ID", ImGuiTableColumnFlags_WidthStretch, 1.8f);
    ImGui::TableHeadersRow();

    DeviceFilter filter{utf8_to_wide(search_.data()),
        selected_class_ > 0 ? classes_[selected_class_ - 1] : L"", ghosts_only_};
    for (int index = 0; index < static_cast<int>(devices_.size()); ++index) {
        auto& device = devices_[index];
        if (!device_matches(device, filter)) {
            continue;
        }

        ImGui::PushID(index);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (device.present) {
            ImGui::BeginDisabled();
        }
        ImGui::Checkbox("##selected", &device.selected);
        if (device.present) {
            device.selected = false;
            ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(1);
        const auto friendly_name = display_value(device.friendly_name);
        ImGui::TextUnformatted(friendly_name.c_str());
        ImGui::TableSetColumnIndex(2);
        const auto class_name = display_value(device.class_name);
        ImGui::TextUnformatted(class_name.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextColored(device.present ? color_from_hex(0xA6E3A1) : color_from_hex(0xF9E2AF),
            "%s", device.present ? "Connected" : "Ghost");
        ImGui::TableSetColumnIndex(4);
        const auto hardware_id = display_value(device.hardware_id);
        ImGui::TextUnformatted(hardware_id.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", hardware_id.c_str());
        }
        ImGui::TableSetColumnIndex(5);
        const auto instance_id = display_value(device.instance_id);
        ImGui::TextUnformatted(instance_id.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", instance_id.c_str());
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void Application::draw_log_panel() {
    ImGui::SeparatorText("Activity");
    ImGui::BeginChild("activity", ImVec2(0, 78), ImGuiChildFlags_Borders);
    for (const auto& entry : logs_) {
        ImGui::TextColored(entry.success ? color_from_hex(0xA6E3A1) : color_from_hex(0xF38BA8),
            "%s  %s", entry.success ? "OK" : "ERR", entry.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void Application::start_removal() {
    if (removal_running_.load()) {
        return;
    }

    std::vector<RemovalJob> jobs;
    for (const auto& device : devices_) {
        if (device.selected && !device.present) {
            jobs.push_back({device.instance_id, device.friendly_name});
        }
    }
    if (jobs.empty()) {
        return;
    }

    if (removal_thread_.joinable()) {
        removal_thread_.join();
    }

    removal_completed_.store(0);
    removal_total_.store(static_cast<int>(jobs.size()));
    removal_succeeded_.store(0);
    removal_failed_.store(0);
    removal_finished_.store(false);
    removal_running_.store(true);
    {
        std::scoped_lock lock(removal_mutex_);
        removal_current_item_ = "Preparing uninstall tasks...";
        removal_logs_.clear();
    }
    progress_finish_seen_ = false;
    progress_popup_requested_ = true;

    const bool remove_driver_package = remove_driver_package_;
    removal_thread_ = std::jthread([this, jobs = std::move(jobs), remove_driver_package]() mutable {
        run_removal(std::move(jobs), remove_driver_package);
    });
}

void Application::run_removal(std::vector<RemovalJob> jobs, bool remove_driver_package) {
    std::vector<LogEntry> operation_logs;
    std::unordered_set<std::wstring> driver_packages;

    const auto set_current_item = [this](std::string text) {
        std::scoped_lock lock(removal_mutex_);
        removal_current_item_ = std::move(text);
    };

    if (remove_driver_package) {
        for (const auto& job : jobs) {
            set_current_item("Reading driver information: " + wide_to_utf8(job.name));
            std::wstring inf_name;
            const auto driver_result = manager_.get_driver_package_inf(job.instance_id, inf_name);
            if (driver_result.success) {
                driver_packages.insert(std::move(inf_name));
            } else {
                operation_logs.push_back({false, wide_to_utf8(job.name + L": " + driver_result.message)});
            }
        }
        removal_total_.store(static_cast<int>(jobs.size() + driver_packages.size()));
    }

    for (const auto& job : jobs) {
        set_current_item("Removing device: " + wide_to_utf8(job.name));
        const auto result = manager_.remove(job.instance_id);
        operation_logs.push_back({result.success, wide_to_utf8(job.name + L": " + result.message)});
        if (result.success) {
            removal_succeeded_.fetch_add(1);
        } else {
            removal_failed_.fetch_add(1);
        }
        removal_completed_.fetch_add(1);
    }

    if (remove_driver_package) {
        for (const auto& inf_name : driver_packages) {
            set_current_item("Removing driver package: " + wide_to_utf8(inf_name));
            const auto result = manager_.uninstall_driver_package(inf_name);
            operation_logs.push_back({result.success, wide_to_utf8(result.message)});
            if (result.success) {
                removal_succeeded_.fetch_add(1);
            } else {
                removal_failed_.fetch_add(1);
            }
            removal_completed_.fetch_add(1);
        }
    }

    {
        std::scoped_lock lock(removal_mutex_);
        removal_current_item_ = "Finalizing...";
        removal_logs_ = std::move(operation_logs);
    }
    removal_running_.store(false);
    removal_finished_.store(true);
}

void Application::finish_removal() {
    if (removal_thread_.joinable()) {
        removal_thread_.join();
    }

    {
        std::scoped_lock lock(removal_mutex_);
        logs_.insert(logs_.end(),
            std::make_move_iterator(removal_logs_.begin()),
            std::make_move_iterator(removal_logs_.end()));
        removal_logs_.clear();
    }

    completed_total_ = removal_total_.load();
    completed_succeeded_ = removal_succeeded_.load();
    completed_failed_ = removal_failed_.load();
    removal_finished_.store(false);
    scan_devices();
    completion_popup_requested_ = true;
}

void Application::draw_confirmation_popup() {
    if (confirmation_requested_) {
        ImGui::OpenPopup("Confirm removal");
        confirmation_requested_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Confirm removal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Remove %d selected ghost device(s)?", selected_ghost_count());
        ImGui::Spacing();
        ImGui::TextColored(color_from_hex(0xF9E2AF),
            "Windows may reinstall a device if it is connected again.");
        ImGui::Spacing();
        ImGui::Checkbox("Attempt to remove the driver package for this device", &remove_driver_package_);
        ImGui::TextDisabled("The package is kept when another device is still using it.");
        ImGui::Spacing();
        if (ImGui::Button("Remove", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            start_removal();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Application::draw_progress_popup() {
    if (progress_popup_requested_) {
        ImGui::OpenPopup("Uninstall progress");
        progress_popup_requested_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Uninstall progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const int completed = removal_completed_.load();
        const int total = std::max(removal_total_.load(), 1);
        const float progress = std::clamp(
            static_cast<float>(completed) / static_cast<float>(total), 0.0f, 1.0f);
        std::string current_item;
        {
            std::scoped_lock lock(removal_mutex_);
            current_item = removal_current_item_;
        }

        ImGui::Text("Processing %d / %d", completed, total);
        ImGui::Spacing();
        const std::string overlay = std::to_string(completed) + " / " + std::to_string(total);
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 22.0f), overlay.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("%s", current_item.c_str());
        ImGui::TextDisabled("Please do not close the application while devices are being removed.");

        if (removal_finished_.load()) {
            if (progress_finish_seen_) {
                ImGui::CloseCurrentPopup();
                finish_removal();
            } else {
                progress_finish_seen_ = true;
            }
        }
        ImGui::EndPopup();
    }
}

void Application::draw_completion_popup() {
    if (completion_popup_requested_) {
        ImGui::OpenPopup("Uninstall complete");
        completion_popup_requested_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Uninstall complete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Uninstall processing is complete.");
        ImGui::Spacing();
        ImGui::Text("Processed: %d", completed_total_);
        ImGui::TextColored(color_from_hex(0xA6E3A1), "Succeeded: %d", completed_succeeded_);
        ImGui::TextColored(
            completed_failed_ == 0 ? color_from_hex(0xA6E3A1) : color_from_hex(0xF38BA8),
            "Failed: %d", completed_failed_);
        if (completed_failed_ > 0) {
            ImGui::Spacing();
            ImGui::TextWrapped("Some items could not be removed. See Activity for details.");
        }
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Application::render() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Remove Ghost Devices", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    draw_header();
    ImGui::Spacing();
    draw_filters();
    ImGui::Spacing();
    draw_device_table();
    draw_log_panel();
    draw_confirmation_popup();
    draw_progress_popup();
    draw_completion_popup();
    ImGui::End();
}
