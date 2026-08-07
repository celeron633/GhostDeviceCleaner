#pragma once

#include "device_filter.h"
#include "device_manager.h"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
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

    struct RemovalJob {
        std::wstring instance_id;
        std::wstring name;
    };

    void scan_devices();
    void start_removal();
    void run_removal(std::vector<RemovalJob> jobs, bool remove_driver_package);
    void finish_removal();
    void draw_header();
    void draw_filters();
    void draw_device_table();
    void draw_log_panel();
    void draw_confirmation_popup();
    void draw_progress_popup();
    void draw_completion_popup();
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

    std::atomic<int> removal_completed_{0};
    std::atomic<int> removal_total_{0};
    std::atomic<int> removal_succeeded_{0};
    std::atomic<int> removal_failed_{0};
    std::atomic<bool> removal_running_{false};
    std::atomic<bool> removal_finished_{false};
    std::mutex removal_mutex_;
    std::string removal_current_item_;
    std::vector<LogEntry> removal_logs_;
    bool progress_popup_requested_ = false;
    bool progress_finish_seen_ = false;
    bool completion_popup_requested_ = false;
    int completed_total_ = 0;
    int completed_succeeded_ = 0;
    int completed_failed_ = 0;
    std::jthread removal_thread_;
};
