#include "state.h"
#include "footprint.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <ctime>

#ifdef VIDYAOS_NATIVE
#include "platform_linux.h"
#endif

namespace fs = std::filesystem;

bool sensor_logging_enabled = false;
bool system_locked = false;
std::string lock_password = "vidya123";

std::string current_working_directory = "/";

std::map<std::string, std::string> virtual_files = {
    {"/readme.txt", "Welcome to Vidya OS!\nUse standard commands to explore."},
    {"/version.txt", "Vidya OS Version 1.0.0 (based on Zephyr RTOS)"}
};

Window windows[8] = {
    {"Terminal",      100, 60,  640, 400, false, 0, false, true, 0},
    {"File Manager",  160, 80,  740, 480, false, 1, false, true, 0},
    {"System Monitor",200, 100, 640, 400, false, 2, false, true, 0},
    {"Chrome Browser",120, 70,  860, 520, false, 3, false, true, 0},
    {"Settings",      180, 90,  760, 500, false, 4, false, true, 0},
    {"Control Panel", 160, 80,  720, 460, false, 5, false, true, 0},
    {"Text Editor",   140, 70,  780, 510, false, 6, false, true, 0},
    {"App Store",     130, 75,  820, 520, false, 7, false, true, 0}
};

bool start_menu_open = false;
bool is_dark_theme = true;
std::string selected_file = "";

int mouse_x = 160;
int mouse_y = 120;

std::vector<std::string> terminal_logs = {
    "Vidya OS Boot OK",
    "Welcome to Desktop"
};

std::vector<int> sensor_history = {20, 20, 20, 20, 20};
int current_sensor_value = 0;
std::string gui_input_buffer = "";

std::map<std::string, bool> apt_installed_packages = {
    {"chrome", false},
    {"neofetch", false},
    {"cmatrix", false},
    {"python", false},
    {"java", false},
    {"g++", false},
    {"gcc", false},
    {"nodejs", false}
};
std::string chrome_url = "google.com";
bool chrome_address_active = false;
bool cmatrix_active = false;
int gui_frame_counter = 0;

bool python_repl_active = false;
std::map<std::string, std::string> python_variables;
std::map<std::string, bool> java_compiled_classes;
std::map<std::string, std::string> compiled_binaries;

bool javascript_repl_active = false;
std::map<std::string, std::string> javascript_variables;

int telemetry_cpu_usage = 0;
int telemetry_ram_usage = 0;
std::string telemetry_json_output = "{}";

// --- Phase 1 Additions ---
int window_z_order[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// --- Phase 3 Networking ---
NetworkState net_state = {true, "VidyaNet", 4, false, "192.168.1.100"};
std::vector<std::string> available_networks = {"VidyaNet", "HomeWiFi", "CoffeeShop"};

// --- Phase 3 Multi-User & Security ---
std::string currentUser = "shri";
std::map<std::string, FileMeta> file_metadata;
bool system_logged_in = false;
std::string login_input_buffer = "";
std::string login_pass_buffer = "";
int login_selected_user_idx = 0;
std::map<std::string, std::string> user_passwords = {
    {"root", "e14cb9e5c0eeee0ea313a4e04fbd10aa17ac17aa33a3cad4bdfe74b87ca18ef8"}, // root123
    {"shri", "a74b0bf1256b2edd8b8987e3fa1d66060e7f630cd63bdb7def250b194caee6b2"}  // vidya123
};
bool firewall_allow_incoming = true;
std::map<std::string, bool> firewall_rules = {
    {"Terminal", true},
    {"Browser", true},
    {"App Store", true}
};

// --- Phase 3 Workspaces ---
int current_workspace = 0;
int settings_wallpapers[4] = {0, 1, 2, 3}; // different wallpapers per workspace

// --- Phase 5 OS features ---
bool is_tiling_mode = false;
std::vector<DownloadItem> browser_downloads;
std::vector<std::string> browser_bookmarks = {"google.com", "github.com", "wikipedia.org"};
std::vector<TermPane> term_panes = {
    {"Pane 1", {"Terminal 1 active. Ready."}, "", true},
    {"Pane 2", {"Terminal 2 active. Ready."}, "", false},
    {"Pane 3", {"Terminal 3 active. Ready."}, "", false}
};
bool editor_terminal_focused = false;
bool downloads_panel_open = false;
int active_term_pane_idx = 0;
std::string active_system_slot = "A";
std::string inactive_system_slot = "B";
bool slot_upgrade_pending = false;
bool apparmor_enabled = true;
std::vector<std::string> firewall_blocked_apps;
std::vector<IoTDevice> iot_devices = {
    {"Living Room Light", "light", "off"},
    {"Smart Thermostat", "thermostat", "21C"},
    {"Smart Plug", "switch", "on"}
};
bool voice_assistant_active = false;
std::string voice_assistant_input = "";
bool screen_reader_enabled = false;
std::string current_locale = "en_US";
bool bluetooth_enabled = true;
std::vector<std::string> bluetooth_devices = {"VidyaBuds", "Keyboard Pro", "MxMouse"};


// --- Phase 3 Themes Engine ---
Theme active_theme = {
    "dark",
    // window_bg colors:
    {0x030C1EFF, 0x050F22FF, 0x030D1BFF, 0x080C18FF, 0x040E20FF, 0x060F1EFF, 0x020B1CFF, 0x050F22FF},
    0x0A2055FF, // titlebar_bg
    0xD8F0FFFF, // titlebar_fg
    0x040D22EE, // taskbar_bg
    0xC8E8FFFF, // font_color
    0x0088FFFF, // accent
    12,         // font_size
    0           // wallpaper
};

std::vector<Theme> installed_themes = {
 active_theme,
 {
    "light",
    {0xECF4FFFF, 0xF0F7FFFF, 0xE8F3FFFF, 0xFFFFFFFF, 0xF2F8FFFF, 0xEDF5FFFF, 0xF5F9FFFF, 0xF0F7FFFF},
    0x1A5FBBFF,
    0xEEF6FFFF,
    0xD0E5FCEE,
    0x111133FF,
    0x0088FFFF,
    12,
    1
 },
 {
    "neon",
    {0x0A001AFF, 0x0F0022FF, 0x0A0015FF, 0x120020FF, 0x0D001CFF, 0x0E001FFF, 0x080018FF, 0x0F0022FF},
    0x440066FF,
    0xFF00FFFF,
    0x15002AEF,
    0x00FF66FF,
    0xFF00FFFF,
    12,
    2
 }
};

// --- Phase 3 App Store ---
int appstore_active_category = 0;
int appstore_installing_idx = -1;
int appstore_progress = 0;
std::vector<AppStoreItem> appstore_apps = {
    {"Chrome Web Browser", "chrome", "Surfing the local intranet websites", 4, "2.4 MB", 0x3366FFAA, "Utilities"},
    {"Neofetch System Info", "neofetch", "Linux-style system information display", 5, "120 KB", 0x00FF88AA, "Utilities"},
    {"CMatrix Matrix Code", "cmatrix", "Green matrix rain console animation", 4, "350 KB", 0x00AA00FF, "Games"},
    {"Python Interactive REPL", "python", "Interactive shell for scripting language", 5, "4.2 MB", 0xFFDD00AA, "Development"},
    {"Java Runtime & Compiler", "java", "Run and compile virtual java source files", 4, "8.5 MB", 0xEE3300AA, "Development"},
    {"G++ C++ Compiler", "g++", "Compile native-like virtual C++17 files", 5, "12.0 MB", 0x0088FFAA, "Development"},
    {"GCC C Compiler", "gcc", "Compile standard virtual C files in shell", 4, "10.0 MB", 0x0099FFAA, "Development"},
    {"Node.js Runtime & REPL", "nodejs", "JavaScript server-side run environment", 5, "9.1 MB", 0x33AA33AA, "Development"},
    {"Neon Visual Theme", "theme_neon", "A vibrant visual style theme pack", 5, "45 KB", 0xFF00FFAA, "Themes"}
};

// --- Phase 3 Browser Upgrade ---
std::vector<std::string> browser_history = {"vidyaos.local"};
int browser_history_idx = 0;
std::vector<WebLink> browser_links;

std::string file_manager_viewing_content = "";
std::string file_manager_viewing_title = "";
SDL_Window* sdl_window = nullptr;
int current_window_scale = 1;

// --- Phase 2 Dragging & Window Manager ---
int dragging_window_index = -1;
int drag_offset_x = 0;
int drag_offset_y = 0;

// --- Phase 2 Settings Customizer ---
int settings_active_tab = 0;
int settings_wallpaper_idx = 0;
int settings_accent_idx = 0;
int settings_font_size = 12;
bool wifi_enabled = true;
std::string wifi_ssid = "VidyaNet";
bool vpn_enabled = false;
int system_volume = 80;
bool system_muted = false;
int sound_device_idx = 0;
bool notifications_enabled = true;
std::vector<std::string> virtual_users = {"root", "shri"};
int update_check_progress = -1;
std::string last_update_check_time = "Never";

// --- Phase 2 Notification Centre ---
std::vector<Notification> notifications;
bool notifications_popup_open = false;

void push_notification(const std::string& msg) {
    if (!notifications_enabled) return;
    Notification n = {msg, (int)std::time(nullptr)};
    notifications.insert(notifications.begin(), n);
    if (notifications.size() > 20) {
        notifications.pop_back();
    }
    if (!system_muted) {
        printf("[PipeWire Alert] Playing sound 'notification.wav' at volume %d%%\n", system_volume);
        fflush(stdout);
    }
}

// --- Phase 2 Clipboard ---
std::string global_clipboard_path = "";

// --- Phase 2 Text Editor ---
std::string editor_text_content = "";
std::string editor_current_file = "";
int editor_cursor_pos = 0;
std::string editor_undo_buffer = "";

// --- Phase 2 System Monitor Scrolling Graphs ---
std::vector<int> cpu_history(20, 0);
std::vector<int> ram_history(20, 0);

// --- Phase 2 File Manager 2.0 tree & context menu ---
std::string file_manager_current_dir = "/";
std::vector<std::string> file_manager_expanded_dirs = {"/"};
bool file_manager_context_menu_open = false;
int file_manager_context_menu_x = 0;
int file_manager_context_menu_y = 0;
std::string file_manager_context_menu_target = "";
std::vector<std::string> footprint_added;
std::vector<std::string> footprint_removed;
std::vector<std::string> footprint_modified;
bool footprint_has_diff = false;

void bring_to_front(int index) {
    int pos = -1;
    for (int j = 0; j < 8; j++) {
        if (window_z_order[j] == index) {
            pos = j;
            break;
        }
    }
    if (pos != -1) {
        for (int j = pos; j < 7; j++) {
            window_z_order[j] = window_z_order[j + 1];
        }
        window_z_order[7] = index;
    }
}


std::string get_sandbox_root() {
    const char* env_val = getenv("VIDYAOS_SANDBOX_ROOT");
    if (env_val) {
        return std::string(env_val);
    }
    const char* home = getenv("HOME");
    if (home) {
        std::string cfg_path = std::string(home) + "/.vidyaos/config";
        std::ifstream cfg(cfg_path);
        if (cfg.is_open()) {
            std::string line;
            if (std::getline(cfg, line)) {
                size_t start = line.find_first_not_of(" \t\r\n");
                size_t end = line.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos) {
                    return line.substr(start, end - start + 1);
                }
            }
        }
    }
    if (home) {
        return std::string(home) + "/.vidyaos/rootfs";
    }
    return "./.vidyaos/rootfs";
}

