#pragma once

#include <string>

[[nodiscard]] std::string wide_to_utf8(const std::wstring& text);
[[nodiscard]] std::wstring utf8_to_wide(const std::string& text);

