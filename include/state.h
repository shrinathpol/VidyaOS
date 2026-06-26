#ifndef STATE_H
#define STATE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct Window {
    const char *title;
    int x, y, w, h;
    bool open;
    int zOrder;
    bool minimized;
    bool hasTitleBar;
    int workspace; // 0..3
};

struct SDL_Window;

struct Notification {
    std::string message;
    int timestamp; // mock unix or tick time
};

extern bool sensor_logging_enabled;
extern bool system_locked;
extern std::string lock_password;
extern std::map<std::string, std::string> virtual_files;
extern std::string current_working_directory;
extern Window windows[8];
extern bool start_menu_open;
extern bool is_dark_theme;
extern std::string selected_file;
extern int mouse_x;
extern int mouse_y;
extern std::vector<std::string> terminal_logs;
extern std::vector<int> sensor_history;
extern int current_sensor_value;
extern std::string gui_input_buffer;

// --- Phase 2 Dragging & Window Manager ---
extern int dragging_window_index;
extern int drag_offset_x;
extern int drag_offset_y;

// --- Phase 2 Settings Customizer ---
extern int settings_active_tab;
extern int settings_wallpaper_idx;
extern int settings_accent_idx;
extern int settings_font_size;
extern bool wifi_enabled;
extern std::string wifi_ssid;
extern bool vpn_enabled;
extern int system_volume;
extern bool system_muted;
extern int sound_device_idx;
extern bool notifications_enabled;
extern std::vector<std::string> virtual_users;
extern int update_check_progress;
extern std::string last_update_check_time;

// --- Phase 2 Notification Centre ---
extern std::vector<Notification> notifications;
extern bool notifications_popup_open;
void push_notification(const std::string& msg);

// --- Phase 2 Clipboard ---
extern std::string global_clipboard_path;

// --- Phase 2 Text Editor ---
extern std::string editor_text_content;
extern std::string editor_current_file;
extern int editor_cursor_pos;
extern std::string editor_undo_buffer;

// --- Phase 2 System Monitor Scrolling Graphs ---
extern std::vector<int> cpu_history;
extern std::vector<int> ram_history;

// --- Phase 2 File Manager 2.0 tree & context menu ---
extern std::string file_manager_current_dir;
extern std::vector<std::string> file_manager_expanded_dirs;
extern bool file_manager_context_menu_open;
extern int file_manager_context_menu_x;
extern int file_manager_context_menu_y;
extern std::string file_manager_context_menu_target;
extern std::vector<std::string> footprint_added;
extern std::vector<std::string> footprint_removed;
extern std::vector<std::string> footprint_modified;
extern bool footprint_has_diff;


extern std::map<std::string, bool> apt_installed_packages;
extern std::string chrome_url;
extern bool chrome_address_active;
extern bool cmatrix_active;
extern int gui_frame_counter;

extern bool python_repl_active;
extern std::map<std::string, std::string> python_variables;
extern std::map<std::string, bool> java_compiled_classes;
extern std::map<std::string, std::string> compiled_binaries;

extern bool javascript_repl_active;
extern std::map<std::string, std::string> javascript_variables;

void handle_desktop_click(int x, int y, bool is_double_click = false);
void handle_right_click(int x, int y);
void save_settings_to_file();
void load_settings_from_file();
void add_terminal_log(const std::string& line);
bool is_focused_window(int idx);

extern int telemetry_cpu_usage;
extern int telemetry_ram_usage;
extern std::string telemetry_json_output;

// --- Phase 1 Additions ---
extern int window_z_order[8];
void bring_to_front(int index);

extern std::string file_manager_viewing_content;
extern std::string file_manager_viewing_title;

extern SDL_Window* sdl_window;
extern int current_window_scale;

// ── Modern UI Layout Constants (native resolution) ──────────────────────
// These are defined as constants here and computed dynamically in graphics.cpp
// based on the actual display width/height.
#define TASKBAR_H   48      // height of the bottom dock/taskbar
#define TITLEBAR_H  32      // height of each window's title bar
#define ICON_SZ     64      // desktop icon square size
#define CORNER_R    8       // window corner rounding radius (approx)