std::string translate_path(const std::string& internal_path) {
#ifdef VIDYAOS_NATIVE
    return internal_path;
#else
    std::string root = get_sandbox_root();
    std::string ip = internal_path;
    if (ip.empty() || ip[0] != '/') {
        ip = "/" + ip;
    }
    if (ip.length() > 1 && ip.back() == '/') {
        ip.pop_back();
    }
    return root + ip;
#endif
}

std::string resolve_path(const std::string& path) {
    std::string clean_path;
    if (path.empty()) {
        clean_path = current_working_directory;
    } else if (path[0] == '/') {
        clean_path = path;
    } else {
        if (current_working_directory == "/") {
            clean_path = "/" + path;
        } else {
            clean_path = current_working_directory + "/" + path;
        }
    }
    std::vector<std::string> parts;
    std::stringstream ss(clean_path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
    }
    std::string result = "";
    for (const auto& p : parts) {
        result += "/" + p;
    }
    return result.empty() ? "/" : result;
}

void sync_sandbox_to_virtual_files() {
#ifdef VIDYAOS_NATIVE
    sync_sandbox_to_virtual_files_native();
    return;
#endif
    std::string root = get_sandbox_root();
    virtual_files.clear();
    
    // Check/create sandbox hierarchy
    if (!fs::exists(root)) {
        fs::create_directories(root);
        fs::create_directories(root + "/home");
        fs::create_directories(root + "/etc");
        fs::create_directories(root + "/usr");
        fs::create_directories(root + "/usr/bin");
        fs::create_directories(root + "/etc/apt/installed");
        fs::create_directories(root + "/var");
        fs::create_directories(root + "/var/run");
        
        std::ofstream(root + "/readme.txt") << "Welcome to Vidya OS!\nUse standard commands to explore.\n";
        std::ofstream(root + "/version.txt") << "Vidya OS Version 1.0.0 (based on Zephyr RTOS)\n";
    }

    fs::create_directories(root + "/var/www");
    fs::create_directories(root + "/etc/vidya");

    if (!fs::exists(root + "/var/www/index.html")) {
        std::ofstream(root + "/var/www/index.html") << "<h1>VidyaOS Home</h1>\n<p>Welcome to the <b>VidyaOS</b> local intranet portal. Explore services below:</p>\n<ul>\n  <li><a href=\"news.html\">Local News</a> - Stay updated on OS developments.</li>\n  <li><a href=\"mail.html\">Mail Client</a> - Check your system inbox.</li>\n  <li><a href=\"store.html\">Flathub Store</a> - Manage apps.</li>\n</ul>\n<br>\n<i>System status: Online</i>\n";
    }
    if (!fs::exists(root + "/var/www/news.html")) {
        std::ofstream(root + "/var/www/news.html") << "<h1>VidyaOS News</h1>\n<p><b>Phase 5 is officially live!</b> The OS now supports native resolution, TrueType vector fonts, and unified settings.</p>\n<p>Read about other improvements:</p>\n<ul>\n  <li>Aero-snapping visual helper preview</li>\n  <li>AppArmor sandboxing and nftables firewall</li>\n  <li>Multiplexed bottom terminal in IDE</li>\n</ul>\n<br>\n<a href=\"index.html\">&larr; Back Home</a>\n";
    }
    if (!fs::exists(root + "/var/www/mail.html")) {
        std::ofstream(root + "/var/www/mail.html") << "<h1>Inbox (0)</h1>\n<p>You have no new notifications or messages.</p>\n<br>\n<a href=\"index.html\">&larr; Back Home</a>\n";
    }
    if (!fs::exists(root + "/var/www/store.html")) {
        std::ofstream(root + "/var/www/store.html") << "<h1>Flathub Store Simulator</h1>\n<p>Download and sandbox Flatpak packages directly:</p>\n<ul>\n  <li><a href=\"download:vscode\">VS Code IDE</a> (Developer tools)</li>\n  <li><a href=\"download:vlc\">VLC Player</a> (Media tools)</li>\n  <li><a href=\"download:firefox\">Firefox</a> (Utilities)</li>\n  <li><a href=\"download:gimp\">GIMP Editor</a> (Graphics)</li>\n</ul>\n<br>\n<a href=\"index.html\">&larr; Back Home</a>\n";
    }
    if (!fs::exists(root + "/etc/vidya/appstore.json")) {
        std::ofstream(root + "/etc/vidya/appstore.json") << "[\n"
            "  {\n"
            "    \"name\": \"Chrome Web Browser\",\n"
            "    \"package_name\": \"chrome\",\n"
            "    \"desc\": \"Surfing the local intranet websites\",\n"
            "    \"rating\": 4,\n"
            "    \"size\": \"2.4 MB\",\n"
            "    \"color\": \"0x3366FFAA\",\n"
            "    \"category\": \"Utilities\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"Neofetch System Info\",\n"
            "    \"package_name\": \"neofetch\",\n"
            "    \"desc\": \"Linux-style system information display\",\n"
            "    \"rating\": 5,\n"
            "    \"size\": \"120 KB\",\n"
            "    \"color\": \"0x00FF88AA\",\n"
            "    \"category\": \"Utilities\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"CMatrix Matrix Code\",\n"
            "    \"package_name\": \"cmatrix\",\n"
            "    \"desc\": \"Green matrix rain console animation\",\n"
            "    \"rating\": 4,\n"
            "    \"size\": \"350 KB\",\n"
            "    \"color\": \"0x00AA00FF\",\n"
            "    \"category\": \"Games\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"Python Interactive REPL\",\n"
            "    \"package_name\": \"python\",\n"
            "    \"desc\": \"Interactive shell for scripting language\",\n"
            "    \"rating\": 5,\n"
            "    \"size\": \"4.2 MB\",\n"
            "    \"color\": \"0xFFDD00AA\",\n"
            "    \"category\": \"Development\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"Java Runtime & Compiler\",\n"
            "    \"package_name\": \"java\",\n"
            "    \"desc\": \"Run and compile virtual java source files\",\n"
            "    \"rating\": 4,\n"
            "    \"size\": \"8.5 MB\",\n"
            "    \"color\": \"0xEE3300AA\",\n"
            "    \"category\": \"Development\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"G++ C++ Compiler\",\n"
            "    \"package_name\": \"g++\",\n"
            "    \"desc\": \"Compile native-like virtual C++17 files\",\n"
            "    \"rating\": 5,\n"
            "    \"size\": \"12.0 MB\",\n"
            "    \"color\": \"0x0088FFAA\",\n"
            "    \"category\": \"Development\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"GCC C Compiler\",\n"
            "    \"package_name\": \"gcc\",\n"
            "    \"desc\": \"Compile standard virtual C files in shell\",\n"
            "    \"rating\": 4,\n"
            "    \"size\": \"10.0 MB\",\n"
            "    \"color\": \"0x0099FFAA\",\n"
            "    \"category\": \"Development\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"Node.js Runtime & REPL\",\n"
            "    \"package_name\": \"nodejs\",\n"
            "    \"desc\": \"JavaScript server-side run environment\",\n"
            "    \"rating\": 5,\n"
            "    \"size\": \"9.1 MB\",\n"
            "    \"color\": \"0x33AA33AA\",\n"
            "    \"category\": \"Development\"\n"
            "  },\n"
            "  {\n"
            "    \"name\": \"Neon Visual Theme\",\n"
            "    \"package_name\": \"theme_neon\",\n"
            "    \"desc\": \"A vibrant visual style theme pack\",\n"
            "    \"rating\": 5,\n"
            "    \"size\": \"45 KB\",\n"
            "    \"color\": \"0xFF00FFAA\",\n"
            "    \"category\": \"Themes\"\n"
            "  }\n"
            "]\n";
    }
    if (!fs::exists(root + "/etc/vidya/settings.json")) {
        std::ofstream(root + "/etc/vidya/settings.json") << "{\n"
            "  \"theme\": \"dark\",\n"
            "  \"scale\": 1,\n"
            "  \"wallpaper\": 0,\n"
            "  \"accent\": 0,\n"
            "  \"wifi\": true,\n"
            "  \"volume\": 100,\n"
            "  \"vpn\": false,\n"
            "  \"apparmor\": true,\n"
            "  \"screen_reader\": false,\n"
            "  \"locale\": \"en_US\",\n"
            "  \"bluetooth\": false,\n"
            "  \"active_slot\": \"A\",\n"
            "  \"inactive_slot\": \"B\",\n"
            "  \"upgrade_pending\": false\n"
            "}\n";
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            std::string path = entry.path().string();
            std::string rel_path = path.substr(root.length());
            if (rel_path.empty()) continue;
            if (rel_path[0] != '/') rel_path = "/" + rel_path;
            
            if (entry.is_directory()) {
                virtual_files[rel_path] = "<dir>";
            } else if (entry.is_regular_file()) {
                std::ifstream file(path, std::ios::binary);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    virtual_files[rel_path] = content;
                } else {
                    virtual_files[rel_path] = "";
                }
            }
        }
    } catch (...) {}
}

