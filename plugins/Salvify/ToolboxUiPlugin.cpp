// Local
#include <PluginUtils.h>
#include "ToolBoxUiPlugin.h"

// Global
#include <Windows.h>
#include <regex>

#include <GWCA/Context/AccountContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/ItemIDs.h>

#include "GWCA/GWCA.h"


namespace {

    std::regex reg("<[^>]+>");
}; 

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ToolboxUIPlugin instance;
    return &instance;
}

void ToolboxUIPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns fns, const HMODULE toolbox_dll)
{
    PathGetComputerName(computer_name);
    PathGetDocumentsPath(docpath, L"GWToolboxpp");
    docpath = docpath / computer_name / plugins;
    // ToolboxUIPlugin::Initialize(ctx, fns, toolbox_dll);
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions(fns.alloc_func, fns.free_func, fns.user_data);
    toolbox_handle = toolbox_dll;
    GW::Initialize();
}

void ToolboxUIPlugin::Draw(IDirect3DDevice9*)
{
    const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | (lock_move ? ImGuiWindowFlags_NoMove : 0) | (lock_size ? ImGuiWindowFlags_NoResize : 0);
    plugin_visible = true;
    ImVec2 sz = ImVec2(-FLT_MIN, -FLT_MIN);
    if (ImGui::Begin(Name(), can_close && show_closebutton ? GetVisiblePtr() : nullptr, flags)) {
        if ((setting_button_counter) && (setting_ident_blue || setting_ident_purple || setting_ident_gold)) {
            ImVec2 avail_size = ImGui::GetContentRegionAvail();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y - 8));
            float old_fontsize = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale *= setting_button_counter_fontsize;
            ImGui::PushFont(ImGui::GetFont());
            if (setting_ident_blue) {
                ImGui::TextColored(ImVec4(item_color_blue), "%d ", bluect);
                ImGui::SameLine();
            }
            if (setting_ident_purple) {
                ImGui::TextColored(ImVec4(item_color_purple), "%d ", purplect);
                ImGui::SameLine();
            }
            if (setting_ident_gold) {
                ImGui::TextColored(ImVec4(item_color_gold), "%d ", goldct);
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::GetFont()->Scale = old_fontsize;
            ImGui::PopFont();
        }
        if (salvage_listeners_attached) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(progress_color));
            ImGui::SameLine();
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(is_progress).x) * 0.95f);
            ImGui::Text(is_progress);
            ImGui::PopStyleColor(1);
        }

        ImGui::PushID(1);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col0[0], col0[1], col0[2], col0[3]));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col2[0], col2[1], col2[2], col2[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col1[0], col1[1], col1[2], col1[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col2[0], col2[1], col2[2], col2[3]));

        {
            float old_fontsize = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale *= setting_button_fontsize;
            ImGui::PushFont(ImGui::GetFont());
            if (ImGui::Button(text_button, sz)) {
                IdentifyAll();
            }
            ImGui::GetFont()->Scale = old_fontsize;
            ImGui::PopFont();
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
        ImGui::End();
    }
    // Display popup under certain condition
    if (salvage_popup || init_salvage_examplepopup && setting_salvage_module) {
        if (ImGui::Begin("Salvage request", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {
            PopUp_Item();
            ImGui::End();
        }
    }
}


std::string WideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}


