#include "platform.h"
#include "state.h"
#include "graphics.h"
#include "shell.h"
#include "footprint.h"
#include <filesystem>
namespace fs = std::filesystem;
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vterm.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>




int pty_master_fd = -1;
int shell_pid = -1;
bool bash_shell_active = false;
VTerm *vterm_instance = nullptr;
VTermScreen *vterm_screen = nullptr;

static void term_output_cb(const char *s, size_t len, void *user) {
    int fd = (int)(intptr_t)user;
    if (fd >= 0) {
        ssize_t written = write(fd, s, len);
        (void)written;
    }
}

void spawn_bash_pty() {
    pty_master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty_master_fd < 0) {
        perror("posix_openpt");
        return;
    }
    if (grantpt(pty_master_fd) < 0) {
        perror("grantpt");
        return;
    }
    if (unlockpt(pty_master_fd) < 0) {
        perror("unlockpt");
        return;
    }
    char *slave_name = ptsname(pty_master_fd);
    if (!slave_name) {
        perror("ptsname");
        return;
    }

    // Set non-blocking mode on master FD
    int flags = fcntl(pty_master_fd, F_GETFL, 0);
    fcntl(pty_master_fd, F_SETFL, flags | O_NONBLOCK);

    shell_pid = fork();
    if (shell_pid < 0) {
        perror("fork");
        return;
    }
    if (shell_pid == 0) {
        // Child process
        close(pty_master_fd);

        int slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            exit(1);
        }

        // Set terminal size on slave PTY
        struct winsize ws;
        ws.ws_row = TERM_ROWS;
        ws.ws_col = TERM_COLS;
        ws.ws_xpixel = TERM_COLS * 8;
        ws.ws_ypixel = TERM_ROWS * 10;
        ioctl(slave_fd, TIOCSWINSZ, &ws);

        // Redirect stdin, stdout, stderr
        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        close(slave_fd);

        // Create new session
        setsid();
        ioctl(0, TIOCSCTTY, 1);

        // Change directory to translated path of "/"
        std::string root_path = translate_path("/");
        chdir(root_path.c_str());

        // Set environment variables
        setenv("TERM", "vt100", 1);
        setenv("VIDYAOS_SANDBOX_ROOT", get_sandbox_root().c_str(), 1);

        // Execute shell
        execl("/bin/bash", "/bin/bash", "--login", nullptr);
        execl("/bin/sh", "/bin/sh", nullptr);
        exit(1);
    }
}

void update_terminal_pty() {
    if (pty_master_fd < 0) return;

    char buf[1024];
    while (true) {
        ssize_t n = read(pty_master_fd, buf, sizeof(buf));
        if (n > 0) {
            vterm_input_write(vterm_instance, buf, n);
        } else {
            // EWOULDBLOCK / EAGAIN or EOF
            break;
        }
    }
}

int get_cpu_usage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0;
    std::string line;
    std::getline(file, line);
    file.close();

    if (line.rfind("cpu ", 0) != 0) return 0;

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    std::stringstream ss(line.substr(4));
    if (!(ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice)) {
        return 0;
    }

    unsigned long long total_idle = idle + iowait;
    unsigned long long total_non_idle = user + nice + system + irq + softirq + steal;
    unsigned long long total = total_idle + total_non_idle;

    static unsigned long long last_total = 0;
    static unsigned long long last_idle = 0;

    unsigned long long diff_total = total - last_total;
    unsigned long long diff_idle = total_idle - last_idle;

    last_total = total;
    last_idle = total_idle;

    if (diff_total == 0) return 0;
    int percentage = (int)((diff_total - diff_idle) * 100 / diff_total);
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    return percentage;
}

int get_ram_usage_mb() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        unsigned long used_bytes = (si.totalram - si.freeram) * si.mem_unit;
        unsigned long used_mb = used_bytes / 1024 / 1024;
        return (int)used_mb;
    }
    return 512;
}