void load_appstore_from_json() {
    std::string path = "/etc/vidya/appstore.json";
    std::string rpath = translate_path(path);
    std::ifstream in(rpath);
    if (!in.is_open()) return;

    appstore_apps.clear();
    std::string line;
    AppStoreItem item;
    bool in_item = false;
    while (std::getline(in, line)) {
        if (line.find("{") != std::string::npos) {
            item = {};
            in_item = true;
        }
        else if (line.find("}") != std::string::npos || line.find("},") != std::string::npos) {
            if (in_item) {
                appstore_apps.push_back(item);
                in_item = false;
            }
        }
        else if (in_item) {
            size_t colon = line.find(":");
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                
                size_t k_start = key.find("\"");
                size_t k_end = key.rfind("\"");
                if (k_start != std::string::npos && k_end != std::string::npos && k_end > k_start) {
                    key = key.substr(k_start + 1, k_end - k_start - 1);
                }
                
                size_t v_start = val.find("\"");
                size_t v_end = val.rfind("\"");
                std::string val_str = "";
                if (v_start != std::string::npos && v_end != std::string::npos && v_end > v_start) {
                    val_str = val.substr(v_start + 1, v_end - v_start - 1);
                } else {
                    size_t first = val.find_first_not_of(" \t\r\n");
                    size_t last = val.find_last_not_of(" \t\r\n,");
                    if (first != std::string::npos && last != std::string::npos) {
                        val_str = val.substr(first, last - first + 1);
                    }
                }
                
                if (key == "name") item.name = val_str;
                else if (key == "package_name") item.package_name = val_str;
                else if (key == "desc") item.desc = val_str;
                else if (key == "rating") item.rating = atoi(val_str.c_str());
                else if (key == "size") item.size = val_str;
                else if (key == "color") {
                    if (val_str.rfind("0x", 0) == 0) {
                        item.color = std::stoul(val_str, nullptr, 16);
                    } else {
                        item.color = std::stoul(val_str);
                    }
                }
                else if (key == "category") item.category = val_str;
            }
        }
    }
    in.close();
}

void save_settings_to_file() {
    std::string path = "/etc/vidya/settings.json";
    std::string rpath = translate_path(path);
    try {
        fs::create_directories(fs::path(rpath).parent_path());
        std::ofstream out(rpath);
        if (out.is_open()) {
            out << "{\n";
            out << "  \"theme\": \"" << (is_dark_theme ? "dark" : "light") << "\",\n";
            out << "  \"scale\": " << current_window_scale << ",\n";
            out << "  \"wallpaper\": " << settings_wallpaper_idx << ",\n";
            out << "  \"accent\": " << settings_accent_idx << ",\n";
            out << "  \"wifi\": " << (wifi_enabled ? "true" : "false") << ",\n";
            out << "  \"volume\": " << system_volume << ",\n";
            out << "  \"vpn\": " << (vpn_enabled ? "true" : "false") << ",\n";
            out << "  \"apparmor\": " << (apparmor_enabled ? "true" : "false") << ",\n";
            out << "  \"screen_reader\": " << (screen_reader_enabled ? "true" : "false") << ",\n";
            out << "  \"locale\": \"" << current_locale << "\",\n";
            out << "  \"bluetooth\": " << (bluetooth_enabled ? "true" : "false") << ",\n";
            out << "  \"active_slot\": \"" << active_system_slot << "\",\n";
            out << "  \"inactive_slot\": \"" << inactive_system_slot << "\",\n";
            out << "  \"upgrade_pending\": " << (slot_upgrade_pending ? "true" : "false") << "\n";
            out << "}\n";
            out.close();
            sync_sandbox_to_virtual_files();
            mft.notify_change(path);
            mft.update(path);
        }
    } catch (...) {}
}