void ToolboxUIPlugin::PopUp_Item()
{
    auto flags = p_lock_move ? ImGuiWindowFlags_NoMove : 0;
    if (ImGui::Begin("Popup Window", &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | flags)) {
        
        ImVec2 bt_sz = ImGui::GetContentRegionAvail();

        std::string name_str = WideToUtf8(item_popup_name_buffer);
        std::string info_str = WideToUtf8(item_popup_info_buffer);

        std::istringstream stream;
        stream.str(name_str + "\n" + info_str);
        std::string lines;

        scale_item_title = true;
        float old_fontsize = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= setting_popup_title_fontsize;
        ImGui::PushFont(ImGui::GetFont());


        while (std::getline(stream, lines)) {
            format_item_description it;
            while ((it.endpos = lines.find("</c>", it.startpos)) != std::string::npos) {
                it.length = it.endpos - it.startpos;
                it.line = lines.substr(it.startpos, it.length + 4);
                if (it.line.find("ItemDull") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_gray));
                }
                else if (it.line.find("ItemBasic") != std::string::npos || it.line.find("ItemCommon") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color_white));
                }
                else if (it.line.find("ItemBonus") != std::string::npos || it.line.find("ItemEnhance") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_blue));
                }
                else if (it.line.find("ItemRare") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_gold));
                }
                else if (it.line.find("ItemUncommon") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_purple));
                }
                else if (it.line.find("ItemUnique") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_green));
                }
                else {
                    break;
                }
                if (it.count >= 1) {
                    ImGui::SameLine();
                }
                it.format_text = std::regex_replace(it.line, reg, "");
                ImGui::TextUnformatted(it.format_text.c_str());
                ImGui::PopStyleColor();
                it.count++;
                it.startpos += 4 + it.endpos;
            }
            if (scale_item_title) {
                ImGui::GetFont()->Scale = old_fontsize;
                ImGui::PopFont();
                scale_item_title = false;
            } 
        }
        // click ok when finished adjusting
        ImGui::NewLine();
        ImGui::Separator();

        ImGui::GetFont()->Scale *= setting_popup_button_scale;
        ImGui::PushFont(ImGui::GetFont());

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col_pop_true_0[0], col_pop_true_0[1], col_pop_true_0[2], col_pop_true_0[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col_pop_true_1[0], col_pop_true_1[1], col_pop_true_1[2], col_pop_true_1[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col_pop_true_2[0], col_pop_true_2[1], col_pop_true_2[2], col_pop_true_2[3]));
        if (prepare_salvage_count) {
            if (clock() / 100 - popup_time_delay > 7) {
                if (salvage_counter == 0) {
                    prepare_salvage_count = false;
                    if (salvage_popup) {
                        init_salvaging = true;
                        salvage_popup = false;
                    }
                }
                else {
                    salvage_counter -= 1;
                    popup_time_delay = clock() / 100;
                }
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col_pop_true_1[0], col_pop_true_1[1], col_pop_true_1[2], col_pop_true_1[3]));
            ImGui::Button(std::to_string(salvage_counter).c_str(), ImVec2(bt_sz.x / 2, 40));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col_pop_true_2[0], col_pop_true_2[1], col_pop_true_2[2], col_pop_true_2[3]));
            if (ImGui::Button("SALVAGE", ImVec2(bt_sz.x / 2, 40))) {
                prepare_salvage_count = true;
                salvage_counter = 3;
                popup_time_delay = clock() / 100;
            }
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col_pop_false_0[0], col_pop_false_0[1], col_pop_false_0[2], col_pop_false_0[3]));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col_pop_false_2[0], col_pop_false_2[1], col_pop_false_2[2], col_pop_false_2[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col_pop_false_1[0], col_pop_false_1[1], col_pop_false_1[2], col_pop_false_1[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col_pop_false_2[0], col_pop_false_2[1], col_pop_false_2[2], col_pop_false_2[3]));
        if (ImGui::Button("STOP", ImVec2(bt_sz.x / 2.07f, 40))) {
            prepare_salvage_count = false;
            if (salvage_popup) {
                pending_item.erase(pending_item.begin());
                salvage_popup = false;
                new_pending_item_session = true;
            }
        }
        ImGui::PopStyleColor(4);

        ImGui::GetFont()->Scale = old_fontsize;
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::End();
    }
}

