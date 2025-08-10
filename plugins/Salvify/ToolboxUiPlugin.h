#pragma once

#include <ToolboxPlugin.h>
// #include <utils/guiUtils.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>



namespace GW::Constants {
    enum class Rarity : uint8_t { White, Blue, Purple, Gold, Green };
}

class ToolboxUIPlugin : public ToolboxPlugin {
public:

    // Function - correspond with PluginModule.cpp 
    bool* GetVisiblePtr() override { return &plugin_visible; };
    const char* Name() const override { return "salvify"; }


    // Button and Popup Window Options
    bool show_closebutton = false;
    bool show_menubutton = false;
    bool show_title = false;
    bool can_collapse = false;
    bool can_close = false;
    bool is_resizable = true;
    bool is_movable = true;
    bool lock_size = true;
    bool lock_move = true;
    bool p_lock_move = false;

    // Example Weapon
    std::string example_weapon_name = "<c=@ItemRare>Vampiric Wintergreen Bow of Enchanting</c>";
    std::string example_weapon_info = "<c=@ItemBasic>Piercing Dmg : 15 - 15 </c>\n<c = @ItemRare>Inscription :\" Guided by Fate\"</c>\n<c=@ItemRare>Damage +15% </c><=@ItemDull>(while Enchanted)</c>\n<c=@ItemRare>Life Draining: 5</c>\n<c=@ItemRare>Health regeneration -1</c>\n<c=@ItemRare>Enchantments last 20% longer</c>\n<c=@ItemDull>Two-handed Longbow</c>\n<c=@ItemDull>Customized for Might Tyson</c>";
    
    // Settings
    bool setting_ident_blue = true;
    bool setting_ident_purple = true;
    bool setting_ident_gold = true;
    bool setting_button_counter = true;
    bool setting_auto_salvage = false;
    bool setting_salvage_module = false;
    bool setting_print_blitems = false;
    bool setting_ident_chatprint = false;
    float setting_button_hovereffect = 1.0f;
    float setting_button_counter_fontsize = 0.9f;
    float setting_button_fontsize = 1.0f;
    float setting_popup_button_scale = 1.1f;
    float setting_popup_title_fontsize = 1.3f;

        // Settings :: Debug Option
        bool setting_debug_option = false;
        bool setting_extended_debug_option = false;

    // Style Color as RGBA Floats.
    float col0[4] = {0.0f, 1.0f, 0.003659f, 1.0f};
    float col1[4] = {0.153705f, 0.153705f, 0.153705f, 0.500000f};
    float col2[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_true_0[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_true_1[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_true_2[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_false_0[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_false_1[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float col_pop_false_2[4] = {0.0f, 0.0f, 0.0f, 0.462745f};
    float blue[4] = {136.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f};

    ImVec4 item_color_blue = ImVec4(136.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f);
    ImVec4 item_color_purple = ImVec4(191.0f / 255.0f, 109.0f / 255.0f, 238.0f / 255.0f, 1.0f);
    ImVec4 item_color_gold = ImVec4(1.0f, 204.0f / 255.0f, 85.0f / 255.0f, 1.0f);
    ImVec4 color_white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 item_color_gray = ImVec4(0.557f, 0.553f, 0.549f, 1.000f);
    ImVec4 item_color_green = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
    ImVec4 color_warning = ImVec4(1.0f, 0.00f, 0.00f, 1.00f);
    ImVec4 text_color_whitegray = ImVec4(1.0f, 1.0f, 1.0f, 0.585f);

    // Path Settings
    std::filesystem::path computer_name;
    std::filesystem::path plugins = L"plugins";
    std::filesystem::path docpath;



private:
 
    struct Item : GW::Item {
        GW::ItemModifier* GetModifier(uint32_t identifier) const;
        GW::Constants::Rarity GetRarity() const;
        [[nodiscard]] bool GetIsIdentified() const { return (interaction & 1) != 0; }
        [[nodiscard]] bool IsBlue() const { return single_item_name && single_item_name[0] == 0xA3F; }
        [[nodiscard]] bool IsPurple() const { return (interaction & 0x400000) != 0; }
        [[nodiscard]] bool IsGreen() const { return (interaction & 0x10) != 0; }
        [[nodiscard]] bool IsGold() const { return (interaction & 0x20000) != 0; }
    }; 

    struct rarid {
        uint16_t id = 0;
        int8_t item_rarity = 0;
        uint32_t item_id = 0;
    };

    struct setting_whitelist {
        size_t startpos = 0;
        size_t endpos = 0;
        size_t i = 0;
        size_t length = 0;
        size_t tcount = 0;
        std::string token;
        std::string item_id;
        std::string model_id;
        bool compare_table = false;
    };

    struct format_item_description {
        size_t startpos = 0;
        size_t endpos = 0;
        size_t i = 0;
        size_t length = 0;
        size_t count = 0;
        std::string line;
        std::string format_text;
    };

    // Functions
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void Draw(IDirect3DDevice9*) override;
    void DrawSettings() override;
    void Update(float delta) override;
    void PopUp_Item();
    Item* IdentifyAll();
    void ChatResult();
    void Setting_WB_Table(std::vector<std::string>& tablecontent, int count);
    void PushList(std::vector<std::string>& list, std::string text);
    void Detach_Salvage_Listener();
    void WhitelistItems();
    void SaveBlacklist();
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings();

    std::vector<std::string> whitelist;
    std::vector<std::string> blacklist;
    std::vector<rarid> pending_item;

    const char* is_progress = "";
    const char* text_button = "= IDENTIFY ALL =";
    std::string full_item_name;
    std::string rarity;
    std::string single_bl;

    uint16_t fallback = 0;
    uint8_t bluect = 0;
    uint8_t purplect = 0;
    uint8_t goldct = 0;
    uint8_t loop_speed = 5; 
    int8_t item_rarity;
    int8_t salvage_counter = 3;

    size_t bl_count;

    ImVec4 progress_color;
    
    clock_t time_delay = clock() / 100;
    clock_t setting_time_delay = clock() / 100;
    clock_t popup_time_delay;
 

    GW::Packet::StoC::SalvageSession current_salvage_session{};
    const GW::AgentLiving* me = nullptr;

    bool salvage_popup = false;
    bool plugin_visible = true;
    bool salvage_listeners_attached = false;

    bool salvage_session_done = false;
    bool prepare_salvage_count = false;
    bool init_salvage_examplepopup = false;

    bool scale_item_title = false;

    bool new_pending_item_session = true;
    bool erase_item = false;
    bool is_salvaging = false;
    bool salvage_session = false;
    bool init_salvaging = false;
    bool init_salvage_popup = false;
    bool init_identify = false;
    bool init_listener = false;
    bool status_salvage_listener = false;
    bool is_open = false;
    bool is_popup_open;
    bool firststart = true;
    bool found_bl_item;

    // Some Buffers for AsyncDecode
    wchar_t item_name_buffer[256];
    wchar_t item_popup_name_buffer[256];
    wchar_t item_info_buffer[1024];
    wchar_t item_popup_info_buffer[512];







};