void load_settings_from_file() {
    std::string path = "/etc/vidya/settings.json";
    std::string rpath = translate_path(path);
    std::ifstream in(rpath);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("\"theme\"") != std::string::npos) {
                is_dark_theme = (line.find("dark") != std::string::npos);
            }
            else if (line.find("\"scale\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    int sc = atoi(line.substr(colon + 1).c_str());
                    if (sc >= 1 && sc <= 4) {
                        current_window_scale = sc;
                    }
                }
            }
            else if (line.find("\"wallpaper\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    settings_wallpaper_idx = atoi(line.substr(colon + 1).c_str());
                }
            }
            else if (line.find("\"accent\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    settings_accent_idx = atoi(line.substr(colon + 1).c_str());
                }
            }
            else if (line.find("\"wifi\"") != std::string::npos) {
                wifi_enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("\"volume\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    system_volume = atoi(line.substr(colon + 1).c_str());
                }
            }
            else if (line.find("\"vpn\"") != std::string::npos) {
                vpn_enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("\"apparmor\"") != std::string::npos) {
                apparmor_enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("\"screen_reader\"") != std::string::npos) {
                screen_reader_enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("\"locale\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    size_t q_start = line.find('"', colon);
                    if (q_start != std::string::npos) {
                        size_t q_end = line.find('"', q_start + 1);
                        if (q_end != std::string::npos) {
                            current_locale = line.substr(q_start + 1, q_end - q_start - 1);
                        }
                    }
                }
            }
            else if (line.find("\"bluetooth\"") != std::string::npos) {
                bluetooth_enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("\"active_slot\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    size_t q_start = line.find('"', colon);
                    if (q_start != std::string::npos) {
                        size_t q_end = line.find('"', q_start + 1);
                        if (q_end != std::string::npos) {
                            active_system_slot = line.substr(q_start + 1, q_end - q_start - 1);
                        }
                    }
                }
            }
            else if (line.find("\"inactive_slot\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    size_t q_start = line.find('"', colon);
                    if (q_start != std::string::npos) {
                        size_t q_end = line.find('"', q_start + 1);
                        if (q_end != std::string::npos) {
                            inactive_system_slot = line.substr(q_start + 1, q_end - q_start - 1);
                        }
                    }
                }
            }
            else if (line.find("\"upgrade_pending\"") != std::string::npos) {
                slot_upgrade_pending = (line.find("true") != std::string::npos);
            }
        }
        in.close();
    }
    load_appstore_from_json();
}