void TextCentered(std::string text, ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
{
    auto windowWidth = ImGui::GetWindowSize().x;
    auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::TextColored(ImVec4(color), text.c_str());
}

void ToolboxUIPlugin::DrawSettings()
{
    ToolboxPlugin::DrawSettings();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    is_popup_open = false;
    ImGui::NewLine();
    TextCentered("SA L V I F Y", ImVec4(item_color_gold));
    TextCentered("A handsome plugin to identify and salvage items in a smooth and effortless way.", ImVec4(text_color_whitegray));
    ImGui::NewLine();
    if (ImGui::TreeNode("Settings")) {
        if (ImGui::TreeNode("General")) {
            ImGui::Separator();
            ImGui::Text("Item rarity which should be processed:");
            ImGui::Checkbox("Blue   -  ", &setting_ident_blue);
            ImGui::SetItemTooltip("Include all unidentified blue items.");
            ImGui::SameLine();
            ImGui::Checkbox("Purple   -  ", &setting_ident_purple);
            ImGui::SetItemTooltip("Include all unidentified purple items.");
            ImGui::SameLine();
            ImGui::Checkbox("Gold", &setting_ident_gold);
            ImGui::SetItemTooltip("Include all unidentified gold items.");
            ImGui::NewLine();
            ImGui::Checkbox("Print result on chat", &setting_ident_chatprint);
            if (ImGui::Checkbox("Enable Salvage", &setting_salvage_module)) {
                if (setting_salvage_module) {
                    text_button = "=- I&S ALL -=";
                }
                else {
                    text_button = "= IDENTIFY ALL =";
                }
            }
            ImGui::SetItemTooltip("A popup appears.");
            if (setting_salvage_module) {
                ImGui::Text(" -> ");
                ImGui::SameLine();
                ImGui::Checkbox("Auto salvage", &setting_auto_salvage);
                ImGui::SetItemTooltip("Items will be get automatically salvaged.");
                if (setting_auto_salvage) {
                    ImGui::TextColored(ImVec4(color_warning), "! Warning ! :: This can be resulted in loss of rare drops. Use it with caution !");
                }
            }
            ImGui::NewLine();

            ImGui::Separator();
            ImGui::TreePop();
        }



        if (ImGui::TreeNode("Widgets")) {
            ImGui::Separator();
            if (ImGui::TreeNode("Button")) {
                ImGui::Separator();
                ImGui::NewLine();
                ImVec2 b_pos(0, 0);
                ImVec2 b_size(200.0f, 85.0f);
                if (const auto window = ImGui::FindWindowByName(Name())) {
                    b_pos = window->Pos;
                    b_size = window->Size;
                }
                if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&b_pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
                    ImGui::SetWindowPos(Name(), b_pos);
                }

                if (ImGui::DragFloat2("Size", reinterpret_cast<float*>(&b_size), 1.0f, 0.0f, 0.0f, "%.0f")) {
                    ImGui::SetWindowSize(Name(), b_size);
                }

                ImGui::Checkbox("Lock Position", &lock_move);
                ImGui::SameLine();
                ImGui::Checkbox("Lock Size", &lock_size);
                ImGui::NewLine();

                ImGui::Text("Button Color");
                ImGui::ColorEdit4("Background color", col2);
                ImGui::ColorEdit4("Hover effect", col1);
                ImGui::ColorEdit4("Text color", col0);
                ImGui::DragFloat("Text size", &setting_button_fontsize, 0.005f, 0.1f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Checkbox("Display counter", &setting_button_counter);
                if (setting_button_counter) {
                    ImGui::DragFloat("Text scale", &setting_button_counter_fontsize, 0.005f, 0.1f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                }
                ImGui::NewLine();
                ImGui::Separator();
                ImGui::TreePop();
            }
            if (setting_salvage_module) {
                if (ImGui::TreeNode("Popup")) {
                    is_popup_open = true;
                    ImVec2 p_pos(0, 0);
                    ImVec2 p_size(200.0f, 85.0f);
                    ImGui::Separator();
                    ImGui::NewLine();

                    if (const auto window = ImGui::FindWindowByName("Popup Window")) {
                        p_pos = window->Pos;
                        p_size = window->Size;
                        if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&p_pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
                            ImGui::SetWindowPos("Popup Window", p_pos);
                        }
                    }
                    ImGui::Checkbox("Lock Position", &p_lock_move);
                    ImGui::NewLine();

                    ImGui::Text("Salvage Button");
                    ImGui::ColorEdit4(" Background color", col_pop_true_2);
                    ImGui::ColorEdit4(" Hover effect", col_pop_true_1);
                    ImGui::ColorEdit4(" Text color", col_pop_true_0);
                    ImGui::NewLine();
                    ImGui::Text("Stop Button");
                    ImGui::ColorEdit4(" Background color ", col_pop_false_2);
                    ImGui::ColorEdit4(" Hover effect ", col_pop_false_1);
                    ImGui::ColorEdit4(" Text color ", col_pop_false_0);
                    ImGui::NewLine();
                    ImGui::Text("Item Title");
                    ImGui::DragFloat(" Text scale", &setting_popup_title_fontsize, 0.005f, 0.1f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::Text("Popup Buttons");
                    ImGui::DragFloat(" Text scale", &setting_popup_button_scale, 0.005f, 0.1f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();
                    ImGui::TreePop();

                    if (!salvage_listeners_attached && init_salvage_examplepopup == false) {
                        std::copy(example_weapon_name.begin(), example_weapon_name.end(), item_popup_name_buffer);
                        std::copy(example_weapon_info.begin(), example_weapon_info.end(), item_popup_info_buffer);
                        init_salvage_examplepopup = true;
                    }
                }
            }
            ImGui::Separator();
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Blacklist")) {
        if (clock() / 100 - setting_time_delay > 10) {
            setting_time_delay = clock() / 250;
            WhitelistItems();
        }
        ImGui::NewLine();
        ImGui::Text("The blacklist exclude items from all subsequent processes.");
        ImGui::NewLine();
        ImGui::Text("Equipment :");
        ImGui::Separator();
        Setting_WB_Table(whitelist, 0);
        ImGui::NewLine();

        ImGui::Text("Blacklist:");
        ImGui::Separator();
        ImGui::SetItemTooltip("The model id of the item will be compared against the blacklist.");
        Setting_WB_Table(blacklist, 1);
        ImGui::Checkbox("Global", &setting_print_blitems);
        ImGui::SetItemTooltip("Display all blacklisted items they aren't contained in the inventory.");
        ImGui::NewLine();
        ImGui::Separator();

        ImGui::TreePop();
    }


    if (ImGui::TreeNode("Help")) {
        ImGui::NewLine();
        ImGui::TextColored(ImVec4(text_color_whitegray), "Salvify use the Guild Wars internal API \"GWCA++\" to handle game content.");

        ImGui::NewLine();
        ImGui::Text("Question & Anwser");
        ImGui::Separator();
        ImGui::Text("Q: What\'s the approach of this tool ?");
        ImGui::TextColored(ImVec4(text_color_whitegray), "A: The plugin was made by the idea collecting craft materials in a convenient way. It should help to optimize user's workflow and maintain a clean inventory space.");
        ImGui::NewLine();
        ImGui::Text("Q: How it works ?");
        ImGui::TextColored(
            ImVec4(text_color_whitegray),
            "A: The plugin itself offers a wide range of customizations, including layout changes and built-in preferences. You can chose which raritie of an item should be processed and whether it should be salvaged or not - either via a popup or automatically."
        );
        ImGui::NewLine();
        ImGui::Text("Q: Is it possible that items in my inventory get randomly salvaged by this plugin?");
        ImGui::TextColored(
            ImVec4(text_color_whitegray),
            "A: An item needs to meet some certain conditions before it get salvaged:\n      1.  The unidentified item must be located in a bag and not in the equipment pack or (Xunlai) storage.\n      2.  If an item is affected by the blacklist, it will be skipped; otherwise, the object will be identified.\n      3.  If the option \"Enable Salvage\" is active an popup appears and stand by of further action from the user.\n      5.  If \"Auto Salvage\" is active the popup will be skipped, and the item is going to be salvaged without any user interaction.\n"
        );
        ImGui::NewLine();
        ImGui::Text("Q: What does the blacklist?");
        ImGui::TextColored(
            ImVec4(text_color_whitegray),
            "The blacklist excludes items inserted by the user from further processing. For example, if you want an item to remain unidentified, just add it to the blacklist. Salvify stores the model ID and subsequently compares it with the current item in the upcoming tasks. The model ID itself is defined by the item's entire state - the skill requirement doesn't matter, only the name does."
        );
        ImGui::NewLine();
        TextCentered("Credits ", ImVec4(item_color_gold));
        ImGui::Separator();
        ImGui::TextColored(
            ImVec4(text_color_whitegray),
            "Building this project was great fun and gave me a glimpse beneath the surface of GWToolbox and its external libraries. My gratitude goes out to the developers of GWToolbox and the GWCA project - without their efforts, it would not have been possible for me to understand even a bit of the logic behind Guild Wars. I'd also like to thank Dear ImGui for providing such a well-designed layout engine."
        );

        ImGui::NewLine();

        ImGui::Checkbox("Debug Log", &setting_debug_option);
        ImGui::SetItemTooltip("Developer feature.");

        if (setting_debug_option) {
            ImGui::Text(" -> ");
            ImGui::SameLine();
            ImGui::Checkbox("Extended Debug Log", &setting_extended_debug_option);
        }

        ImGui::TreePop();
    }
    ImGui::NewLine();
    ImGui::Separator();

    if (ImGui::Button("Save", ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2, 0))) {
        SaveSettings();
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::NewLine();

    if (is_popup_open == false) {
        init_salvage_examplepopup = false;
    }
}


void ToolboxUIPlugin::ChatResult()
{
    SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFF62827E);
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L"____________");
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L" ");
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L" = TOTAL = ");
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L"____________");
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L" ");
    if (setting_ident_blue) {
        SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFF88CCCC);
        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"BLUE      :  %d", bluect);
    }
    if (setting_ident_purple) {
        SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFFBF6DEE);
        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"PURPLE :  %d", purplect);
    }
    if (setting_ident_gold) {
        SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFFFFCC55);
        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"GOLD    :  %d", goldct);
    }
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L" ");

    SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xCCBFBFBF);
    WriteChatF(GW::Chat::CHANNEL_GWCA1, L"SKIPPED ITEMS:  %d", bl_count);

    SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0);
}