#define ICO_START_X 24
#define ICO_START_Y 24
#define ICO_STEP_X  96
#define ICO_STEP_Y  96

// Terminal dimensions — larger at native resolution (fits a ~600×400 window)
#define TERM_COLS   70
#define TERM_ROWS   22

struct VTerm;
struct VTermScreen;
extern VTerm* vterm_instance;
extern VTermScreen* vterm_screen;
extern bool bash_shell_active;
extern int pty_master_fd;
extern int shell_pid;

std::string get_sandbox_root();
std::string translate_path(const std::string& internal_path);
std::string resolve_path(const std::string& path);
void sync_sandbox_to_virtual_files();

// --- Phase 3 Networking ---
struct NetworkState {
    bool wifiOn;
    std::string ssid;
    int signalStrength;
    bool vpn;
    std::string ipAddress;
};
extern NetworkState net_state;
extern std::vector<std::string> available_networks;

// --- Phase 3 Multi-User & Security ---
extern std::string currentUser;
struct FileMeta {
    std::string owner;
    int permissions; // e.g. 0755
};
extern std::map<std::string, FileMeta> file_metadata;
extern bool system_logged_in;
extern std::string login_input_buffer;
extern std::string login_pass_buffer;
extern int login_selected_user_idx;
extern std::map<std::string, std::string> user_passwords; // username -> sha256_hash
extern bool firewall_allow_incoming;
extern std::map<std::string, bool> firewall_rules; // app -> allowed

// --- Phase 3 Workspaces ---
extern int current_workspace;
extern int settings_wallpapers[4]; // Wallpapers for workspace 0..3

// --- Phase 3 Themes Engine ---
struct Theme {
    std::string name;
    uint32_t window_bg[8]; // individual bg color for each of the 8 windows
    uint32_t titlebar_bg;
    uint32_t titlebar_fg;
    uint32_t taskbar_bg;
    uint32_t font_color;
    uint32_t accent;
    int font_size;
    int wallpaper;
};
extern Theme active_theme;
extern std::vector<Theme> installed_themes;

// --- Phase 3 App Store ---
extern int appstore_active_category; // 0: All, 1: Dev, 2: Utils, 3: Games, 4: Themes
extern int appstore_installing_idx;  // index of app currently installing (-1 if none)
extern int appstore_progress;        // 0..100
struct AppStoreItem {
    std::string name;
    std::string package_name;
    std::string desc;
    int rating;
    std::string size;
    uint32_t color; // representation color for mock screenshot
};
extern std::vector<AppStoreItem> appstore_apps;

// --- Phase 3 Browser Upgrade ---
extern std::vector<std::string> browser_history;
extern int browser_history_idx;
struct WebLink {
    int x1, y1, x2, y2;
    std::string url;
};
extern std::vector<WebLink> browser_links;

void set_default_metadata(const std::string& path, bool is_dir);
uint32_t get_accent_color();

// --- Phase 5 OS features ---
extern bool is_tiling_mode;

struct DownloadItem {
    std::string filename;
    int progress; // 0..100
    bool completed;
};
extern std::vector<DownloadItem> browser_downloads;
extern std::vector<std::string> browser_bookmarks;

struct TermPane {
    std::string title;
    std::vector<std::string> logs;
    std::string input_buffer;
    bool active;
};
extern std::vector<TermPane> term_panes;
extern int active_term_pane_idx;

extern std::string active_system_slot;
extern std::string inactive_system_slot;
extern bool slot_upgrade_pending;

extern bool apparmor_enabled;
extern std::vector<std::string> firewall_blocked_apps;

struct IoTDevice {
    std::string name;
    std::string type;
    std::string state;
};
extern std::vector<IoTDevice> iot_devices;

extern bool voice_assistant_active;
extern std::string voice_assistant_input;

extern bool screen_reader_enabled;
extern std::string current_locale;

extern bool bluetooth_enabled;
extern std::vector<std::string> bluetooth_devices;

extern bool editor_terminal_focused;
extern bool downloads_panel_open;

void get_tiled_bounds(int open_count, int index_in_tiled, int screen_w, int screen_h, int& ox, int& oy, int& ow, int& oh);

#endif // STATE_H