void handle_desktop_click(int x, int y, bool is_double_click) {
    int screen_w = 1280, screen_h = 720;
    if (sdl_window) {
        SDL_GetWindowSize(sdl_window, &screen_w, &screen_h);
    }

    if (!system_logged_in) {
        int cx = screen_w / 2 - 200;
        int cy = screen_h / 2 - 230;
        
        // User tab selection
        if (y >= cy + 168 && y <= cy + 196) {
            if (x >= screen_w / 2 - 104 && x <= screen_w / 2 - 4) {
                login_selected_user_idx = 0;
            } else if (x >= screen_w / 2 + 4 && x <= screen_w / 2 + 104) {
                login_selected_user_idx = 1;
            }
        }
        
        // Sign In button hit area
        if (x >= cx + 36 && x <= cx + 364 && y >= cy + 332 && y <= cy + 376) {
            std::string user = (login_selected_user_idx == 0) ? "root" : "shri";
            if (sha256(login_pass_buffer) == user_passwords[user]) {
                currentUser = user;
                system_logged_in = true;
                current_working_directory = (user == "root") ? "/root" : "/home/" + user;
                fs::create_directories(translate_path(current_working_directory));
                set_default_metadata(current_working_directory, true);
                push_notification("Logged in as " + user);
                login_pass_buffer.clear();
            } else {
                push_notification("Incorrect passcode");
                login_pass_buffer.clear();
            }
        }
        return;
    }

    // If context menu is open, handle its click and close it
    if (file_manager_context_menu_open) {
        file_manager_context_menu_open = false;
        int cx = file_manager_context_menu_x;
        int cy = file_manager_context_menu_y;
        if (x >= cx && x <= cx + 180 && y >= cy && y <= cy + 180) {
            int opt = (y - (cy + 8)) / 32;
            if (opt >= 0 && opt < 5) {
                std::string target = file_manager_context_menu_target;
                if (opt == 0) { // Open (in Editor)
                    if (virtual_files.count(target) && virtual_files[target] != "<dir>") {
                        editor_current_file = target;
                        editor_text_content = virtual_files[target];
                        editor_cursor_pos = editor_text_content.length();
                        windows[6].open = true;
                        windows[6].minimized = false;
                        bring_to_front(6);
                    }
                }
                else if (opt == 1) { // Copy
                    global_clipboard_path = target;
                }
                else if (opt == 2) { // Paste
                    if (!global_clipboard_path.empty() && virtual_files.count(global_clipboard_path)) {
                        std::string filename = global_clipboard_path.substr(global_clipboard_path.find_last_of('/'));
                        std::string dest = file_manager_current_dir;
                        if (dest.back() != '/') dest += "/";
                        dest += filename;
                        
                        std::string rsrc = translate_path(global_clipboard_path);
                        std::string rdest = translate_path(dest);
                        try {
                            fs::copy_file(rsrc, rdest, fs::copy_options::overwrite_existing);
                            sync_sandbox_to_virtual_files();
                            mft.notify_change(dest);
                            mft.update(dest);
                            push_notification("Pasted file: " + dest);
                        } catch(...) {}
                    }
                }
                else if (opt == 3) { // Delete
                    if (virtual_files.count(target)) {
                        std::string rpath = translate_path(target);
                        try {
                            if (virtual_files[target] == "<dir>") {
                                fs::remove_all(rpath);
                            } else {
                                fs::remove(rpath);
                            }
                            sync_sandbox_to_virtual_files();
                            selected_file = "";
                            mft.notify_change(target);
                            mft.update(target);
                            push_notification("Deleted file: " + target);
                        } catch(...) {}
                    }
                }
                else if (opt == 4) { // Rename
                    if (virtual_files.count(target)) {
                        std::string dest = target;
                        size_t dot = dest.find_last_of('.');
                        if (dot != std::string::npos) {
                            dest.insert(dot, "_renamed");
                        } else {
                            dest += "_renamed";
                        }
                        std::string rsrc = translate_path(target);
                        std::string rdest = translate_path(dest);
                        try {
                            fs::rename(rsrc, rdest);
                            sync_sandbox_to_virtual_files();
                            mft.notify_change(target);
                            mft.notify_change(dest);
                            mft.update(target);
                            mft.update(dest);
                            selected_file = dest;
                            push_notification("Renamed file: " + dest);
                        } catch(...) {}
                    }
                }
            }
        }
        return;
    }

    // Toggle Notification center visibility
    if (notifications_popup_open) {
        notifications_popup_open = false;
        int px = screen_w - 320 - 12;
        int py = screen_h - TASKBAR_H - std::min(380, 80 + (int)notifications.size() * 64) - 8;
        if (x >= px && x <= screen_w - 12 && y >= py && y <= screen_h - TASKBAR_H - 8) {
            if (x >= px + 320 - 72 && x <= px + 320 - 12 && y >= py + 10 && y <= py + 34) {
                notifications.clear();
            }
        }
        return;
    }

    // 1. Check if Start Menu is open
    if (start_menu_open) {
        start_menu_open = false; // clicking anywhere closes the start menu
        int my = screen_h - TASKBAR_H - 440 - 8;
        int mx = 16;
        if (x >= mx && x <= mx + 360 && y >= my && y <= my + 440) {
            int ry = y - my;
            if (ry >= 88 && ry < 88 + 8 * 34) {
                int app_id = (ry - 88) / 34;
                if (app_id >= 0 && app_id < 8) {
                    if (app_id == 3 && !apt_installed_packages["chrome"]) {
                        // Chrome not installed
                    } else {
                        std::string app_name = "";
                        if (app_id == 0) app_name = "Terminal";
                        else if (app_id == 3) app_name = "Browser";
                        else if (app_id == 7) app_name = "App Store";
                        if (!app_name.empty() && !firewall_rules[app_name]) {
                            push_notification("[Security] Access blocked by AppArmor/nftables policy");
                            return;
                        }

                        windows[app_id].open = !windows[app_id].open;
                        windows[app_id].minimized = false;
                        if (windows[app_id].open) bring_to_front(app_id);
                    }
                }
            }
            else if (ry >= 440 - 48 && ry < 440) {
                if (x >= mx + 20 && x <= mx + 100) {
                    system_locked = true;
                }
                else if (x >= mx + 110 && x <= mx + 240) {
                    exit(0);
                }
            }
        }
        return;
    }

    // 2. Check Taskbar triggers
    if (y >= screen_h - TASKBAR_H) {
        // Start button (x: 8..100)
        if (x >= 8 && x <= 100) {
            start_menu_open = !start_menu_open;
            return;
        }
        // Workspace switcher (x: 108..196)
        if (x >= 108 && x <= 196) {
            int ws = (x - 108) / 22;
            if (ws >= 0 && ws <= 3) {
                current_workspace = ws;
                push_notification("Switched to Workspace " + std::to_string(ws));
            }
            return;
        }
        
        // Centered Dock clicks
        int total_w = 8 * 32 + 7 * 12;
        int start_x = screen_w / 2 - total_w / 2;
        for (int i = 0; i < 8; ++i) {
            int ix = start_x + i * (32 + 12);
            if (x >= ix && x <= ix + 32) {
                if (i == 3 && !apt_installed_packages["chrome"]) return;
                
                std::string app_name = "";
                if (i == 0) app_name = "Terminal";
                else if (i == 3) app_name = "Browser";
                else if (i == 7) app_name = "App Store";
                if (!app_name.empty() && !firewall_rules[app_name]) {
                    push_notification("[Security] Access blocked by AppArmor/nftables policy");
                    return;
                }

                if (windows[i].open) {
                    if (is_focused_window(i)) {
                        windows[i].minimized = true;
                    } else {
                        windows[i].minimized = false;
                        bring_to_front(i);
                    }
                } else {
                    windows[i].open = true;
                    windows[i].minimized = false;
                    bring_to_front(i);
                }
                return;
            }
        }
        
        // System tray clicks
        int sx2 = screen_w - 220;
        // Wifi click
        if (x >= sx2 + 4 && x <= sx2 + 24) {
            wifi_enabled = !wifi_enabled;
            net_state.wifiOn = wifi_enabled;
            push_notification(wifi_enabled ? "Wi-Fi Connected" : "Wi-Fi Disconnected");
            return;
        }
        // Volume click
        if (x >= sx2 + 28 && x <= sx2 + 84) {
            system_muted = !system_muted;
            push_notification(system_muted ? "Volume Muted" : "Volume Unmuted");
            return;
        }
        // Notification bell click
        if (x >= sx2 + 85 && x <= sx2 + 120) {
            notifications_popup_open = !notifications_popup_open;
            return;
        }
        // Clock click
        if (x >= screen_w - 90) {
            notifications_popup_open = !notifications_popup_open;
            return;
        }
        return;
    }

    // 3. Find which window was clicked (from top to bottom in Z-order)
    int clicked_window_index = -1;
    for (int z = 7; z >= 0; z--) {
        int i = window_z_order[z];
        if (!windows[i].open || windows[i].minimized || windows[i].workspace != current_workspace) continue;
        int wx = windows[i].x;
        int wy = windows[i].y;
        int ww = windows[i].w;
        int wh = windows[i].h;
        if (x >= wx && x <= wx + ww && y >= wy && y <= wy + wh) {
            clicked_window_index = i;
            bring_to_front(i);
            break;
        }
    }

    // 4. If a window was clicked, handle its internal/dragging actions
    if (clicked_window_index != -1) {
        int i = clicked_window_index;
        int wx = windows[i].x;
        int wy = windows[i].y;
        int ww = windows[i].w;
        int wh = windows[i].h;

        int btn_cy = wy + TITLEBAR_H / 2;
        // Close Button
        int dist_close = (x - (wx + 18)) * (x - (wx + 18)) + (y - btn_cy) * (y - btn_cy);
        if (dist_close <= 8 * 8) {
            windows[i].open = false;
            if (i == 3) chrome_address_active = false;
            return;
        }

        // Minimize Button
        int dist_min = (x - (wx + 42)) * (x - (wx + 42)) + (y - btn_cy) * (y - btn_cy);
        if (dist_min <= 8 * 8) {
            windows[i].minimized = true;
            return;
        }

        // Title Bar Dragging
        if (x >= wx + 80 && x < wx + ww && y >= wy && y <= wy + TITLEBAR_H) {
            dragging_window_index = i;
            drag_offset_x = x - wx;
            drag_offset_y = y - wy;
            return;
        }

        int ca_x = wx + 1;
        int ca_y = wy + TITLEBAR_H + 1;
        int ca_w = ww - 2;
        int ca_h = wh - TITLEBAR_H - 1;

        // Terminal Window (windows[0])
        if (i == 0) {
            return;
        }

        // File Manager Window (windows[1])
        if (i == 1) {
            // Check Category Sidebar
            if (x >= ca_x && x <= ca_x + 180) {
                int row = (y - (ca_y + 28)) / 28;
                std::vector<std::string> tree_paths = {"/", "/home", "/etc", "/usr", "/var", "/etc/apt"};
                if (row >= 0 && row < (int)tree_paths.size()) {
                    file_manager_current_dir = tree_paths[row];
                    selected_file = "";
                }
                return;
            }

            // Check Text viewer mode back button click
            if (!file_manager_viewing_content.empty()) {
                if (x >= ca_x + 16 && x <= ca_x + 96 && y >= ca_y + ca_h - 38 && y <= ca_y + ca_h - 10) {
                    file_manager_viewing_content = "";
                }
                return;
            }

            // Check Right Pane clicks
            if (x >= ca_x + 180 && x <= ca_x + ca_w) {
                // New File button
                if (x >= ca_x + ca_w - 90 && x <= ca_x + ca_w - 10 && y >= ca_y + 6 && y <= ca_y + 30) {
                    std::string dir = file_manager_current_dir;
                    if (dir.back() != '/') dir += "/";
                    std::string base = dir + "new_file.txt";
                    int count = 1;
                    while (virtual_files.count(base)) {
                        base = dir + "new_file_" + std::to_string(count) + ".txt";
                        count++;
                    }
                    std::string rpath = translate_path(base);
                    std::ofstream out(rpath);
                    if (out.is_open()) {
                        out << "Hello world! Real File Manager creation.";
                        out.close();
                    }
                    sync_sandbox_to_virtual_files();
                    mft.notify_change(base);
                    mft.update(base);
                    selected_file = base;
                    push_notification("Created file: " + base);
                    return;
                }
                
                // Open button
                if (!selected_file.empty()) {
                    if (x >= ca_x + 182 + 16 && x <= ca_x + 182 + 96 && y >= ca_y + ca_h - 30 && y <= ca_y + ca_h - 6) {
                        if (virtual_files.count(selected_file) && virtual_files[selected_file] != "<dir>") {
                            file_manager_viewing_title = selected_file.substr(selected_file.find_last_of('/') + 1);
                            file_manager_viewing_content = virtual_files[selected_file];
                        }
                        return;
                    }
                    // Delete button
                    else if (x >= ca_x + 182 + 104 && x <= ca_x + 182 + 184 && y >= ca_y + ca_h - 30 && y <= ca_y + ca_h - 6) {
                        std::string rpath = translate_path(selected_file);
                        std::string removed_path = selected_file;
                        if (virtual_files[selected_file] == "<dir>") {
                            fs::remove_all(rpath);
                        } else {
                            fs::remove(rpath);
                        }
                        sync_sandbox_to_virtual_files();
                        selected_file = "";
                        mft.notify_change(removed_path);
                        mft.update(removed_path);
                        push_notification("Deleted file: " + removed_path);
                        return;
                    }
                }

                // File Item Selection
                int click_idx = (y - (ca_y + 46)) / 28;
                if (click_idx >= 0) {
                    std::vector<std::pair<std::string, std::string>> result;
                    for (const auto& [name, content] : virtual_files) {
                        if (name == file_manager_current_dir) continue;
                        if (file_manager_current_dir == "/") {
                            if (name.find('/', 1) == std::string::npos) {
                                result.push_back({name, content});
                            }
                        } else {
                            if (name.rfind(file_manager_current_dir + "/", 0) == 0) {
                                std::string sub = name.substr(file_manager_current_dir.length() + 1);
                                if (sub.find('/') == std::string::npos) {
                                    result.push_back({name, content});
                                }
                            }
                        }
                    }
                    if (click_idx < (int)result.size()) {
                        selected_file = result[click_idx].first;
                        if (is_double_click) {
                            if (result[click_idx].second != "<dir>") {
                                file_manager_viewing_title = selected_file.substr(selected_file.find_last_of('/') + 1);
                                file_manager_viewing_content = result[click_idx].second;
                            }
                        }
                    }
                }
            }
            return;
        }

        // Chrome Browser (windows[3])
        if (i == 3) {
            // Check Back button
            if (x >= ca_x + 8 && x <= ca_x + 38 && y >= ca_y + 8 && y <= ca_y + 36) {
                if (browser_history_idx > 0) {
                    browser_history_idx--;
                    chrome_url = browser_history[browser_history_idx];
                }
                return;
            }
            // Check Forward button
            else if (x >= ca_x + 44 && x <= ca_x + 74 && y >= ca_y + 8 && y <= ca_y + 36) {
                if (browser_history_idx < (int)browser_history.size() - 1) {
                    browser_history_idx++;
                    chrome_url = browser_history[browser_history_idx];
                }
                return;
            }
            // Check Downloads button
            else if (x >= ca_x + ca_w - 48 && x <= ca_x + ca_w - 12 && y >= ca_y + 8 && y <= ca_y + 36) {
                downloads_panel_open = !downloads_panel_open;
                return;
            }
            // Check Address bar
            else if (x >= ca_x + 82 && x <= ca_x + ca_w - 58 && y >= ca_y + 8 && y <= ca_y + 36) {
                chrome_address_active = true;
                return;
            } else {
                chrome_address_active = false;
            }

            // Check Bookmarks Bar clicks (y: ca_y + 44 .. ca_y + 68)
            if (y >= ca_y + 44 && y <= ca_y + 68) {
                if (x >= ca_x + 10 && x <= ca_x + 100) {
                    chrome_url = "google.com";
                    browser_history.push_back(chrome_url);
                    browser_history_idx = browser_history.size() - 1;
                }
                else if (x >= ca_x + 110 && x <= ca_x + 200) {
                    chrome_url = "github.com";
                    browser_history.push_back(chrome_url);
                    browser_history_idx = browser_history.size() - 1;
                }
                else if (x >= ca_x + 210 && x <= ca_x + 310) {
                    chrome_url = "wikipedia.org";
                    browser_history.push_back(chrome_url);
                    browser_history_idx = browser_history.size() - 1;
                }
                else if (x >= ca_x + 320 && x <= ca_x + 410) {
                    chrome_url = "flathub.org";
                    browser_history.push_back(chrome_url);
                    browser_history_idx = browser_history.size() - 1;
                }
                else if (x >= ca_x + 420 && x <= ca_x + 510) {
                    DownloadItem dl;
                    std::string apps[] = {"gimp_flatpak.tar.gz", "vlc_player.tar.gz", "libreoffice.tar.gz"};
                    dl.filename = apps[rand() % 3];
                    dl.progress = 0;
                    dl.completed = false;
                    browser_downloads.push_back(dl);
                    push_notification("Download started: " + dl.filename);
                }
                return;
            }

            // Click links
            for (const auto& link : browser_links) {
                if (x >= link.x1 && x <= link.x2 && y >= link.y1 && y <= link.y2) {
                    if (link.url.rfind("download:", 0) == 0) {
                        std::string pkg = link.url.substr(9);
                        DownloadItem dl = {pkg + ".flatpak", 0, false};
                        browser_downloads.push_back(dl);
                        push_notification("Flatpak download started: " + dl.filename);
                        return;
                    }
                    if (browser_history_idx + 1 < (int)browser_history.size()) {
                        browser_history.erase(browser_history.begin() + browser_history_idx + 1, browser_history.end());
                    }
                    browser_history.push_back(link.url);
                    browser_history_idx = browser_history.size() - 1;
                    chrome_url = link.url;
                    return;
                }
            }
            return;
        }

        // Settings Window (windows[4])
        if (i == 4) {
            // Sidebar Category clicks
            if (x >= ca_x && x <= ca_x + 160) {
                int tab = (y - (ca_y + 8)) / 38;
                if (tab >= 0 && tab < 11) {
                    settings_active_tab = tab;
                }
                return;
            }

            // Content clicks
            int px = ca_x + 180;
            int py = ca_y + 20;
            int pw = ca_w - 200;
            auto row_y = [&](int r_idx) { return py + r_idx * 52; };

            if (settings_active_tab == 0) { // Display
                // Scale slider
                if (y >= row_y(1) + 16 && y <= row_y(1) + 28) {
                    if (x >= px && x <= px + pw) {
                        int scale = 1 + (x - px) * 3 / pw;
                        if (scale < 1) scale = 1;
                        if (scale > 4) scale = 4;
                        current_window_scale = scale;
                        if (sdl_window) {
                            SDL_SetWindowSize(sdl_window, 320 * current_window_scale, 240 * current_window_scale);
                        }
                        save_settings_to_file();
                    }
                }
                // Wallpaper selection
                else if (y >= row_y(2) + 18 && y <= row_y(2) + 50) {
                    for (int wp = 0; wp < 4; ++wp) {
                        int bx = px + wp * 56;
                        if (x >= bx && x <= bx + 48) {
                            settings_wallpaper_idx = wp;
                            save_settings_to_file();
                        }
                    }
                }
            }
            else if (settings_active_tab == 1) { // Appearance
                // Theme selection
                if (y >= row_y(1) + 18 && y <= row_y(1) + 50) {
                    for (size_t t = 0; t < installed_themes.size(); t++) {
                        int tx = px + (int)t * 90;
                        if (x >= tx && x <= tx + 80) {
                            active_theme = installed_themes[t];
                            is_dark_theme = (active_theme.name != "light");
                            settings_wallpaper_idx = active_theme.wallpaper;
                            settings_accent_idx = (active_theme.name == "neon") ? 4 : (is_dark_theme ? 0 : 5);
                            save_settings_to_file();
                            push_notification("Applied theme: " + active_theme.name);
                            break;
                        }
                    }
                }
                // Accent Color
                else if (y >= row_y(2) + 12 && y <= row_y(2) + 44) {
                    for (int c = 0; c < 6; c++) {
                        int bx = px + c * 40;
                        int cx = bx + 14;
                        int cy = row_y(2) + 28;
                        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 16 * 16) {
                            settings_accent_idx = c;
                            active_theme.accent = get_accent_color();
                            save_settings_to_file();
                        }
                    }
                }
                // Dark Mode toggle
                else if (y >= row_y(3) + 14 && y <= row_y(3) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    is_dark_theme = !is_dark_theme;
                    save_settings_to_file();
                }
                // Tiling Mode toggle
                else if (y >= row_y(4) + 14 && y <= row_y(4) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    is_tiling_mode = !is_tiling_mode;
                    save_settings_to_file();
                    push_notification(is_tiling_mode ? "Tiling mode enabled" : "Tiling mode disabled");
                }
            }
            else if (settings_active_tab == 2) { // Network
                // Wi-Fi toggle
                if (y >= row_y(1) + 14 && y <= row_y(1) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    wifi_enabled = !wifi_enabled;
                    save_settings_to_file();
                }
                // VPN toggle
                else if (y >= row_y(2) + 14 && y <= row_y(2) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    vpn_enabled = !vpn_enabled;
                    save_settings_to_file();
                }
            }
            else if (settings_active_tab == 3) { // Accounts
                // Add User
                if (x >= px && x <= px + 150 && y >= row_y(1) + 18 && y <= row_y(1) + 48) {
                    virtual_users.push_back("user_" + std::to_string(virtual_users.size()));
                    push_notification("User account added");
                }
                // Reset Passcode
                else if (x >= px && x <= px + 150 && y >= row_y(2) + 18 && y <= row_y(2) + 48) {
                    lock_password = "shri123";
                    push_notification("Passcode reset to shri123");
                }
            }
            else if (settings_active_tab == 4) { // Sound
                // Volume slider
                if (y >= row_y(1) + 18 && y <= row_y(1) + 30) {
                    if (x >= px && x <= px + pw) {
                        system_volume = (x - px) * 100 / pw;
                        if (system_volume < 0) system_volume = 0;
                        if (system_volume > 100) system_volume = 100;
                        save_settings_to_file();
                    }
                }
                // Mute toggle
                else if (y >= row_y(2) + 14 && y <= row_y(2) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    system_muted = !system_muted;
                }
            }
            else if (settings_active_tab == 5) { // Notifications
                // Toggle notifications
                if (y >= row_y(1) + 14 && y <= row_y(1) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    notifications_enabled = !notifications_enabled;
                }
                // Clear History
                else if (x >= px && x <= px + 150 && y >= row_y(2) + 18 && y <= row_y(2) + 48) {
                    notifications.clear();
                }
            }
            else if (settings_active_tab == 6) { // Updates
                // Check Updates
                if (x >= px && x <= px + 150 && y >= row_y(1) + 18 && y <= row_y(1) + 48) {
                    update_check_progress = 0;
                    push_notification("Checking for system updates...");
                }
                // Download update
                else if (x >= px && x <= px + 200 && y >= row_y(2) + 18 && y <= row_y(2) + 48) {
                    slot_upgrade_pending = true;
                    inactive_system_slot = "B (Version 1.1.0 Ready)";
                    save_settings_to_file();
                    push_notification("Update applied to inactive slot B");
                }
                // Reboot & Swap
                else if (slot_upgrade_pending && x >= px && x <= px + 200 && y >= row_y(3) + 18 && y <= row_y(3) + 48) {
                    std::swap(active_system_slot, inactive_system_slot);
                    slot_upgrade_pending = false;
                    save_settings_to_file();
                    push_notification("Rebooted and swapped slot to " + active_system_slot);
                }
            }
            else if (settings_active_tab == 7) { // Hardening
                // AppArmor Toggle
                if (y >= row_y(1) + 14 && y <= row_y(1) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    apparmor_enabled = !apparmor_enabled;
                    save_settings_to_file();
                    push_notification(apparmor_enabled ? "AppArmor Policies Activated" : "AppArmor Policies Disabled");
                }
                // Firewall Blocks (Terminal, Browser, App Store)
                else if (y >= row_y(3) && y <= row_y(3) + 100) {
                    int idx = (y - row_y(3)) / 28;
                    std::string apps[] = {"Terminal", "Browser", "App Store"};
                    if (idx >= 0 && idx < 3) {
                        firewall_rules[apps[idx]] = !firewall_rules[apps[idx]];
                        save_settings_to_file();
                        push_notification("Firewall rules updated for " + apps[idx]);
                    }
                }
            }
            else if (settings_active_tab == 8) { // Accessibility
                // Screen Reader
                if (y >= row_y(1) + 14 && y <= row_y(1) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    screen_reader_enabled = !screen_reader_enabled;
                    save_settings_to_file();
                    push_notification(screen_reader_enabled ? "Screen Reader Enabled" : "Screen Reader Disabled");
                }
                // Locale Selection
                else if (y >= row_y(2) + 18 && y <= row_y(2) + 48) {
                    if (x >= px && x <= px + 70) {
                        current_locale = "en_US";
                        save_settings_to_file();
                        push_notification("Locale changed to US English");
                    } else if (x >= px + 80 && x <= px + 150) {
                        current_locale = "es_ES";
                        save_settings_to_file();
                        push_notification("Locale cambiado a Español");
                    } else if (x >= px + 160 && x <= px + 230) {
                        current_locale = "de_DE";
                        save_settings_to_file();
                        push_notification("Sprachauswahl geändert in Deutsch");
                    }
                }
            }
            else if (settings_active_tab == 9) { // Smart Home
                // Living Room Light Toggle
                if (x >= px + pw - 100 && x <= px + pw - 20 && y >= row_y(1) + 10 && y <= row_y(1) + 40) {
                    iot_devices[0].state = (iot_devices[0].state == "on") ? "off" : "on";
                    save_settings_to_file();
                    push_notification("[MQTT] Living Room Light set to " + iot_devices[0].state);
                }
                // Smart Plug Toggle
                else if (x >= px + pw - 100 && x <= px + pw - 20 && y >= row_y(3) + 10 && y <= row_y(3) + 40) {
                    iot_devices[2].state = (iot_devices[2].state == "on") ? "off" : "on";
                    save_settings_to_file();
                    push_notification("[MQTT] Smart Plug set to " + iot_devices[2].state);
                }
                // Thermostat adjustment
                else if (y >= row_y(2) + 10 && y <= row_y(2) + 40) {
                    int val = atoi(iot_devices[1].state.c_str());
                    if (x >= px + pw - 120 && x <= px + pw - 90) {
                        val--;
                        iot_devices[1].state = std::to_string(val) + "C";
                        save_settings_to_file();
                        push_notification("[MQTT] Smart Thermostat decreased to " + iot_devices[1].state);
                    } else if (x >= px + pw - 50 && x <= px + pw - 20) {
                        val++;
                        iot_devices[1].state = std::to_string(val) + "C";
                        save_settings_to_file();
                        push_notification("[MQTT] Smart Thermostat increased to " + iot_devices[1].state);
                    }
                }
            }
            else if (settings_active_tab == 10) { // Bluetooth
                // Bluetooth Toggle
                if (y >= row_y(1) + 14 && y <= row_y(1) + 40 && x >= px + pw - 60 && x <= px + pw - 12) {
                    bluetooth_enabled = !bluetooth_enabled;
                    save_settings_to_file();
                    push_notification(bluetooth_enabled ? "Bluetooth enabled" : "Bluetooth disabled");
                }
                // Device connect toggling
                else if (bluetooth_enabled && y >= row_y(3) && y <= row_y(3) + 120) {
                    int idx = (y - row_y(3)) / 36;
                    if (idx >= 0 && idx < 3) {
                        push_notification("Bluetooth toggled for " + bluetooth_devices[idx]);
                    }
                }
            }
            return;
        }

        // Control Panel Window (windows[5])
        if (i == 5) {
            int px = ca_x + 16;
            int py = ca_y + 16;
            int cw2 = ca_w / 2 - 24;
            int rx2 = ca_x + ca_w / 2 + 8;
            
            // Left column (System)
            // Sensor Logger Toggle
            if (x >= px + cw2 - 68 && x <= px + cw2 - 20 && y >= py + 34 && y <= py + 60) {
                sensor_logging_enabled = !sensor_logging_enabled;
            }
            // Lock screen Lock Now
            else if (x >= px + 16 && x <= px + 116 && y >= py + 108 && y <= py + 138) {
                system_locked = true;
            }
            // Chrome Browser Button
            else if (x >= px + 16 && x <= px + 116 && y >= py + 176 && y <= py + 206) {
                apt_installed_packages["chrome"] = !apt_installed_packages["chrome"];
                if (!apt_installed_packages["chrome"]) {
                    windows[3].open = false;
                    chrome_address_active = false;
                }
            }
            // Right column (Firewall)
            // Firewall incoming traffic toggle
            else if (x >= rx2 + cw2 - 68 && x <= rx2 + cw2 - 20 && y >= py + 34 && y <= py + 60) {
                firewall_allow_incoming = !firewall_allow_incoming;
            }
            // Rules
            else {
                struct { const char *k; } rules[] = {{"Terminal"},{"Browser"},{"App Store"}};
                for (int r = 0; r < 3; ++r) {
                    int ry2 = py + 82 + r * 42;
                    if (x >= rx2 + cw2 - 70 && x <= rx2 + cw2 - 10 && y >= ry2 - 4 && y <= ry2 + 20) {
                        firewall_rules[rules[r].k] = !firewall_rules[rules[r].k];
                        break;
                    }
                }
            }
            return;
        }

        // Text Editor Window (windows[6])
        if (i == 6) {
            int ty_start = ca_y + ca_h - 120;
            
            if (x >= ca_x && x <= ca_x + 130 && y >= ca_y + 60 && y <= ca_y + 60 + 6 * 22) {
                int f_idx = (y - (ca_y + 60)) / 22;
                std::vector<std::string> workspace_files = {
                    "src/main.cpp", "src/graphics.cpp", "src/state.cpp",
                    "include/state.h", "Makefile.standalone", "README.md"
                };
                if (f_idx >= 0 && f_idx < 6) {
                    std::string path = "/home/shrinathpol/EmbeddedSystem/" + workspace_files[f_idx];
                    std::ifstream file(path);
                    if (file.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        editor_text_content = content;
                        editor_current_file = workspace_files[f_idx];
                        editor_cursor_pos = 0;
                        editor_terminal_focused = false;
                        push_notification("Loaded " + workspace_files[f_idx]);
                    }
                }
                return;
            }
            
            if (y >= ty_start) {
                editor_terminal_focused = true;
                if (y <= ty_start + 24) {
                    if (x >= ca_x + 130 + 120 && x <= ca_x + 130 + 190) {
                        active_term_pane_idx = 0;
                    } else if (x >= ca_x + 130 + 200 && x <= ca_x + 130 + 270) {
                        active_term_pane_idx = 1;
                    } else if (x >= ca_x + 130 + 280 && x <= ca_x + 130 + 350) {
                        active_term_pane_idx = 2;
                    }
                }
                return;
            }
            
            if (y >= ca_y + 32 && y < ty_start) {
                editor_terminal_focused = false;
            }
            
            // File Menu
            if (x >= ca_x + 16 && x <= ca_x + 50 && y >= ca_y && y <= ca_y + 32) {
                editor_undo_buffer = (editor_undo_buffer == "file_menu") ? "" : "file_menu";
            }
            // Edit Menu
            else if (x >= ca_x + 60 && x <= ca_x + 95 && y >= ca_y && y <= ca_y + 32) {
                editor_undo_buffer = (editor_undo_buffer == "edit_menu") ? "" : "edit_menu";
            }
            return;
        }

        // App Store Window (windows[7])
        if (i == 7) {
            // Sidebar Tab clicks
            if (x >= ca_x && x <= ca_x + 150) {
                int cat = (y - (ca_y + 8)) / 38;
                if (cat >= 0 && cat < 5) {
                    appstore_active_category = cat;
                }
                return;
            }

            // Right Pane install/remove clicks
            int app_y = ca_y + 12;
            int bw = 100;
            for (size_t a = 0; a < appstore_apps.size(); a++) {
                const auto& app = appstore_apps[a];
                bool show = false;
                if (appstore_active_category == 0) show = true;
                else if (appstore_active_category == 1 &&
                    (app.package_name == "python" || app.package_name == "java" ||
                     app.package_name == "g++" || app.package_name == "gcc" || app.package_name == "nodejs")) show = true;
                else if (appstore_active_category == 2 &&
                    (app.package_name == "chrome" || app.package_name == "neofetch")) show = true;
                else if (appstore_active_category == 3 && app.package_name == "cmatrix") show = true;
                if (!show) continue;
                
                int btn_x1 = ca_x + ca_w - bw - 20;
                int btn_x2 = ca_x + ca_w - 20;
                int btn_y1 = app_y + 24;
                int btn_y2 = app_y + 56;
                
                if (x >= btn_x1 && x <= btn_x2 && y >= btn_y1 && y <= btn_y2) {
                    bool is_installed = apt_installed_packages[app.package_name];
                    if (is_installed) {
                        apt_installed_packages[app.package_name] = false;
                        if (app.package_name == "chrome") {
                            windows[3].open = false;
                            chrome_address_active = false;
                        }
                        push_notification("Removed " + app.name);
                    } else if (appstore_installing_idx == -1) {
                        appstore_installing_idx = (int)a;
                        appstore_progress = 0;
                        push_notification("Installing " + app.name + "...");
                    }
                    return;
                }
                
                app_y += 88;
                if (app_y + 80 > ca_y + ca_h) break;
            }
            return;
        }
    }

    // 5. Desktop Icons Click (Grid layout starts at (24, 24) with step 96)
    struct DesktopIcon {
        int col;
        int row;
        int app_id;
    };
    DesktopIcon icons[] = {
        {0, 0, 0}, // Terminal
        {0, 1, 1}, // Files
        {0, 2, 2}, // Monitor
        {0, 3, 6}, // Editor
        {1, 0, 4}, // Settings
        {1, 1, 5}, // Control
        {1, 2, 3}, // Browser
        {1, 3, 7}  // App Store
    };
    
    for (auto const& ic : icons) {
        int ix = ICO_START_X + ic.col * ICO_STEP_X;
        int iy = ICO_START_Y + ic.row * ICO_STEP_Y;
        if (x >= ix && x <= ix + ICON_SZ && y >= iy && y <= iy + ICON_SZ) {
            if (ic.app_id == 3 && !apt_installed_packages["chrome"]) return;
            windows[ic.app_id].open = !windows[ic.app_id].open;
            windows[ic.app_id].minimized = false;
            if (windows[ic.app_id].open) bring_to_front(ic.app_id);
            return;
        }
    }
}