int get_temp() {
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (file.is_open()) {
        int raw_temp = 0;
        if (file >> raw_temp) {
            file.close();
            return raw_temp / 1000;
        }
        file.close();
    }
    static int mock_temp = 35;
    mock_temp += (rand() % 3) - 1;
    if (mock_temp < 25) mock_temp = 25;
    if (mock_temp > 75) mock_temp = 75;
    return mock_temp;
}

// ─────────────────────────────────────────────
//  Shared SDL2 event handler (mouse + keyboard)
//  Uses SDL_RenderWindowToLogical() so cursor
//  is correct at ANY display resolution / scale.
// ─────────────────────────────────────────────
// is_focused_window is now implemented in state.cpp and declared in state.h

static void handle_sdl_event(const SDL_Event& event, SDL_Renderer* renderer) {
    // Keep ensuring system cursor is disabled
    SDL_ShowCursor(SDL_DISABLE);

    if (event.type == SDL_MOUSEMOTION) {
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
        if (dragging_window_index != -1) {
            windows[dragging_window_index].x = mouse_x - drag_offset_x;
            windows[dragging_window_index].y = mouse_y - drag_offset_y;
        }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouse_x = event.button.x;
            mouse_y = event.button.y;
            bool is_double_click = (event.button.clicks == 2);
            handle_desktop_click(mouse_x, mouse_y, is_double_click);
        }
        else if (event.button.button == SDL_BUTTON_RIGHT) {
            mouse_x = event.button.x;
            mouse_y = event.button.y;
            handle_right_click(mouse_x, mouse_y);
        }
    }
    else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (dragging_window_index != -1 && !is_tiling_mode) {
                int screen_w = 1280, screen_h = 720;
                if (sdl_window) {
                    SDL_GetWindowSize(sdl_window, &screen_w, &screen_h);
                }
                int i = dragging_window_index;
                if (mouse_x < 50 && mouse_y < 50) {
                    windows[i].x = 8;
                    windows[i].y = 8;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = (screen_h - TASKBAR_H)/2 - 12;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Top-Left");
                } else if (mouse_x > screen_w - 50 && mouse_y < 50) {
                    windows[i].x = screen_w/2 + 4;
                    windows[i].y = 8;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = (screen_h - TASKBAR_H)/2 - 12;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Top-Right");
                } else if (mouse_x < 50 && mouse_y > screen_h - TASKBAR_H - 50) {
                    windows[i].x = 8;
                    windows[i].y = (screen_h - TASKBAR_H)/2 + 4;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = (screen_h - TASKBAR_H)/2 - 12;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Bottom-Left");
                } else if (mouse_x > screen_w - 50 && mouse_y > screen_h - TASKBAR_H - 50) {
                    windows[i].x = screen_w/2 + 4;
                    windows[i].y = (screen_h - TASKBAR_H)/2 + 4;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = (screen_h - TASKBAR_H)/2 - 12;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Bottom-Right");
                } else if (mouse_x < 30) {
                    windows[i].x = 8;
                    windows[i].y = 8;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = screen_h - TASKBAR_H - 16;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Left Half");
                } else if (mouse_x > screen_w - 30) {
                    windows[i].x = screen_w/2 + 4;
                    windows[i].y = 8;
                    windows[i].w = screen_w/2 - 12;
                    windows[i].h = screen_h - TASKBAR_H - 16;
                    push_notification("Snapped " + std::string(windows[i].title) + " to Right Half");
                } else if (mouse_y < 30) {
                    windows[i].x = 8;
                    windows[i].y = 8;
                    windows[i].w = screen_w - 16;
                    windows[i].h = screen_h - TASKBAR_H - 16;
                    push_notification("Maximized " + std::string(windows[i].title));
                }
            }
            dragging_window_index = -1;
        }
    }
    else if (event.type == SDL_TEXTINPUT) {
        if (!system_logged_in) {
            login_pass_buffer += event.text.text;
        }
        else if (cmatrix_active) {
            cmatrix_active = false;
        }
        else if (windows[6].open && is_focused_window(6)) {
            if (editor_terminal_focused) {
                term_panes[active_term_pane_idx].input_buffer += event.text.text;
            } else {
                editor_undo_buffer = editor_text_content;
                editor_text_content.insert(editor_cursor_pos, event.text.text);
                editor_cursor_pos += strlen(event.text.text);
            }
        }
        else if (windows[3].open && chrome_address_active) {
            chrome_url += event.text.text;
        }
        else if (bash_shell_active && windows[0].open && is_focused_window(0)) {
            for (size_t i = 0; event.text.text[i] != '\0'; i++) {
                vterm_keyboard_unichar(vterm_instance, (uint32_t)event.text.text[i], VTERM_MOD_NONE);
            }
        }
        else if (windows[0].open && is_focused_window(0)) {
            gui_input_buffer += event.text.text;
        }
    }
    else if (event.type == SDL_KEYDOWN) {
        if (!system_logged_in) {
            if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (!login_pass_buffer.empty()) login_pass_buffer.pop_back();
            }
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
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
        if (cmatrix_active) {
            cmatrix_active = false;
            return;
        }

        // Escape toggles bash shell mode
        if (event.key.keysym.sym == SDLK_ESCAPE && windows[0].open && is_focused_window(0)) {
            bash_shell_active = !bash_shell_active;
            return;
        }

        if (windows[6].open && is_focused_window(6)) {
            if (editor_terminal_focused) {
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    if (!term_panes[active_term_pane_idx].input_buffer.empty()) {
                        term_panes[active_term_pane_idx].input_buffer.pop_back();
                    }
                }
                else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                    std::string cmd = term_panes[active_term_pane_idx].input_buffer;
                    term_panes[active_term_pane_idx].logs.push_back("$ " + cmd);
                    
                    if (cmd == "help") {
                        term_panes[active_term_pane_idx].logs.push_back("Available: help, clear, neofetch, make, ls");
                    } else if (cmd == "clear") {
                        term_panes[active_term_pane_idx].logs.clear();
                    } else if (cmd == "neofetch") {
                        term_panes[active_term_pane_idx].logs.push_back("VidyaOS 1.0.0 (x86_64)");
                        term_panes[active_term_pane_idx].logs.push_back("Kernel: Linux + KMSDRM");
                        term_panes[active_term_pane_idx].logs.push_back("Uptime: 2 mins");
                    } else if (cmd == "make") {
                        term_panes[active_term_pane_idx].logs.push_back("g++ -std=c++17 -O2 -Iinclude -c src/main.cpp...");
                        term_panes[active_term_pane_idx].logs.push_back("Build finished successfully!");
                    } else if (cmd == "ls") {
                        term_panes[active_term_pane_idx].logs.push_back("Makefile.standalone  include  src  thirdparty  vidyaos-desktop");
                    } else if (!cmd.empty()) {
                        term_panes[active_term_pane_idx].logs.push_back("Command not found: " + cmd);
                    }
                    
                    if (term_panes[active_term_pane_idx].logs.size() > 10) {
                        term_panes[active_term_pane_idx].logs.erase(term_panes[active_term_pane_idx].logs.begin());
                    }
                    term_panes[active_term_pane_idx].input_buffer.clear();
                }
                return;
            }

            if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (editor_cursor_pos > 0) {
                    editor_undo_buffer = editor_text_content;
                    editor_text_content.erase(editor_cursor_pos - 1, 1);
                    editor_cursor_pos--;
                }
            }
            else if (event.key.keysym.sym == SDLK_DELETE) {
                if (editor_cursor_pos < (int)editor_text_content.length()) {
                    editor_undo_buffer = editor_text_content;
                    editor_text_content.erase(editor_cursor_pos, 1);
                }
            }
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                editor_undo_buffer = editor_text_content;
                editor_text_content.insert(editor_cursor_pos, "\n");
                editor_cursor_pos++;
            }
            else if (event.key.keysym.sym == SDLK_LEFT) {
                if (editor_cursor_pos > 0) editor_cursor_pos--;
            }
            else if (event.key.keysym.sym == SDLK_RIGHT) {
                if (editor_cursor_pos < (int)editor_text_content.length()) editor_cursor_pos++;
            }
            else if (event.key.keysym.sym == SDLK_UP) {
                size_t prev_line = editor_text_content.rfind('\n', editor_cursor_pos - 1);
                if (prev_line != std::string::npos) {
                    size_t pprev_line = editor_text_content.rfind('\n', prev_line - 1);
                    size_t current_line_col = editor_cursor_pos - (prev_line + 1);
                    size_t prev_line_len = prev_line - (pprev_line == std::string::npos ? 0 : pprev_line + 1);
                    size_t target_col = std::min(current_line_col, prev_line_len);
                    editor_cursor_pos = (pprev_line == std::string::npos ? 0 : pprev_line + 1) + target_col;
                } else {
                    editor_cursor_pos = 0;
                }
            }
            else if (event.key.keysym.sym == SDLK_DOWN) {
                size_t next_line = editor_text_content.find('\n', editor_cursor_pos);
                if (next_line != std::string::npos) {
                    size_t prev_line = editor_text_content.rfind('\n', editor_cursor_pos - 1);
                    size_t current_line_col = editor_cursor_pos - (prev_line == std::string::npos ? 0 : prev_line + 1);
                    size_t nnext_line = editor_text_content.find('\n', next_line + 1);
                    size_t next_line_len = (nnext_line == std::string::npos ? editor_text_content.length() : nnext_line) - (next_line + 1);
                    size_t target_col = std::min(current_line_col, next_line_len);
                    editor_cursor_pos = next_line + 1 + target_col;
                } else {
                    editor_cursor_pos = editor_text_content.length();
                }
            }
            else if (event.key.keysym.mod & KMOD_CTRL) {
                if (event.key.keysym.sym == SDLK_z) { // Undo
                    std::string temp = editor_text_content;
                    editor_text_content = editor_undo_buffer;
                    editor_undo_buffer = temp;
                    editor_cursor_pos = std::min(editor_cursor_pos, (int)editor_text_content.length());
                }
            }
            return;
        }

        if (windows[3].open && chrome_address_active) {
            if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (!chrome_url.empty()) chrome_url.pop_back();
            }
            else if (event.key.keysym.sym == SDLK_RETURN ||
                     event.key.keysym.sym == SDLK_KP_ENTER) {
                chrome_address_active = false;
            }
        }
        else if (bash_shell_active && windows[0].open && is_focused_window(0)) {
            VTermKey vkey = VTERM_KEY_NONE;
            switch(event.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER: vkey = VTERM_KEY_ENTER; break;
                case SDLK_BACKSPACE: vkey = VTERM_KEY_BACKSPACE; break;
                case SDLK_TAB: vkey = VTERM_KEY_TAB; break;
                case SDLK_UP: vkey = VTERM_KEY_UP; break;
                case SDLK_DOWN: vkey = VTERM_KEY_DOWN; break;
                case SDLK_LEFT: vkey = VTERM_KEY_LEFT; break;
                case SDLK_RIGHT: vkey = VTERM_KEY_RIGHT; break;
                default: break;
            }
            if (vkey != VTERM_KEY_NONE) {
                vterm_keyboard_key(vterm_instance, vkey, VTERM_MOD_NONE);
            }
            else {
                VTermModifier mod = VTERM_MOD_NONE;
                if (event.key.keysym.mod & KMOD_CTRL) {
                    mod = (VTermModifier)(mod | VTERM_MOD_CTRL);
                    char c = 0;
                    if (event.key.keysym.sym >= SDLK_a && event.key.keysym.sym <= SDLK_z) {
                        c = event.key.keysym.sym - SDLK_a + 'a';
                    }
                    if (c) {
                        vterm_keyboard_unichar(vterm_instance, (uint32_t)c, mod);
                    }
                }
            }
        }
        else if (windows[0].open && is_focused_window(0)) {
            if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (!gui_input_buffer.empty()) gui_input_buffer.pop_back();
            }
            else if (event.key.keysym.sym == SDLK_RETURN ||
                     event.key.keysym.sym == SDLK_KP_ENTER) {
                execute_os_command(gui_input_buffer);
                gui_input_buffer.clear();
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Zephyr-only: Zephyr Sensor / Display devices
// ─────────────────────────────────────────────
#ifdef __ZEPHYR__
#if DT_NODE_HAS_STATUS(DT_NODELABEL(my_custom_sensor), okay)
    const struct device *sensor_dev = DEVICE_DT_GET(DT_NODELABEL(my_custom_sensor));
#else
    const struct device *sensor_dev = NULL;
#endif
K_THREAD_DEFINE(shell_thread_id, 8192, shell_thread_entry, NULL, NULL, NULL, 5, 0, 0);
#else
    const struct device *sensor_dev = NULL;
#endif

// ─────────────────────────────────────────────
//  Boot animation (systemd-style)
// ─────────────────────────────────────────────
void play_boot_animation() {
    const char* boot_lines[] = {
        "Booting Vidya OS Kernel...",
        "[  OK  ] Created slice User Application Slice.",
        "[  OK  ] Started Dispatch Password Requests to Console Directory.",
        "[  OK  ] Reached target Local File Systems.",
        "[  OK  ] Started udev Coldplug all Devices.",
        "[  OK  ] Started Set Up Additional Trust Store Certificates.",
        "[  OK  ] Started System Logging Service.",
        "[  OK  ] Started Accounts Service.",
        "[  OK  ] Reached target Multi-User System.",
        "[  OK  ] Reached target Graphical Interface.",
        "Loading Desktop Environment Shell..."
    };
    for (int i = 0; i < 11; i++) {
        printk("%s\n", boot_lines[i]);
        k_msleep(80);
    }
}

// ─────────────────────────────────────────────
//  Main entry point
// ─────────────────────────────────────────────
int main() {
    play_boot_animation();

    // ── Standalone: launch CLI shell on a background thread ──
    #ifndef __ZEPHYR__
    std::thread cli_thread(shell_thread_entry, nullptr, nullptr, nullptr);
    cli_thread.detach();
    #endif

    // ── Native framebuffer: render at actual display resolution ──
    SDL_DisplayMode dm_early;
    int display_width  = 1280;  // safe fallback
    int display_height = 720;
    {
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            if (SDL_GetCurrentDisplayMode(0, &dm_early) == 0) {
                display_width  = dm_early.w;
                display_height = dm_early.h;
            }
        }
    }
    // Clamp to sensible range in case of virtual/headless environments
    if (display_width  < 800)  display_width  = 1280;
    if (display_height < 600)  display_height = 720;

    sdl_window   = nullptr;
    SDL_Renderer *sdl_renderer = nullptr;

    current_window_scale = 1;

    char win_title[128];
    snprintf(win_title, sizeof(win_title), "VidyaOS Desktop");

    // Initialize libvterm with larger terminal dimensions for native resolution
    spawn_bash_pty();
    vterm_instance = vterm_new(TERM_ROWS, TERM_COLS);
    if (vterm_instance) {
        vterm_set_utf8(vterm_instance, 1);
        vterm_screen = vterm_obtain_screen(vterm_instance);
        VTermScreenCallbacks cb = {};
        cb.movecursor = nullptr;
        vterm_screen_set_callbacks(vterm_screen, &cb, nullptr);
        vterm_screen_reset(vterm_screen, 1);
        VTermState *vs = vterm_obtain_state(vterm_instance);
        if (vs) {
            VTermValue vv; vv.boolean = 0;
            vterm_state_set_termprop(vs, VTERM_PROP_ALTSCREEN, &vv);
        }
        vterm_output_set_callback(vterm_instance, term_output_cb, (void*)(intptr_t)pty_master_fd);
    }

    #ifdef VIDYAOS_KMSDRM
    sdl_window = SDL_CreateWindow(
        "VidyaOS",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        0, 0,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
    );
    #else
    sdl_window = SDL_CreateWindow(
        win_title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        display_width, display_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    SDL_SetWindowMinimumSize(sdl_window, 800, 600);
    #endif

    if (sdl_window) {
        sdl_renderer = SDL_CreateRenderer(sdl_window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        if (sdl_renderer) {
            // Initialise TrueType font system
            if (!init_fonts(sdl_renderer)) {
                fprintf(stderr, "[VidyaOS] Font init failed — text will be blank\n");
            }
            SDL_StartTextInput();
            SDL_ShowCursor(SDL_ENABLE);  // Use native OS cursor

            // Load settings dynamically from the virtual JSON store
            load_settings_from_file();
        }
    }

    if (!sdl_window || !sdl_renderer) {
        fprintf(stderr, "[SDL2] Window/Renderer failed: %s\n", SDL_GetError());
        fprintf(stderr, "[SDL2] Running in headless CLI mode only.\n");
    } else {
        printf("[VidyaOS] Native %dx%d window ready — SDL2+TTF modern desktop UI\n",
               display_width, display_height);
        printf("[VidyaOS] You can also type shell commands in THIS terminal!\n");
        fflush(stdout);
    }


    // ── Zephyr-only: use Zephyr display driver ──
    #ifdef __ZEPHYR__
    #if DT_NODE_HAS_STATUS(DT_NODELABEL(sdl_dc), okay)
    const struct device *display_dev = DEVICE_DT_GET(DT_NODELABEL(sdl_dc));
    struct display_capabilities caps;
    struct display_buffer_descriptor buf_desc;
    if (device_is_ready(display_dev)) {
        display_blanking_off(display_dev);
        display_get_capabilities(display_dev, &caps);
        memset(&buf_desc, 0, sizeof(buf_desc));
        buf_desc.width    = caps.x_resolution;
        buf_desc.pitch    = caps.x_resolution;
        buf_desc.height   = caps.y_resolution;
        buf_desc.buf_size = buf_size;
    }
    #endif
    #endif

    if (sensor_dev && !device_is_ready(sensor_dev)) {
        printk("Warning: Virtual sensor not ready!\n");
    }

    int loop_ticks = 0;
    bool running   = true;

    // ── Main render + event loop ──
    while (running) {
        update_terminal_pty();

        // ── Poll SDL2 events ──
        // SDL_RenderWindowToLogical (called inside handler) correctly maps
        // from any physical window resolution → virtual 320×240 coordinates.
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }
            handle_sdl_event(ev, sdl_renderer);
        }

        // ── 1-second periodic tasks ──
        loop_ticks++;
        if (loop_ticks >= 10) {
            loop_ticks = 0;

            // Check updates progress simulation
            if (update_check_progress >= 0 && update_check_progress < 100) {
                update_check_progress += 20;
                if (update_check_progress >= 100) {
                    update_check_progress = 100;
                    last_update_check_time = "Just Now";
                    push_notification("Updates check complete. System up to date.");
                }
            }

            // Browser downloads progress simulation
            for (auto &dl : browser_downloads) {
                if (!dl.completed && dl.progress < 100) {
                    dl.progress += 25;
                    if (dl.progress >= 100) {
                        dl.progress = 100;
                        dl.completed = true;
                        push_notification("Downloaded " + dl.filename);
                        
                        if (dl.filename.find(".flatpak") != std::string::npos) {
                            std::string app_name = dl.filename.substr(0, dl.filename.find(".flatpak"));
                            for (auto &app : appstore_apps) {
                                if (app.package_name == app_name) {
                                    apt_installed_packages[app_name] = true;
                                    push_notification("Flatpak app registered: " + app.name);
                                    
                                    std::string bin_path = "/usr/bin/" + app_name;
                                    std::ofstream out(translate_path(bin_path));
                                    if (out.is_open()) {
                                        out << "#!/bin/sh\necho \"Running " << app.name << " via Flatpak sandboxing\"\n";
                                        out.close();
                                    }
                                    set_default_metadata(bin_path, false);
                                    sync_sandbox_to_virtual_files();
                                }
                            }
                        }
                    }
                }
            }

            // App Store installation progress
            if (appstore_installing_idx >= 0 && appstore_installing_idx < (int)appstore_apps.size()) {
                appstore_progress += 20;
                if (appstore_progress >= 100) {
                    appstore_progress = 100;
                    std::string pkg = appstore_apps[appstore_installing_idx].package_name;
                    apt_installed_packages[pkg] = true;
                    std::string bin_path = "/usr/bin/" + pkg;
                    std::ofstream out(translate_path(bin_path));
                    if (out.is_open()) {
                        out << "#!/bin/sh\necho \"Running " << appstore_apps[appstore_installing_idx].name << "\"\n";
                        out.close();
                    }
                    set_default_metadata(bin_path, false);
                    sync_sandbox_to_virtual_files();
                    
                    push_notification("Installed " + appstore_apps[appstore_installing_idx].name);
                    appstore_installing_idx = -1;
                    appstore_progress = 0;
                }
            }

            // Check for cloud status IPC from daemon
            std::string status_path = translate_path("/var/run/cloud_status.json");
            if (fs::exists(status_path)) {
                std::ifstream f(status_path);
                if (f.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    f.close();
                    fs::remove(status_path);

                    if (content.find("\"success\"") != std::string::npos) {
                        if (content.find("Restore") != std::string::npos || content.find("restore") != std::string::npos) {
                            load_settings_from_file();
                            push_notification("Cloud restore successful");
                        } else {
                            push_notification("Cloud sync successful");
                        }
                    } else {
                        push_notification("Cloud operation failed");
                    }
                    sync_sandbox_to_virtual_files();
                }
            }

            // Real telemetry and sensor reading
            current_sensor_value = get_temp();

            sensor_history.push_back(current_sensor_value);
            if (sensor_history.size() > 25)
                sensor_history.erase(sensor_history.begin());

            if (sensor_logging_enabled)
                printk("[SENSOR] Reading: %d\n", current_sensor_value);

            telemetry_cpu_usage = get_cpu_usage();
            telemetry_ram_usage = get_ram_usage_mb();

            // Pushes values into rolling monitor graphs
            cpu_history.push_back(telemetry_cpu_usage);
            if (cpu_history.size() > 20) cpu_history.erase(cpu_history.begin());
            ram_history.push_back(telemetry_ram_usage);
            if (ram_history.size() > 20) ram_history.erase(ram_history.begin());

            char json_buf[128];
            snprintf(json_buf, sizeof(json_buf),
                     "{\"cpu\": %d, \"ram\": %d, \"temp\": %d}",
                     telemetry_cpu_usage, telemetry_ram_usage, current_sensor_value);
            telemetry_json_output = std::string(json_buf);
            virtual_files["/var/run/telemetry.json"] = telemetry_json_output;
        }

        // ── Render frame directly to SDL2 renderer ──
        int current_w = display_width;
        int current_h = display_height;
        if (sdl_renderer) {
            SDL_GetRendererOutputSize(sdl_renderer, &current_w, &current_h);
        }
        draw_desktop(sdl_renderer, current_w, current_h);

        // ── Present rendered frame ──
        if (sdl_renderer) {
            SDL_RenderPresent(sdl_renderer);
        }

        // ── Zephyr display driver write ──
        #ifdef __ZEPHYR__
        #if DT_NODE_HAS_STATUS(DT_NODELABEL(sdl_dc), okay)
        if (device_is_ready(display_dev)) {
            display_write(display_dev, 0, 0, &buf_desc, fb_buf);
        }
        #endif
        #endif

        k_msleep(33); // ~30 FPS
    }

    // ── Cleanup ──
    close_fonts();
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (sdl_window)   SDL_DestroyWindow(sdl_window);
    SDL_Quit();

    return 0;
}