void ToolboxUIPlugin::Setting_WB_Table(std::vector<std::string>& tablecontent, int count)
{
    ImGui::PushID(count);
    if (ImGui::BeginTable("Table", 4 - count)) {
        ImGui::TableSetupColumn("Model ID", ImGuiTableColumnFlags_WidthFixed);
        if (count == 0) {
            ImGui::TableSetupColumn("Item ID", ImGuiTableColumnFlags_WidthFixed);
        }
        ImGui::TableSetupColumn("Item Name");
        ImGui::TableSetupColumn(" Action ", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        for (size_t a = 0, size = tablecontent.size(); a < size; a++) {
            if (tablecontent[a] == "0") {
                continue;
            }

            ImGui::PushID(a);
            setting_whitelist q;
            while (q.i < 5) {
                q.endpos = tablecontent[a].find(":", q.startpos);
                if (q.i == 4) {
                    q.endpos = tablecontent[a].length();
                }
                q.length = q.endpos - q.startpos;
                q.token = tablecontent[a].substr(q.startpos, q.length);

                switch (q.i) {
                    case 2:
                        if (q.token == "Enhance") {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_blue));
                        }
                        else if (q.token == "Uncommon") {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_purple));
                        }
                        else if (q.token == "Rare") {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item_color_gold));
                        }
                        else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color_white));
                        }
                        break;

                    case 4:
                        ImGui::PopStyleColor(1);
                        ImGui::SetItemTooltip(q.token.c_str());
                        break;

                    case 0:
                    case 1:
                        if (q.i == 0) {
                            q.model_id = q.token;
                        }
                        else if (q.i == 1 && count == 0) {
                            q.item_id = q.token;
                        }
                        else if (q.i == 1 && count == 1) {
                            break;
                        }
                        if (count == 0) {
                            for (size_t ia = 0; ia < blacklist.size(); ia++) {
                                if (blacklist[ia].find(q.model_id) != std::string::npos) {
                                    q.i = 5;
                                    break;
                                }
                            }
                            if (q.i == 5) {
                                break;
                            }
                        }
                        else if (!setting_print_blitems && q.i == 0) {
                            for (size_t ib = 0; ib < whitelist.size(); ib++) {
                                // if whitelist match blacklist trigger
                                if (whitelist[ib].find(q.model_id) != std::string::npos) {
                                    q.compare_table = true;
                                    break;
                                }
                            }
                            if (!q.compare_table) {
                                q.i = 5;
                                break;
                            }
                        }

                    default:
                        ImGui::TableSetColumnIndex(q.tcount++);
                        ImGui::TextUnformatted(q.token.c_str());
                        break;
                }
                q.startpos = q.endpos + 1;
                q.i++;
            }
            if (q.i != 6) {
                ImGui::TableSetColumnIndex(q.tcount++);
                if (count == 0) {
                    if (ImGui::Button("Add  ")) {
                        blacklist.push_back(tablecontent[a]);
                        SaveBlacklist();
                    }
                }
                else if (count == 1) {
                    if (ImGui::Button("Delete  ")) {
                        blacklist.erase(blacklist.begin() + a);
                        SaveBlacklist();
                    }
                }
                ImGui::TableNextRow();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        ImGui::PopID();
    }
}





