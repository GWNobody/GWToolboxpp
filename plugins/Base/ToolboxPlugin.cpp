#pragma once

#include "ToolboxPlugin.h"

// import PluginUtils;

/* From PluginUtils */
std::wstring StringToWString(const std::string& str)
{
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (str.empty()) {
        return {};
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
    PLUGIN_ASSERT(size_needed != 0);
    std::wstring wstrTo(size_needed, 0);
    PLUGIN_ASSERT(MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstrTo.data(), size_needed));
    return wstrTo;
}

std::filesystem::path ToolboxPlugin::GetSettingFile(const wchar_t* folder) const
{
    const auto wname = StringToWString(Name());
    const auto ininame = wname + L".ini";
    return std::filesystem::path(folder) / ininame;
}

void ToolboxPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions(allocator_fns.alloc_func, allocator_fns.free_func, allocator_fns.user_data);
    toolbox_handle = toolbox_dll;
}

