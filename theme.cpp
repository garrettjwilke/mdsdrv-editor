#include "theme.h"
#include <imgui.h>

namespace Theme {

void ApplyDefault() {
    ApplyDark();
}

void ApplyDark() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Higher-contrast dark theme
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.97f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.63f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.17f, 0.20f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.33f, 0.40f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.27f, 0.33f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.38f, 0.48f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.37f, 0.45f, 0.55f, 0.95f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.55f, 0.68f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.29f, 0.64f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.29f, 0.64f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.74f, 0.99f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.38f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.56f, 0.86f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.50f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.44f, 0.62f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.38f, 0.46f, 0.58f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.46f, 0.56f, 0.70f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.44f, 0.62f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.34f, 0.43f, 0.56f, 0.80f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.52f, 0.68f, 0.90f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.45f, 0.62f, 0.84f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.26f, 0.34f, 0.44f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.24f, 0.31f, 0.40f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.76f, 0.86f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.62f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.12f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.77f, 0.26f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.32f, 0.40f, 0.48f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.15f, 0.17f, 0.20f, 0.60f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.18f, 0.21f, 0.24f, 0.60f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.44f, 0.68f, 1.00f);
    // Match the text cursor to the current text color so it stays visible on dark backgrounds
    colors[ImGuiCol_InputTextCursor] = colors[ImGuiCol_Text];
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.44f, 0.68f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.78f, 0.86f, 0.98f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.14f, 0.16f, 0.19f, 0.80f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.12f, 0.14f, 0.17f, 0.80f);

    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}

void ApplyLight() {
    ImGui::StyleColorsLight();

    // Slightly bolder controls for readability on light background
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Button] = ImVec4(0.82f, 0.86f, 0.92f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.74f, 0.82f, 0.92f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.74f, 0.90f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.84f, 0.88f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.84f, 0.92f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.85f, 0.88f, 0.93f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.80f, 0.84f, 0.92f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.76f, 0.82f, 0.90f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.82f, 0.86f, 0.92f, 1.00f);
    // Keep the text cursor in sync with the theme's text color
    colors[ImGuiCol_InputTextCursor] = colors[ImGuiCol_Text];
}

void ApplyClassic() {
    ImGui::StyleColorsClassic();
    // Ensure the text cursor follows the text color for consistency
    ImGui::GetStyle().Colors[ImGuiCol_InputTextCursor] = ImGui::GetStyle().Colors[ImGuiCol_Text];
}

} // namespace Theme

