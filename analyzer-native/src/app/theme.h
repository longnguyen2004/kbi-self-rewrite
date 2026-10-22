#pragma once

namespace Kbi::Ui {

enum class Theme { Dark,
    Light };

void applyTheme(Theme theme);
[[nodiscard]] Theme currentTheme();
void toggleTheme();
[[nodiscard]] const char* themeName(Theme theme);

} // namespace Kbi::ui