void ToolboxUIPlugin::WhitelistItems()
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving()) {
        // const auto g = GW::GetGameContext(); // g->character
        whitelist.clear();
        for (auto bag_id = GW::Constants::Bag::Backpack; bag_id <= GW::Constants::Bag::Equipment_Pack; bag_id++) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            GW::ItemArray& items = bag->items;
            for (size_t i = 0, size = items.size(); i < size; i++) {
                // WriteChatF(GW::Chat::CHANNEL_GWCA1, L"T1");
                const auto item = static_cast<Item*>(items[i]);
                if (!item) {
                    continue;
                }

                if (item->GetIsIdentified()) {
                    continue;
                }

                if (item->IsGreen() || item->type == GW::Constants::ItemType::Minipet || item->type == GW::Constants::ItemType::Trophy || item->type == GW::Constants::ItemType::Rune_Mod || item->type == GW::Constants::ItemType::Scroll) {
                    continue;
                }

                if (!item->IsBlue() && !item->IsPurple() && !item->IsGold()) {
                    continue;
                }

                size_t start_pos;

                GW::UI::AsyncDecodeStr(item->complete_name_enc, item_name_buffer, sizeof(item_name_buffer) / sizeof(wchar_t));
                GW::UI::AsyncDecodeStr(item->info_string, item_info_buffer, sizeof(item_info_buffer) / sizeof(wchar_t));

                std::string w_name_str = WideToUtf8(item_name_buffer);
                std::string w_info_str = WideToUtf8(item_info_buffer);

                std::string decoded_s = std::regex_replace(w_info_str, reg, "");
                start_pos = w_name_str.find('>', 0);
                if (start_pos != std::string::npos) {
                    full_item_name = w_name_str.substr(start_pos + 1, w_name_str.length() - start_pos - 5);
                    rarity = w_name_str.substr(8, start_pos - 8);
                }
                std::string item_info = std::to_string(item->model_id) + ":" + std::to_string(item->item_id) + ":" + rarity + ":" + full_item_name + ":" + decoded_s;
                PushList(whitelist, item_info);

            }
        }
    }
}

GW::Item* GetIdentKit()
{
    const auto bags = GW::Items::GetBagArray();
    for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
        const auto bag = bags[bagIndex];
        if (!bag) continue;
        for (const auto item : bag->items) {
            if (item && static_cast<uint8_t>(item->type) == 29) {
                if (item->model_id == 5899 || item->model_id == 2989) {
                    return item;
                }
            }
        }
    }

    return nullptr;
}

GW::Item* GetSalvageKit()
{
    const auto bags = GW::Items::GetBagArray();
    for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
        const auto bag = bags[bagIndex];
        if (!bag) continue;
        for (const auto item : bag->items) {
            if (item && static_cast<uint8_t>(item->type) == 29) {
                if (item->model_id == 2992 || item->model_id == 2991 || item->model_id == 5900) {
                    return item;
                }
            }
        }
    }
    return nullptr;
}


void ToolboxUIPlugin::Update(float)

