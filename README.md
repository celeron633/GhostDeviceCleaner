# Remove Ghost Devices

一个使用 C++20、Win32 API、SetupAPI、DirectX 11 和 Dear ImGui 编写的 Windows 幽灵设备清理工具。功能参考仓库内的 `RemoveGhostDevices_EN.ps1`，但使用“当前设备集合”与“全部设备集合”的差集判断未连接设备。

## 功能

- 扫描所有已注册的即插即用设备，并识别当前未连接的幽灵设备
- 显示设备名称、类别、硬件 ID、实例 ID 和连接状态
- 按名称/ID 搜索、按设备类别筛选、只显示幽灵设备
- 批量选择可见设备，删除前二次确认
- 删除确认中默认勾选“尝试删除设备驱动程序包”；仍被其他设备使用时不会强制删除
- 非管理员权限下可浏览，删除按钮保持禁用，并可从界面请求管理员权限重启
- 显示扫描及删除结果日志

## 构建

需要 Windows、CMake 3.24+ 和支持 C++20 的 Visual Studio。首次配置时 CMake 会从官方仓库下载固定版本的 Dear ImGui。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

生成的程序位于 `build/Release/RemoveGhostDevices.exe`。删除设备需要管理员权限；设备重新连接后，Windows 仍可能重新安装它。

## GitHub Actions

仓库包含 Windows x64 自动构建工作流。推送代码、创建 Pull Request 或手动运行工作流后，会生成 `RemoveGhostDevices-windows-x64.zip`，并在对应 Actions 运行页面保留 30 天供下载。

## 源码结构

- `src/main.cpp`：Win32 窗口、DirectX 11 渲染和 ImGui 生命周期
- `src/application.*`：界面布局、选择、确认和操作日志
- `src/device_manager.*`：SetupAPI 设备枚举与删除
- `src/device_filter.*`：搜索和类别筛选
- `src/admin_utils.*`：管理员权限检测与提权重启
- `src/text_utils.*`：UTF-8 / UTF-16 转换
