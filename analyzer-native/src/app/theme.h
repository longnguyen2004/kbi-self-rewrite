#pragma once

#include <string_view>

namespace Kbi::Ui {

enum class Theme { Dark,
    Light };

void applyTheme(Theme theme);
[[nodiscard]] Theme currentTheme();
void toggleTheme();
[[nodiscard]] std::string_view themeName(Theme theme);

} // namespace Kbi::Ui