{
    if (clock() / 100 - time_delay > loop_speed) {
        time_delay = clock() / 100;

        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving()) {
            if (pending_item.size()) {
                if (new_pending_item_session) {
                    if (setting_debug_option) {
                        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : 0 : %d ; %d", init_listener, init_identify);
                    }
                    is_progress = "";

                    new_pending_item_session = false;
                    init_identify = true;
                    me = GW::Agents::GetControlledCharacter();
                }

                if (erase_item) {
                    pending_item.erase(pending_item.begin());
                    is_progress = "+ + +";

                    fallback = 0;
                    erase_item = false;
                    new_pending_item_session = true;
                }

                if (is_salvaging) {
                    if (setting_debug_option) {
                        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : 4 : Salvage...");
                    }
                    GW::UI::ButtonClick(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), 0x6, 0x64, 0x6));
                    current_salvage_session.salvage_item_id = 0;
                    GW::Items::SalvageMaterials();
                    is_salvaging = false;
                    erase_item = true;
                }

                if (init_salvaging) {
                    if (me->GetIsAlive()) {
                        const Item* skit = static_cast<Item*>(GetSalvageKit());
                        if (skit) {
                            GW::Items::SalvageStart(skit->item_id, pending_item[0].item_id);
                            init_salvaging = false;
                            is_salvaging = true;
                            if (setting_debug_option) {
                                WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : 3 : s_kit: %d ; item: %d", skit->item_id, pending_item[0]);
                            }
                        }
                        else {
                            WriteChatF(GW::Chat::CHANNEL_GWCA1, L"<c=@ItemBonus>S A L V I F Y</c> : <c=@ItemDull>Salvage kit consumed !</c>");

                            Detach_Salvage_Listener();
                            pending_item.clear();
                            new_pending_item_session = true;
                        }
                        init_salvaging = false;
                        is_progress = "+ + ";
                    }
                }

                if (init_salvage_popup) {
                    if (setting_auto_salvage) {
                        init_salvaging = true;
                        init_salvage_popup = false;
                    }
                    else {
                        const Item* item = static_cast<Item*>(GW::Items::GetItemById(pending_item[0].item_id));
                        GW::UI::AsyncDecodeStr(item->complete_name_enc, item_popup_name_buffer, sizeof(item_popup_name_buffer) / sizeof(wchar_t));
                        GW::UI::AsyncDecodeStr(item->info_string, item_popup_info_buffer, sizeof(item_popup_info_buffer) / sizeof(wchar_t));
                        salvage_popup = true;
                        init_salvage_popup = false;
                    }
                }
                


                if (init_identify) {
                    if (me->GetIsAlive()) {
                        const Item* ikit = static_cast<Item*>(GetIdentKit());

                        if (ikit) {
                            item_rarity = pending_item[0].item_rarity;
                            if (item_rarity == 1) {
                                bluect++;
                                progress_color = item_color_blue;
                            }
                            else if (item_rarity == 2) {
                                purplect++;
                                progress_color = item_color_purple;
                            }
                            else if (item_rarity == 3) {
                                goldct++;
                                progress_color = item_color_gold;
                            }
                            GW::Items::IdentifyItem(ikit->item_id, pending_item[0].item_id);

                            if (setting_salvage_module) {
                                init_salvage_popup = true;
                            }
                            else {
                                erase_item = true;
                            }
                            is_progress = "+ ";

                            if (setting_debug_option) {
                                WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : 1 : i_kit: %d ; item: %d :: salv = %d ; erase = %d", ikit->item_id, pending_item[0], init_salvaging, erase_item);
                            }
                        }
                        else {
                            WriteChatF(GW::Chat::CHANNEL_GWCA1, L"<c=@ItemBonus>S A L V I F Y</c> : <c=@ItemDull>Identification kit consumed !</c>");
                            
                            pending_item.clear();
                            new_pending_item_session = true;
                        }
                        init_identify = false;
                    }
                }

                if (setting_extended_debug_option && setting_debug_option) {
                    WriteChatF(
                        GW::Chat::CHANNEL_GWCA1, L"init_listener: %d; init_identify: %d; init_salvaging: %d ;is_salvaging: %d; erase_item: %d; new_pending_item_session: %d", init_listener, init_identify, init_salvaging, is_salvaging, erase_item,
                        new_pending_item_session
                    );
                }

                if (fallback >= 20) {
                    erase_item = true;
                    if (setting_debug_option) {
                        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : Fallback");
                    }
                }
                if (!salvage_popup) {
                    fallback++;
                }
            }
            else if (salvage_listeners_attached) {
                Detach_Salvage_Listener();
                is_progress = "";
                if (setting_ident_chatprint) {
                    ChatResult();
                }
            }
        }
        else if (pending_item.size()) {
            pending_item.clear();
            Detach_Salvage_Listener();
        }
    }
}

void ToolboxUIPlugin::Detach_Salvage_Listener()
{
    
    // reset variable
    salvage_listeners_attached = false;
    new_pending_item_session = true;
    erase_item = false;
    is_salvaging = false;
    init_salvaging = false;
    init_salvage_popup = false;
    init_identify = false;
    init_listener = false;
    status_salvage_listener = false;
    salvage_popup = false;
    salvage_session = false;

    if (setting_debug_option) {
        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"DEBUG_I4S : X : Detach Packet Listener");
    }
}





ToolboxUIPlugin::Item* ToolboxUIPlugin::IdentifyAll()
{

    // IMPORTED ITEM LOCKUP ITERATOR SEQUENCE FROM INVENTORYMANAGER.CPP ~ GWTOOLBOX
    rarid q;
    size_t start_slot = 0;
    bl_count = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;
    for (auto bag_id = start_bag_id; bag_id < GW::Constants::Bag::Equipment_Pack; bag_id++) {
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        size_t slot = start_slot;
        start_slot = 0;
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            found_bl_item = false;
            const auto item = static_cast<Item*>(items[i]);
            if (!item) {
                continue;
            }

            if (item->GetIsIdentified()) {
                continue;
            }

            if (item->IsGreen() || item->type == GW::Constants::ItemType::Minipet || item->type == GW::Constants::ItemType::Trophy || item->type == GW::Constants::ItemType::Rune_Mod || item->type == GW::Constants::ItemType::Scroll) {
                continue;
            }

            for (size_t ia = 0; ia < blacklist.size(); ++ia) {
                if (blacklist[ia].find(std::to_string(item->model_id)) != std::string::npos) {
                    found_bl_item = true;
                    bl_count++;
                    break;
                }
            }

            if (!found_bl_item) {
                if (item->IsBlue() && setting_ident_blue) {
                    q.item_rarity = 1;
                }
                else if (item->IsPurple() && setting_ident_purple) {
                    q.item_rarity = 2;
                }
                else if (item->IsGold() && setting_ident_gold) {
                    q.item_rarity = 3;
                }
                else {
                    continue;
                }
                // CHECK BL
                if (!found_bl_item) {
                    q.id++;
                    if (setting_salvage_module) {
                        q.item_id = item->item_id;
                        pending_item.push_back(q);
                    }
                    else {
                        const Item* kit = static_cast<Item*>(GetIdentKit());
                        if (!kit) {
                            WriteChatF(GW::Chat::CHANNEL_GWCA1, L"<c=@ItemBonus>S A L V I F Y</c> : <c=@ItemDull>Identification kit consumed !</c>");
                            return nullptr;
                        }
                        GW::Items::IdentifyItem(kit->item_id, item->item_id);
                    }
                }
            }
        }
    }


    if ((q.id >= 1)) {
        bluect = 0;
        purplect = 0;
        goldct = 0;
    }

    return nullptr;
}

