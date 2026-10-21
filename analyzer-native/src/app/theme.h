#pragma once

namespace kbi::ui {

enum class Theme { Dark, Light };

void apply_theme(Theme theme);
[[nodiscard]] Theme current_theme();
void toggle_theme();
[[nodiscard]] const char* theme_name(Theme theme);

} // namespace kbi::ui