void handle_right_click(int x, int y) {
    if (windows[1].open && !windows[1].minimized) {
        int wx = windows[1].x;
        int wy = windows[1].y;
        int ww = windows[1].w;
        int wh = windows[1].h;
        
        int ca_x = wx + 1;
        int ca_y = wy + TITLEBAR_H + 1;
        int ca_w = ww - 2;
        int ca_h = wh - TITLEBAR_H - 1;
        
        // Right pane click target
        if (x >= ca_x + 180 && x <= ca_x + ca_w && y >= ca_y + 36 && y <= ca_y + ca_h - 36) {
            std::vector<std::pair<std::string, std::string>> result;
            for (const auto& [name, content] : virtual_files) {
                if (name == file_manager_current_dir) continue;
                if (file_manager_current_dir == "/") {
                    if (name.find('/', 1) == std::string::npos) {
                        result.push_back({name, content});
                    }
                } else {
                    if (name.rfind(file_manager_current_dir + "/", 0) == 0) {
                        std::string sub = name.substr(file_manager_current_dir.length() + 1);
                        if (sub.find('/') == std::string::npos) {
                            result.push_back({name, content});
                        }
                    }
                }
            }
            int file_y = ca_y + 48;
            for (auto const& [name, content] : result) {
                if (y >= file_y - 2 && y < file_y + 26) {
                    file_manager_context_menu_open = true;
                    file_manager_context_menu_x = x;
                    file_manager_context_menu_y = y;
                    file_manager_context_menu_target = name;
                    selected_file = name;
                    return;
                }
                file_y += 28;
            }
        }
    }
    file_manager_context_menu_open = false;
}