void ToolboxUIPlugin::PushList(std::vector<std::string>& list, std::string text)
{
    // Just only to display "%"
    std::regex pattern("%");
    std::string new_s = std::regex_replace(text, pattern, "%%");
    list.push_back(new_s);
}

void ToolboxUIPlugin::LoadSettings(const wchar_t* folder)
{
    ini.LoadFile(GetSettingFile(folder).c_str());

    lock_move = ini.GetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
    lock_size = ini.GetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
    p_lock_move = ini.GetBoolValue(Name(), VAR_NAME(p_lock_move), p_lock_move);
    setting_ident_blue = ini.GetBoolValue(Name(), VAR_NAME(setting_ident_blue), setting_ident_blue);
    setting_ident_purple = ini.GetBoolValue(Name(), VAR_NAME(setting_ident_purple), setting_ident_purple);
    setting_ident_gold = ini.GetBoolValue(Name(), VAR_NAME(setting_ident_gold), setting_ident_gold);
    setting_button_counter = ini.GetBoolValue(Name(), VAR_NAME(setting_button_counter), setting_button_counter);
    setting_auto_salvage = ini.GetBoolValue(Name(), VAR_NAME(setting_auto_salvage), setting_auto_salvage);
    setting_salvage_module = ini.GetBoolValue(Name(), VAR_NAME(setting_salvage_module), setting_salvage_module);



    setting_debug_option = ini.GetBoolValue(Name(), VAR_NAME(setting_debug_option), setting_debug_option);
    setting_extended_debug_option = ini.GetBoolValue(Name(), VAR_NAME(setting_extended_debug_option), setting_extended_debug_option);

    setting_button_counter_fontsize = std::stof(ini.GetValue(Name(), VAR_NAME(setting_button_counter_fontsize), "0.9f"));
    setting_button_fontsize = std::stof(ini.GetValue(Name(), VAR_NAME(setting_button_fontsize), "1.15f"));
    setting_popup_button_scale = std::stof(ini.GetValue(Name(), VAR_NAME(setting_popup_button_scale), "1.1f"));

    col0[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col0_0), "1"));
    col0[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col0_1), "1"));
    col0[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col0_2), "1"));
    col0[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col0_3), "0.55f"));

    col1[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col1_0), "0.088f"));
    col1[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col1_1), "0.54f"));
    col1[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col1_2), "0.704f"));
    col1[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col1_3), "0.192f"));

    col2[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col2_0), "0"));
    col2[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col2_1), "0"));
    col2[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col2_2), "0"));
    col2[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col2_3), "0"));

    col_pop_false_0[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_0_0), "1"));
    col_pop_false_0[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_0_1), "1"));
    col_pop_false_0[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_0_2), "1"));
    col_pop_false_0[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_0_3), "1"));

    col_pop_false_1[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_1_0), "0.345f"));
    col_pop_false_1[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_1_1), "0"));
    col_pop_false_1[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_1_2), "0"));
    col_pop_false_1[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_1_3), "1"));

    col_pop_false_2[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_2_0), "0.33f"));
    col_pop_false_2[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_2_1), "0.33f"));
    col_pop_false_2[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_2_2), "0.33f"));
    col_pop_false_2[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_false_2_3), "0.098f"));

    col_pop_true_0[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_0_0), "1"));
    col_pop_true_0[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_0_1), "1"));
    col_pop_true_0[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_0_2), "1"));
    col_pop_true_0[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_0_3), "1"));

    col_pop_true_1[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_1_0), "0"));
    col_pop_true_1[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_1_1), "0.196f"));
    col_pop_true_1[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_1_2), "0.027f"));
    col_pop_true_1[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_1_3), "1"));

    col_pop_true_2[0] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_2_0), "0.33f"));
    col_pop_true_2[1] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_2_1), "0.33f"));
    col_pop_true_2[2] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_2_2), "0.33f"));
    col_pop_true_2[3] = std::stof(ini.GetValue(Name(), VAR_NAME(col_pop_true_2_3), "0.098f"));

    int i = 0;
    std::regex pattern("(?![A-z0-9 %()\'-:\"]).");
    while (true) {
        std::string var_blacklist = "blacklist_" + std::to_string(i);
        single_bl = ini.GetValue(Name(), var_blacklist.c_str(), "-1");
        if (single_bl != "-1") {
            if (single_bl != "0") {
                std::string b64d_blitem = base64_decode(single_bl);
                if (!b64d_blitem.empty()) {
                    blacklist.push_back(b64d_blitem);
                    if (std::regex_search(b64d_blitem, pattern)) {
                        SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0xFFFF6666);
                        WriteChatF(GW::Chat::CHANNEL_GWCA1, L"Warning: corrupted blacklist entry at index %i !", i);
                        SetMessageColor(GW::Chat::CHANNEL_GWCA1, 0);
                    }
                    i++;
                }
            }
        }
        else {
            break;
        }
    }

    if (setting_salvage_module) {
        text_button = "=- I4S ALL -=";
    }
    else {
        text_button = "= IDENTIFY ALL =";
    }

    std::copy(example_weapon_name.begin(), example_weapon_name.end(), item_popup_name_buffer);
    std::copy(example_weapon_info.begin(), example_weapon_info.end(), item_popup_info_buffer);
}

