#include "application.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <filesystem>
#include <memory>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_render_target = nullptr;

void create_render_target() {
    ID3D11Texture2D* back_buffer = nullptr;
    if (SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target);
        back_buffer->Release();
    }
}

void cleanup_render_target() {
    if (g_render_target) {
        g_render_target->Release();
        g_render_target = nullptr;
    }
}

bool create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device,
            &feature_level, &g_context))) {
        return false;
    }
    create_render_target();
    return true;
}

void cleanup_device() {
    cleanup_render_target();
    if (g_swap_chain) g_swap_chain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    g_swap_chain = nullptr;
    g_context = nullptr;
    g_device = nullptr;
}

void configure_style() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(18, 16);
    style.ItemSpacing = ImVec2(9, 8);
    style.FramePadding = ImVec2(10, 7);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.082f, 0.105f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.095f, 0.105f, 0.135f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.31f, 0.53f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.42f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.29f, 0.48f, 1.0f);
}

void load_font() {
    wchar_t windows_dir[MAX_PATH]{};
    if (GetWindowsDirectoryW(windows_dir, MAX_PATH) == 0) {
        return;
    }
    const auto font = std::filesystem::path(windows_dir) / L"Fonts" / L"msyh.ttc";
    if (std::filesystem::exists(font)) {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(font.string().c_str(), 17.0f, nullptr,
            ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    }
}

LRESULT WINAPI window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) {
        return true;
    }
    switch (message) {
    case WM_SIZE:
        if (g_device && wparam != SIZE_MINIMIZED) {
            cleanup_render_target();
            g_swap_chain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
            create_render_target();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t class_name[] = L"RemoveGhostDevicesWindow";
    WNDCLASSEXW window_class{sizeof(window_class), CS_CLASSDC, window_proc, 0, 0, instance,
        nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, class_name, nullptr};
    RegisterClassExW(&window_class);

    HWND window = CreateWindowW(class_name, L"Remove Ghost Devices", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr, nullptr, instance, nullptr);
    if (!window || !create_device(window)) {
        MessageBoxW(nullptr, L"Unable to initialize the DirectX 11 renderer.", L"Startup error", MB_ICONERROR);
        if (window) DestroyWindow(window);
        UnregisterClassW(class_name, instance);
        return 1;
    }

    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    configure_style();
    load_font();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_device, g_context);

    auto app = std::make_unique<Application>();
    bool done = false;
    while (!done) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app->render();
        ImGui::Render();

        constexpr float clear_color[4] = {0.075f, 0.082f, 0.105f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_render_target, nullptr);
        g_context->ClearRenderTargetView(g_render_target, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);
    }

    app.reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    DestroyWindow(window);
    UnregisterClassW(class_name, instance);
    return 0;
}