void add_terminal_log(const std::string& line) {
    terminal_logs.push_back(line);
    if (terminal_logs.size() > 8) {
        terminal_logs.erase(terminal_logs.begin());
    }
}

bool is_focused_window(int idx) {
    for (int z = 7; z >= 0; z--) {
        int i = window_z_order[z];
        if (windows[i].open && !windows[i].minimized) {
            return (i == idx);
        }
    }
    return false;
}

void get_tiled_bounds(int open_count, int index_in_tiled, int screen_w, int screen_h, int& ox, int& oy, int& ow, int& oh) {
    int gap = 8;
    int top_margin = 8;
    int bottom_margin = TASKBAR_H + 8;
    int left_margin = 8;
    int right_margin = 8;

    int area_x = left_margin;
    int area_y = top_margin;
    int area_w = screen_w - left_margin - right_margin;
    int area_h = screen_h - top_margin - bottom_margin;

    if (open_count <= 1) {
        ox = area_x;
        oy = area_y;
        ow = area_w;
        oh = area_h;
    } else if (open_count == 2) {
        int half_w = (area_w - gap) / 2;
        if (index_in_tiled == 0) {
            ox = area_x;
            oy = area_y;
            ow = half_w;
            oh = area_h;
        } else {
            ox = area_x + half_w + gap;
            oy = area_y;
            ow = half_w;
            oh = area_h;
        }
    } else if (open_count == 3) {
        int main_w = (area_w - gap) * 6 / 10;
        int stack_w = area_w - main_w - gap;
        int half_h = (area_h - gap) / 2;
        if (index_in_tiled == 0) {
            ox = area_x;
            oy = area_y;
            ow = main_w;
            oh = area_h;
        } else if (index_in_tiled == 1) {
            ox = area_x + main_w + gap;
            oy = area_y;
            ow = stack_w;
            oh = half_h;
        } else {
            ox = area_x + main_w + gap;
            oy = area_y + half_h + gap;
            ow = stack_w;
            oh = half_h;
        }
    } else {
        int cols = (open_count <= 4) ? 2 : 3;
        int rows = (open_count + cols - 1) / cols;
        int r = index_in_tiled / cols;
        int c = index_in_tiled % cols;

        int w_with_gaps = area_w - (cols - 1) * gap;
        int h_with_gaps = area_h - (rows - 1) * gap;
        int cell_w = w_with_gaps / cols;
        int cell_h = h_with_gaps / rows;

        ox = area_x + c * (cell_w + gap);
        oy = area_y + r * (cell_h + gap);
        ow = cell_w;
        oh = cell_h;
    }
}
