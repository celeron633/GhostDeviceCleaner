#pragma once

#include "device_filter.h"
#include "device_manager.h"

#include <array>
#include <string>
#include <vector>

class Application {
public:
    Application();

    void render();

private:
    struct LogEntry {
        bool success;
        std::string text;
    };

    void scan_devices();
    void remove_selected();
    void draw_header();
    void draw_filters();
    void draw_device_table();
    void draw_log_panel();
    void draw_confirmation_popup();
    [[nodiscard]] int selected_ghost_count() const;
    [[nodiscard]] int visible_device_count() const;

    DeviceManager manager_;
    std::vector<DeviceInfo> devices_;
    std::vector<std::wstring> classes_;
    std::vector<LogEntry> logs_;
    std::array<char, 256> search_{};
    int selected_class_ = 0;
    bool ghosts_only_ = true;
    bool elevated_ = false;
    bool confirmation_requested_ = false;
    bool remove_driver_package_ = true;
};