void ToolboxUIPlugin::SaveBlacklist()
{
    std::wstring wideString = docpath.wstring();
    const wchar_t* folder = wideString.c_str();

    int c = 0;
    while (true) {
        std::string var_blacklist = "blacklist_" + std::to_string(c++);
        single_bl = ini.GetValue(Name(), var_blacklist.c_str(), "-1");
        if (single_bl != "-1") {
            ini.Delete(Name(), var_blacklist.c_str(), true);
        }
        else {
            break;
        }
    }
    for (size_t i = 0; i < blacklist.size(); i++) {
        std::string var_blacklist = "blacklist_" + std::to_string(i);
        std::string b64_encode = base64_encode(blacklist[i]);
        ini.SetValue(Name(), var_blacklist.c_str(), b64_encode.c_str());
    }

    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}


void ToolboxUIPlugin::SaveSettings()
{

    if (!PathCreateDirectorySafe(docpath)) {
        WriteChatF(GW::Chat::CHANNEL_WARNING, L"Fail, something with the save directory goes wrong.");
        return;
    }

    ini.SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
    ini.SetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
    ini.SetBoolValue(Name(), VAR_NAME(p_lock_move), p_lock_move);
    ini.SetBoolValue(Name(), VAR_NAME(setting_ident_blue), setting_ident_blue);
    ini.SetBoolValue(Name(), VAR_NAME(setting_ident_purple), setting_ident_purple);
    ini.SetBoolValue(Name(), VAR_NAME(setting_ident_gold), setting_ident_gold);
    ini.SetBoolValue(Name(), VAR_NAME(setting_button_counter), setting_button_counter);
    ini.SetBoolValue(Name(), VAR_NAME(setting_auto_salvage), setting_auto_salvage);
    ini.SetBoolValue(Name(), VAR_NAME(setting_salvage_module), setting_salvage_module);
    ini.SetBoolValue(Name(), VAR_NAME(setting_debug_option), setting_debug_option);
    ini.SetBoolValue(Name(), VAR_NAME(setting_extended_debug_option), setting_extended_debug_option);

    ini.SetValue(Name(), VAR_NAME(setting_button_counter_fontsize), std::to_string(setting_button_counter_fontsize).c_str());
    ini.SetValue(Name(), VAR_NAME(setting_button_fontsize), std::to_string(setting_button_fontsize).c_str());
    ini.SetValue(Name(), VAR_NAME(setting_popup_button_scale), std::to_string(setting_popup_button_scale).c_str());

    ini.SetValue(Name(), VAR_NAME(col0_0), std::to_string(col0[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col0_1), std::to_string(col0[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col0_2), std::to_string(col0[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col0_3), std::to_string(col0[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col1_0), std::to_string(col1[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col1_1), std::to_string(col1[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col1_2), std::to_string(col1[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col1_3), std::to_string(col1[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col2_0), std::to_string(col2[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col2_1), std::to_string(col2[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col2_2), std::to_string(col2[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col2_3), std::to_string(col2[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_true_0_0), std::to_string(col_pop_true_0[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_0_1), std::to_string(col_pop_true_0[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_0_2), std::to_string(col_pop_true_0[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_0_3), std::to_string(col_pop_true_0[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_true_1_0), std::to_string(col_pop_true_1[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_1_1), std::to_string(col_pop_true_1[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_1_2), std::to_string(col_pop_true_1[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_1_3), std::to_string(col_pop_true_1[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_true_2_0), std::to_string(col_pop_true_2[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_2_1), std::to_string(col_pop_true_2[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_2_2), std::to_string(col_pop_true_2[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_true_2_3), std::to_string(col_pop_true_2[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_false_0_0), std::to_string(col_pop_false_0[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_0_1), std::to_string(col_pop_false_0[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_0_2), std::to_string(col_pop_false_0[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_0_3), std::to_string(col_pop_false_0[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_false_1_0), std::to_string(col_pop_false_1[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_1_1), std::to_string(col_pop_false_1[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_1_2), std::to_string(col_pop_false_1[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_1_3), std::to_string(col_pop_false_1[3]).c_str());

    ini.SetValue(Name(), VAR_NAME(col_pop_false_2_0), std::to_string(col_pop_false_2[0]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_2_1), std::to_string(col_pop_false_2[1]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_2_2), std::to_string(col_pop_false_2[2]).c_str());
    ini.SetValue(Name(), VAR_NAME(col_pop_false_2_3), std::to_string(col_pop_false_2[3]).c_str());

    SaveBlacklist();

}


