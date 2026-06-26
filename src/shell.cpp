#include "shell.h"
#include "platform.h"
#include "state.h"
#include "footprint.h"
#include <string.h>
#include <stdio.h>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>

#ifdef VIDYAOS_NATIVE
#include "platform_linux.h"
#endif

namespace fs = std::filesystem;
#include <string>
#include <vector>
#include <sstream>
#include <stdlib.h>
#include <set>
#include <algorithm>

static std::vector<std::string> split_string(const std::string &str) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Redirection state
static bool cmd_redirect_active = false;
static std::string cmd_redirect_buffer = "";



// Directory checking helper
static bool is_directory(const std::string& path) {
    std::string resolved = resolve_path(path);
    std::string real_path = translate_path(resolved);
    struct stat st;
    if (stat(real_path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

// Output helper that supports redirection buffering, printing to console, and logging to GUI
static void shell_out(const std::string& text) {
    if (cmd_redirect_active) {
        cmd_redirect_buffer += text;
    } else {
        printf("%s", text.c_str());
        fflush(stdout);
        // Strip ANSI colors
        std::string clean = "";
        bool in_ansi = false;
        for (size_t i = 0; i < text.length(); ++i) {
            if (text[i] == '\033') {
                in_ansi = true;
            } else if (in_ansi && text[i] == 'm') {
                in_ansi = false;
            } else if (!in_ansi) {
                clean += text[i];
            }
        }
        if (!clean.empty()) {
            std::istringstream iss(clean);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    add_terminal_log(line);
                }
            }
        }
    }
}

// Redirection structures and parser
struct RedirectionInfo {
    std::vector<std::string> args;
    bool has_redirection = false;
    std::string filepath = "";
    bool append = false;
};

static RedirectionInfo parse_redirection(const std::vector<std::string>& original_args) {
    RedirectionInfo info;
    info.args = original_args;
    if (original_args.size() < 3) return info;
    
    for (int i = (int)original_args.size() - 2; i >= 0; --i) {
        if (original_args[i] == ">" || original_args[i] == ">>") {
            info.has_redirection = true;
            info.filepath = original_args[i+1];
            info.append = (original_args[i] == ">>");
            info.args.erase(info.args.begin() + i, info.args.end());
            break;
        }
    }
    return info;
}

bool check_permission(const std::string& path, bool write_req) {
#ifdef VIDYAOS_NATIVE
    return check_permission_native(path, write_req);
#else
    if (currentUser == "root") return true;
    std::string resolved = resolve_path(path);
    
    if (resolved.rfind("/home/", 0) == 0 && resolved.length() > 6) {
        size_t next_slash = resolved.find('/', 6);
        std::string home_owner = resolved.substr(6, next_slash == std::string::npos ? std::string::npos : next_slash - 6);
        if (home_owner != currentUser && home_owner != "root") {
            return false;
        }
    }
    if (write_req) {
        if (resolved.rfind("/root", 0) == 0 ||
            resolved.rfind("/etc", 0) == 0 ||
            resolved.rfind("/usr", 0) == 0 ||
            resolved.rfind("/var", 0) == 0) {
            return false;
        }
    }

    if (file_metadata.count(resolved) == 0) {
        return true;
    }
    const auto& meta = file_metadata[resolved];
    if (meta.owner == currentUser) {
        int mask = write_req ? 2 : 4;
        return (meta.permissions & (mask << 6)) != 0;
    }
    int mask = write_req ? 2 : 4;
    return (meta.permissions & mask) != 0;
#endif
}

void set_default_metadata(const std::string& path, bool is_dir) {
    std::string resolved = resolve_path(path);
    if (file_metadata.count(resolved) == 0) {
        FileMeta meta;
        meta.owner = currentUser;
        meta.permissions = is_dir ? 0755 : 0644;
        file_metadata[resolved] = meta;
    }
}

static bool write_redirected_output(const std::string& path, const std::string& content, bool append) {
    if (!check_permission(path, true)) {
        printf(ANSI_RED "bash: %s: Permission denied\n" ANSI_RESET, path.c_str());
        fflush(stdout);
        add_terminal_log("bash: " + path + ": Permission denied");
        return false;
    }
    std::string resolved = resolve_path(path);
    std::string real_path = translate_path(resolved);
    
    size_t last_slash = resolved.find_last_of('/');
    if (last_slash != std::string::npos && last_slash > 0) {
        std::string parent = resolved.substr(0, last_slash);
        if (!is_directory(parent)) {
            printf(ANSI_RED "bash: %s: No such file or directory\n" ANSI_RESET, path.c_str());
            fflush(stdout);
            add_terminal_log("bash: " + path + ": No such file or directory");
            return false;
        }
    }
    std::ofstream out(real_path, append ? std::ios::app : std::ios::trunc);
    if (out.is_open()) {
        out << content;
        out.close();
        set_default_metadata(resolved, false);
    } else {
        printf(ANSI_RED "bash: %s: Permission denied\n" ANSI_RESET, path.c_str());
        fflush(stdout);
        return false;
    }
    sync_sandbox_to_virtual_files();
    mft.notify_change(resolved);
    mft.update(resolved);
    return true;
}

void print_help() {
    printf(ANSI_BOLD ANSI_CYAN "\n=== Vidya OS - Available Commands ===\n" ANSI_RESET);
    printf(ANSI_BOLD "System Controls:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "lock" ANSI_RESET "               - Locks the console. Passcode required.\n");
    printf("  " ANSI_GREEN "sleep" ANSI_RESET "              - Simulates putting the system to sleep.\n");
    printf("  " ANSI_GREEN "reset" ANSI_RESET "              - Reboots the OS/simulator.\n");
    printf("  " ANSI_GREEN "shutdown" ANSI_RESET "           - Terminates the simulator.\n");
    printf(ANSI_BOLD "Desktop & Mouse controls:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "desktop list" ANSI_RESET "       - Lists status of all desktop windows.\n");
    printf("  " ANSI_GREEN "desktop open <app>" ANSI_RESET " - Opens a window (terminal, files, monitor, chrome).\n");
    printf("  " ANSI_GREEN "desktop close <app>" ANSI_RESET "- Closes a window.\n");
    printf("  " ANSI_GREEN "mouse move <x> <y>" ANSI_RESET "  - Moves virtual mouse to specific coordinates.\n");
    printf("  " ANSI_GREEN "mouse click" ANSI_RESET "        - Clicks at current cursor position.\n");
    printf(ANSI_BOLD "Hardware & Sensor:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "sensor status" ANSI_RESET "      - Reads the current virtual sensor value.\n");
    printf("  " ANSI_GREEN "sensor log start" ANSI_RESET "   - Enables periodic sensor printing to console.\n");
    printf("  " ANSI_GREEN "sensor log stop" ANSI_RESET "    - Disables periodic sensor printing.\n");
    printf(ANSI_BOLD "Device Manager:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "device list" ANSI_RESET "        - Lists all devices registered in Zephyr.\n");
    printf("  " ANSI_GREEN "device status <name>" ANSI_RESET "- Checks if a device is ready.\n");
    printf(ANSI_BOLD "File Manager:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "file list" ANSI_RESET "          - Lists all mock files and sizes.\n");
    printf("  " ANSI_GREEN "file read <name>" ANSI_RESET "    - Reads a mock file's content.\n");
    printf("  " ANSI_GREEN "file write <name> <val>" ANSI_RESET "- Creates/writes content to a mock file.\n");
    printf("  " ANSI_GREEN "file rm <name>" ANSI_RESET "      - Deletes a mock file.\n");
    printf(ANSI_BOLD "GNU Core Utilities & Redirection:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "pwd" ANSI_RESET "                - Prints the current working directory.\n");
    printf("  " ANSI_GREEN "cd <dir>" ANSI_RESET "           - Changes the current directory.\n");
    printf("  " ANSI_GREEN "ls [-l] [-a] [path]" ANSI_RESET " - Lists directory files/subfolders.\n");
    printf("  " ANSI_GREEN "cat <file1> [file2]" ANSI_RESET " - Displays the contents of virtual files.\n");
    printf("  " ANSI_GREEN "echo <text>" ANSI_RESET "        - Prints text. Supports redirection (> and >>).\n");
    printf("  " ANSI_GREEN "grep [-i] <pat> <file>" ANSI_RESET " - Searches for a pattern in a file.\n");
    printf("  " ANSI_GREEN "mkdir [-p] <path>" ANSI_RESET "  - Creates a simulated directory.\n");
    printf("  " ANSI_GREEN "touch <path>" ANSI_RESET "       - Creates an empty virtual file.\n");
    printf("  " ANSI_GREEN "cp [-r] <src> <dest>" ANSI_RESET " - Copies files/folders recursively.\n");
    printf("  " ANSI_GREEN "mv <src> <dest>" ANSI_RESET "      - Moves/renames files/folders.\n");
    printf("  " ANSI_GREEN "rm [-r] <path>" ANSI_RESET "       - Deletes files/folders recursively.\n");
    printf(ANSI_BOLD "Linux & Package Manager:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "apt list" ANSI_RESET "           - Lists all available packages.\n");
    printf("  " ANSI_GREEN "apt install <pkg>" ANSI_RESET "  - Installs a package (chrome, neofetch, cmatrix, python, java, g++, gcc, nodejs).\n");
    printf("  " ANSI_GREEN "apt remove <pkg>" ANSI_RESET "   - Removes a package.\n");
    printf("  " ANSI_GREEN "neofetch" ANSI_RESET "           - Displays Linux-like system information.\n");
    printf("  " ANSI_GREEN "cmatrix" ANSI_RESET "            - Triggers animated green code rain screen.\n");
    printf("  " ANSI_GREEN "python" ANSI_RESET "             - Starts the interactive Python REPL.\n");
    printf("  " ANSI_GREEN "python <file.py>" ANSI_RESET "    - Runs a virtual Python script.\n");
    printf("  " ANSI_GREEN "javac <file.java>" ANSI_RESET "   - Compiles a Java source file.\n");
    printf("  " ANSI_GREEN "java <classname>" ANSI_RESET "    - Runs a compiled Java class.\n");
    printf("  " ANSI_GREEN "g++ <file.cpp>" ANSI_RESET "      - Compiles a C++ source file.\n");
    printf("  " ANSI_GREEN "gcc <file.c>" ANSI_RESET "        - Compiles a C source file.\n");
    printf("  " ANSI_GREEN "node" ANSI_RESET "               - Starts the interactive JavaScript/Node REPL.\n");
    printf("  " ANSI_GREEN "node <file.js>" ANSI_RESET "      - Runs a virtual JavaScript script.\n");
    printf(ANSI_BOLD "Memory Footprint Change Tracker:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "footprint snapshot" ANSI_RESET "     - Takes a snapshot footprint of the system.\n");
    printf("  " ANSI_GREEN "footprint diff" ANSI_RESET "         - Lists changes between active state and snapshot.\n");
    printf("  " ANSI_GREEN "footprint update [path]" ANSI_RESET " - Updates specified path (or all) in snapshot.\n");
    printf(ANSI_BOLD "Phase 3 Simulator Utilities:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "network status/scan" ANSI_RESET "    - Display and scan networks.\n");
    printf("  " ANSI_GREEN "network connect/disconnect" ANSI_RESET "- Manage Wi-Fi connections.\n");
    printf("  " ANSI_GREEN "user add/del/list/switch" ANSI_RESET "  - Multi-user authentication & switching.\n");
    printf("  " ANSI_GREEN "whoami" ANSI_RESET "             - Prints current user.\n");
    printf("  " ANSI_GREEN "chmod <mode> <file>" ANSI_RESET "    - Change file permissions.\n");
    printf("  " ANSI_GREEN "chown <user> <file>" ANSI_RESET "    - Change file owner.\n");
    printf("  " ANSI_GREEN "cloud sync/restore/status" ANSI_RESET " - Node.js Cloud Daemon Sync.\n");
    printf("  " ANSI_GREEN "theme list/apply/export" ANSI_RESET "  - Custom visual themes engine.\n");
    printf("  " ANSI_GREEN "workspace <0-3|list>" ANSI_RESET "   - Switch and check workspaces.\n");
    printf(ANSI_BOLD "Maintenance:\n" ANSI_RESET);
    printf("  " ANSI_GREEN "upgrade" ANSI_RESET "            - Performs mock OS firmware upgrade and reboots.\n");
    printf("  " ANSI_GREEN "help" ANSI_RESET "               - Displays this help menu.\n\n");
    fflush(stdout);
}

void run_python_line(const std::string& line) {
    if (line.empty()) return;
    auto trim = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };
    std::string l = trim(line);
    if (l == "exit()" || l == "quit()") {
        python_repl_active = false;
        printf("Exited Python REPL.\n");
        fflush(stdout);
        add_terminal_log("Exited Python REPL.");
        return;
    }
    size_t eq = l.find('=');
    if (eq != std::string::npos && eq > 0 && eq < l.length() - 1) {
        std::string var = trim(l.substr(0, eq));
        std::string val = trim(l.substr(eq + 1));
        if (val.rfind("read_file(", 0) == 0 && val.back() == ')') {
            std::string arg = trim(val.substr(10, val.length() - 11));
            if (arg.length() >= 2 && (arg.front() == '"' || arg.front() == '\'') && (arg.back() == '"' || arg.back() == '\'')) {
                std::string fname = arg.substr(1, arg.length() - 2);
                if (virtual_files.count(fname)) {
                    python_variables[var] = virtual_files[fname];
                } else {
                    printf("FileNotFoundError: No such file or directory: '%s'\n", fname.c_str()); fflush(stdout);
                    add_terminal_log("FileNotFoundError: " + fname);
                }
                return;
            }
        }
        if (val.length() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.length() - 2);
        else if (val.length() >= 2 && val.front() == '\'' && val.back() == '\'') val = val.substr(1, val.length() - 2);
        python_variables[var] = val;
        return;
    }
    if (l.rfind("print(", 0) == 0 && l.back() == ')') {
        std::string arg = trim(l.substr(6, l.length() - 7));
        if (arg.length() >= 2 && arg.front() == '"' && arg.back() == '"') {
            std::string content = arg.substr(1, arg.length() - 2);
            printf("%s\n", content.c_str()); fflush(stdout);
            add_terminal_log(content);
        }
        else if (arg.length() >= 2 && arg.front() == '\'' && arg.back() == '\'') {
            std::string content = arg.substr(1, arg.length() - 2);
            printf("%s\n", content.c_str()); fflush(stdout);
            add_terminal_log(content);
        }
        else {
            if (python_variables.count(arg)) {
                std::string content = python_variables[arg];
                printf("%s\n", content.c_str()); fflush(stdout);
                add_terminal_log(content);
            } else {
                printf("NameError: name '%s' is not defined\n", arg.c_str()); fflush(stdout);
                add_terminal_log("NameError: name '" + arg + "' is not defined");
            }
        }
        return;
    }
    size_t plus = l.find('+');
    size_t minus = l.find('-');
    size_t mult = l.find('*');
    if (plus != std::string::npos || minus != std::string::npos || mult != std::string::npos) {
        char op = '+';
        size_t op_pos = plus;
        if (minus != std::string::npos) { op = '-'; op_pos = minus; }
        else if (mult != std::string::npos) { op = '*'; op_pos = mult; }
        std::string s_left = trim(l.substr(0, op_pos));
        std::string s_right = trim(l.substr(op_pos + 1));
        int left = 0, right = 0;
        bool left_ok = false, right_ok = false;
        if (python_variables.count(s_left)) {
            left = atoi(python_variables[s_left].c_str());
            left_ok = true;
        } else {
            left = atoi(s_left.c_str());
            left_ok = !s_left.empty();
        }
        if (python_variables.count(s_right)) {
            right = atoi(python_variables[s_right].c_str());
            right_ok = true;
        } else {
            right = atoi(s_right.c_str());
            right_ok = !s_right.empty();
        }
        if (left_ok && right_ok) {
            int res = 0;
            if (op == '+') res = left + right;
            else if (op == '-') res = left - right;
            else if (op == '*') res = left * right;
            std::string s_res = std::to_string(res);
            printf("%s\n", s_res.c_str()); fflush(stdout);
            add_terminal_log(s_res);
            return;
        }
    }
    if (python_variables.count(l)) {
        std::string content = python_variables[l];
        printf("%s\n", content.c_str()); fflush(stdout);
        add_terminal_log(content);
        return;
    }
    printf("SyntaxError: invalid syntax\n"); fflush(stdout);
    add_terminal_log("SyntaxError: invalid syntax");
}

void run_javascript_line(const std::string& line) {
    if (line.empty()) return;
    auto trim = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };
    std::string l = trim(line);
    if (!l.empty() && l.back() == ';') {
        l = trim(l.substr(0, l.length() - 1));
    }
    if (l == "exit()" || l == "quit()" || l == ".exit") {
        javascript_repl_active = false;
        printf("Exited Node.js REPL.\n");
        fflush(stdout);
        add_terminal_log("Exited Node.js REPL.");
        return;
    }
    bool is_decl = false;
    if (l.rfind("let ", 0) == 0) { is_decl = true; l = trim(l.substr(4)); }
    else if (l.rfind("const ", 0) == 0) { is_decl = true; l = trim(l.substr(6)); }
    else if (l.rfind("var ", 0) == 0) { is_decl = true; l = trim(l.substr(4)); }
    size_t eq = l.find('=');
    if (eq != std::string::npos && eq > 0 && eq < l.length() - 1) {
        std::string var = trim(l.substr(0, eq));
        std::string val = trim(l.substr(eq + 1));
        if (val.length() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.length() - 2);
        else if (val.length() >= 2 && val.front() == '\'' && val.back() == '\'') val = val.substr(1, val.length() - 2);
        javascript_variables[var] = val;
        if (is_decl) {
            printf("undefined\n"); fflush(stdout);
            add_terminal_log("undefined");
        } else {
            printf("%s\n", val.c_str()); fflush(stdout);
            add_terminal_log(val);
        }
        return;
    }
    if (l.rfind("console.log(", 0) == 0 && l.back() == ')') {
        std::string arg = trim(l.substr(12, l.length() - 13));
        if (arg.length() >= 2 && arg.front() == '"' && arg.back() == '"') {
            std::string content = arg.substr(1, arg.length() - 2);
            printf("%s\n", content.c_str()); fflush(stdout);
            add_terminal_log(content);
        }
        else if (arg.length() >= 2 && arg.front() == '\'' && arg.back() == '\'') {
            std::string content = arg.substr(1, arg.length() - 2);
            printf("%s\n", content.c_str()); fflush(stdout);
            add_terminal_log(content);
        }
        else {
            if (javascript_variables.count(arg)) {
                std::string content = javascript_variables[arg];
                printf("%s\n", content.c_str()); fflush(stdout);
                add_terminal_log(content);
            } else {
                printf("ReferenceError: %s is not defined\n", arg.c_str()); fflush(stdout);
                add_terminal_log("ReferenceError: " + arg + " is not defined");
            }
        }
        return;
    }
    size_t plus = l.find('+');
    size_t minus = l.find('-');
    size_t mult = l.find('*');
    if (plus != std::string::npos || minus != std::string::npos || mult != std::string::npos) {
        char op = '+';
        size_t op_pos = plus;
        if (minus != std::string::npos) { op = '-'; op_pos = minus; }
        else if (mult != std::string::npos) { op = '*'; op_pos = mult; }
        std::string s_left = trim(l.substr(0, op_pos));
        std::string s_right = trim(l.substr(op_pos + 1));
        int left = 0, right = 0;
        bool left_ok = false, right_ok = false;
        if (javascript_variables.count(s_left)) {
            left = atoi(javascript_variables[s_left].c_str());
            left_ok = true;
        } else {
            left = atoi(s_left.c_str());
            left_ok = !s_left.empty();
        }
        if (javascript_variables.count(s_right)) {
            right = atoi(javascript_variables[s_right].c_str());
            right_ok = true;
        } else {
            right = atoi(s_right.c_str());
            right_ok = !s_right.empty();
        }
        if (left_ok && right_ok) {
            int res = 0;
            if (op == '+') res = left + right;
            else if (op == '-') res = left - right;
            else if (op == '*') res = left * right;
            std::string s_res = std::to_string(res);
            printf("%s\n", s_res.c_str()); fflush(stdout);
            add_terminal_log(s_res);
            return;
        }
    }
    if (javascript_variables.count(l)) {
        std::string content = javascript_variables[l];
        printf("%s\n", content.c_str()); fflush(stdout);
        add_terminal_log(content);
        return;
    }
    printf("SyntaxError: Unexpected token\n"); fflush(stdout);
    add_terminal_log("SyntaxError: Unexpected token");
}

struct RedirectionGuard {
    RedirectionInfo info;
    bool active;
    RedirectionGuard(const RedirectionInfo& redir) : info(redir), active(redir.has_redirection) {
        if (active) {
            cmd_redirect_active = true;
            cmd_redirect_buffer = "";
        }
    }
    ~RedirectionGuard() {
        if (active) {
            cmd_redirect_active = false;
            write_redirected_output(info.filepath, cmd_redirect_buffer, info.append);
            cmd_redirect_buffer = "";
        }
    }
};

void execute_os_command(const std::string& line_str) {
    if (python_repl_active) {
        run_python_line(line_str);
        return;
    }
    if (javascript_repl_active) {
        run_javascript_line(line_str);
        return;
    }

    if (!line_str.empty()) {
        add_terminal_log(line_str);
    }

    std::vector<std::string> args = split_string(line_str);
    if (args.empty()) {
        return;
    }

    RedirectionInfo redir = parse_redirection(args);
    RedirectionGuard guard(redir);
    if (redir.has_redirection) {
        args = redir.args;
        if (args.empty()) return;
    }

    std::string cmd = args[0];

    if (cmd == "help") {
        print_help();
    }
    else if (cmd == "footprint") {
        if (args.size() < 2) {
            shell_out(ANSI_RED "Usage: footprint <snapshot|diff|update> [path]\n" ANSI_RESET);
            return;
        }
        std::string sub = args[1];
        if (sub == "snapshot") {
            mft.snapshot();
            shell_out("Footprint stored in memory. Root hash: " + mft.get_root_hash() + "\n");
        }
        else if (sub == "diff") {
            sync_sandbox_to_virtual_files();
            mft.rebuild_active_footprint();
            std::vector<std::string> added, removed, modified;
            mft.diff(added, removed, modified);

            std::vector<std::string> added_files;
            std::vector<std::string> added_dirs;
            for (const auto& p : added) {
                if (mft.is_active_dir(p)) added_dirs.push_back(p);
                else added_files.push_back(p);
            }

            std::vector<std::string> removed_files;
            std::vector<std::string> removed_dirs;
            for (const auto& p : removed) {
                if (mft.is_stored_dir(p)) removed_dirs.push_back(p);
                else removed_files.push_back(p);
            }

            std::vector<std::string> modified_files;
            std::vector<std::string> modified_dirs;
            for (const auto& p : modified) {
                if (mft.is_active_dir(p)) modified_dirs.push_back(p);
                else modified_files.push_back(p);
            }

            if (added_files.empty() && added_dirs.empty() &&
                removed_files.empty() && removed_dirs.empty() &&
                modified_files.empty() && modified_dirs.empty()) {
                shell_out("No changes detected. Footprint is in sync.\n");
            } else {
                if (!added_files.empty()) {
                    shell_out("Added files:\n");
                    for (const auto& p : added_files) shell_out("  " + p + "\n");
                }
                if (!added_dirs.empty()) {
                    shell_out("Added directories:\n");
                    for (const auto& p : added_dirs) shell_out("  " + p + "\n");
                }
                if (!removed_files.empty()) {
                    shell_out("Removed files:\n");
                    for (const auto& p : removed_files) shell_out("  " + p + "\n");
                }
                if (!removed_dirs.empty()) {
                    shell_out("Removed directories:\n");
                    for (const auto& p : removed_dirs) shell_out("  " + p + "\n");
                }
                if (!modified_files.empty()) {
                    shell_out("Modified files:\n");
                    for (const auto& p : modified_files) shell_out("  " + p + "\n");
                }
                if (!modified_dirs.empty()) {
                    shell_out("Directory hashes changed (propagated):\n");
                    for (const auto& p : modified_dirs) shell_out("  " + p + "\n");
                }
            }
        }
        else if (sub == "update") {
            std::string target_path = "";
            if (args.size() > 2) {
                target_path = resolve_path(args[2]);
            }
            mft.update(target_path);
            if (target_path.empty()) {
                shell_out("Footprint up to date.\n");
            } else {
                shell_out("Footprint updated for " + target_path + ". New root hash: " + mft.get_root_hash() + "\n");
            }
        }
        else {
            shell_out(ANSI_RED "Unknown footprint subcommand. Try footprint <snapshot|diff|update>\n" ANSI_RESET);
        }
    }
    else if (cmd == "pwd") {
        shell_out(current_working_directory + "\n");
    }
    else if (cmd == "cd") {
        std::string target = "/";
        if (args.size() > 1) {
            target = args[1];
        }
        std::string resolved = resolve_path(target);
        if (is_directory(resolved)) {
            current_working_directory = resolved;
            chdir(translate_path(resolved).c_str());
        } else {
            shell_out(ANSI_RED "cd: " + target + ": No such file or directory\n" ANSI_RESET);
        }
    }
    else if (cmd == "ls") {
        bool opt_a = false;
        bool opt_l = false;
        std::vector<std::string> paths;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-a") opt_a = true;
            else if (args[i] == "-l") opt_l = true;
            else if (args[i] == "-la" || args[i] == "-al") { opt_a = true; opt_l = true; }
            else if (args[i][0] == '-' && args[i].length() > 1) {
                for (size_t j = 1; j < args[i].length(); j++) {
                    if (args[i][j] == 'a') opt_a = true;
                    else if (args[i][j] == 'l') opt_l = true;
                }
            } else {
                paths.push_back(args[i]);
            }
        }
        if (paths.empty()) {
            paths.push_back(current_working_directory);
        }

        for (size_t p = 0; p < paths.size(); p++) {
            std::string target = paths[p];
            std::string resolved = resolve_path(target);
            std::string real_path = translate_path(resolved);

            if (!fs::exists(real_path)) {
                shell_out(ANSI_RED "ls: cannot access '" + target + "': No such file or directory\n" ANSI_RESET);
                continue;
            }

            if (paths.size() > 1) {
                shell_out(target + ":\n");
            }

            if (!fs::is_directory(real_path)) {
                if (opt_l) {
                    struct stat st;
                    stat(real_path.c_str(), &st);
                    shell_out("-rw-r--r-- 1 vidya vidya " + std::to_string(st.st_size) + " Jun 23 14:00 " + target + "\n");
                } else {
                    shell_out(target + "\n");
                }
            } else {
                std::vector<std::string> contents;
                if (opt_a) {
                    contents.push_back(".");
                    contents.push_back("..");
                }
                try {
                    for (const auto& entry : fs::directory_iterator(real_path)) {
                        std::string fname = entry.path().filename().string();
                        if (!opt_a && fname[0] == '.') continue;
                        contents.push_back(fname);
                    }
                } catch (...) {}
                std::sort(contents.begin(), contents.end());

                for (const auto& entry : contents) {
                    std::string item_resolved = resolved == "/" ? ("/" + entry) : (resolved + "/" + entry);
                    std::string item_real = translate_path(item_resolved);
                    struct stat st;
                    stat(item_real.c_str(), &st);
                    bool is_dir = S_ISDIR(st.st_mode);

                    if (opt_l) {
                        std::string type_perms = is_dir ? "drwxr-xr-x" : "-rw-r--r--";
                        shell_out(type_perms + " 1 vidya vidya " + std::to_string(st.st_size) + " Jun 23 14:00 " + entry + "\n");
                    } else {
                        if (is_dir) {
                            shell_out(std::string(ANSI_BLUE) + entry + "/" + ANSI_RESET + "  ");
                        } else {
                            shell_out(entry + "  ");
                        }
                    }
                }
                if (!opt_l && !contents.empty()) {
                    shell_out("\n");
                }
            }
        }
    }
    else if (cmd == "cat") {
        if (args.size() < 2) {
            shell_out("cat: missing file operand\n");
        } else {
            for (size_t i = 1; i < args.size(); i++) {
                if (!check_permission(args[i], false)) {
                    shell_out("cat: " + args[i] + ": Permission denied\n");
                    continue;
                }
                std::string resolved = resolve_path(args[i]);
                std::string real_path = translate_path(resolved);
                struct stat st;
                if (stat(real_path.c_str(), &st) != 0) {
                    shell_out("cat: " + args[i] + ": No such file or directory\n");
                } else if (S_ISDIR(st.st_mode)) {
                    shell_out("cat: " + args[i] + ": Is a directory\n");
                } else {
                    std::ifstream in(real_path, std::ios::binary);
                    if (in.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                        shell_out(content + "\n");
                    } else {
                        shell_out("cat: " + args[i] + ": Permission denied\n");
                    }
                }
            }
        }
    }
    else if (cmd == "echo") {
        std::string text = "";
        for (size_t i = 1; i < args.size(); i++) {
            text += args[i] + (i == args.size() - 1 ? "" : " ");
        }
        if (text.length() >= 2 && text.front() == '"' && text.back() == '"') {
            text = text.substr(1, text.length() - 2);
        }
        else if (text.length() >= 2 && text.front() == '\'' && text.back() == '\'') {
            text = text.substr(1, text.length() - 2);
        }
        shell_out(text + "\n");
    }
    else if (cmd == "grep") {
        bool opt_i = false;
        std::vector<std::string> grep_args;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-i") opt_i = true;
            else grep_args.push_back(args[i]);
        }
        if (grep_args.size() < 2) {
            shell_out("Usage: grep [-i] <pattern> <file>\n");
        } else {
            std::string pattern = grep_args[0];
            std::string filepath = resolve_path(grep_args[1]);
            if (!virtual_files.count(filepath)) {
                shell_out("grep: " + grep_args[1] + ": No such file or directory\n");
            } else if (is_directory(filepath)) {
                shell_out("grep: " + grep_args[1] + ": Is a directory\n");
            } else {
                std::string content = virtual_files[filepath];
                std::istringstream iss(content);
                std::string line;
                std::string pat_lower = pattern;
                if (opt_i) {
                    std::transform(pat_lower.begin(), pat_lower.end(), pat_lower.begin(), ::tolower);
                }
                while (std::getline(iss, line)) {
                    std::string line_check = line;
                    if (opt_i) {
                        std::transform(line_check.begin(), line_check.end(), line_check.begin(), ::tolower);
                    }
                    if (line_check.find(pat_lower) != std::string::npos) {
                        shell_out(line + "\n");
                    }
                }
            }
        }
    }
    else if (cmd == "cp") {
        bool opt_r = false;
        std::vector<std::string> cp_paths;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-r" || args[i] == "-R") opt_r = true;
            else cp_paths.push_back(args[i]);
        }
        if (cp_paths.size() < 2) {
            shell_out("cp: missing file operand\n");
        } else {
            std::string src = resolve_path(cp_paths[0]);
            std::string dest = resolve_path(cp_paths[1]);
            std::string src_real = translate_path(src);
            std::string dest_real = translate_path(dest);
            
            if (!fs::exists(src_real)) {
                shell_out("cp: cannot stat '" + cp_paths[0] + "': No such file or directory\n");
            } else if (fs::is_directory(src_real)) {
                if (!opt_r) {
                    shell_out("cp: -r not specified; omitting directory '" + cp_paths[0] + "'\n");
                } else {
                    std::string actual_dest_real = dest_real;
                    std::string actual_dest_virt = dest;
                    if (fs::exists(dest_real) && fs::is_directory(dest_real)) {
                        size_t last_slash = src.find_last_of('/');
                        std::string src_dir_name = (last_slash == std::string::npos) ? src : src.substr(last_slash + 1);
                        actual_dest_virt = (dest == "/") ? ("/" + src_dir_name) : (dest + "/" + src_dir_name);
                        actual_dest_real = translate_path(actual_dest_virt);
                    }
                    try {
                        fs::copy(src_real, actual_dest_real, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                        sync_sandbox_to_virtual_files();
                        mft.notify_change(actual_dest_virt);
                        mft.update(actual_dest_virt);
                    } catch (...) {
                        shell_out("cp: error copying directory\n");
                    }
                }
            } else {
                std::string actual_dest_real = dest_real;
                std::string actual_dest_virt = dest;
                if (fs::exists(dest_real) && fs::is_directory(dest_real)) {
                    size_t last_slash = src.find_last_of('/');
                    std::string src_file_name = (last_slash == std::string::npos) ? src : src.substr(last_slash + 1);
                    actual_dest_virt = (dest == "/") ? ("/" + src_file_name) : (dest + "/" + src_file_name);
                    actual_dest_real = translate_path(actual_dest_virt);
                }
                try {
                    fs::copy_file(src_real, actual_dest_real, fs::copy_options::overwrite_existing);
                    sync_sandbox_to_virtual_files();
                    mft.notify_change(actual_dest_virt);
                    mft.update(actual_dest_virt);
                } catch (...) {
                    shell_out("cp: error copying file\n");
                }
            }
        }
    }
    else if (cmd == "mv") {
        if (args.size() < 3) {
            shell_out("mv: missing file operand\n");
        } else {
            std::string src = resolve_path(args[1]);
            std::string dest = resolve_path(args[2]);
            std::string src_real = translate_path(src);
            std::string dest_real = translate_path(dest);

            if (!fs::exists(src_real)) {
                shell_out("mv: cannot stat '" + args[1] + "': No such file or directory\n");
            } else {
                std::string actual_dest_real = dest_real;
                std::string actual_dest_virt = dest;
                if (fs::exists(dest_real) && fs::is_directory(dest_real)) {
                    size_t last_slash = src.find_last_of('/');
                    std::string src_name = (last_slash == std::string::npos) ? src : src.substr(last_slash + 1);
                    actual_dest_virt = (dest == "/") ? ("/" + src_name) : (dest + "/" + src_name);
                    actual_dest_real = translate_path(actual_dest_virt);
                }
                try {
                    fs::rename(src_real, actual_dest_real);
                    sync_sandbox_to_virtual_files();
                    mft.notify_change(src);
                    mft.update(src);
                    mft.notify_change(actual_dest_virt);
                    mft.update(actual_dest_virt);
                } catch (...) {
                    shell_out("mv: error moving file/directory\n");
                }
            }
        }
    }
    else if (cmd == "rm") {
        bool opt_r = false;
        std::vector<std::string> rm_paths;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-r" || args[i] == "-R" || args[i] == "-rf") opt_r = true;
            else rm_paths.push_back(args[i]);
        }
        if (rm_paths.empty()) {
            shell_out("rm: missing operand\n");
        } else {
            for (const auto& path_arg : rm_paths) {
                if (!check_permission(path_arg, true)) {
                    shell_out("rm: cannot remove '" + path_arg + "': Permission denied\n");
                    continue;
                }
                std::string resolved = resolve_path(path_arg);
                std::string real_path = translate_path(resolved);
                if (!fs::exists(real_path)) {
                    shell_out("rm: cannot remove '" + path_arg + "': No such file or directory\n");
                    continue;
                }
                if (fs::is_directory(real_path)) {
                    if (!opt_r) {
                        shell_out("rm: cannot remove '" + path_arg + "': Is a directory\n");
                    } else {
                        try {
                            fs::remove_all(real_path);
                            sync_sandbox_to_virtual_files();
                            mft.notify_change(resolved);
                            mft.update(resolved);
                        } catch (...) {
                            shell_out("rm: error removing directory\n");
                        }
                    }
                } else {
                    try {
                        fs::remove(real_path);
                        sync_sandbox_to_virtual_files();
                        mft.notify_change(resolved);
                        mft.update(resolved);
                    } catch (...) {
                        shell_out("rm: error removing file\n");
                    }
                }
            }
        }
    }
    else if (cmd == "mkdir") {
        bool opt_p = false;
        std::vector<std::string> mkdir_paths;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-p") opt_p = true;
            else mkdir_paths.push_back(args[i]);
        }
        if (mkdir_paths.empty()) {
            shell_out("mkdir: missing operand\n");
        } else {
            for (const auto& path_arg : mkdir_paths) {
                if (!check_permission(path_arg, true)) {
                    shell_out("mkdir: cannot create directory '" + path_arg + "': Permission denied\n");
                    continue;
                }
                std::string resolved = resolve_path(path_arg);
                std::string real_path = translate_path(resolved);
                if (fs::exists(real_path)) {
                    if (!opt_p) {
                        shell_out("mkdir: cannot create directory '" + path_arg + "': File exists\n");
                    }
                    continue;
                }
                try {
                    if (opt_p) {
                        fs::create_directories(real_path);
                    } else {
                        fs::create_directory(real_path);
                    }
                    sync_sandbox_to_virtual_files();
                    set_default_metadata(resolved, true);
                    mft.notify_change(resolved);
                    mft.update(resolved);
                } catch (...) {
                    shell_out("mkdir: cannot create directory '" + path_arg + "'\n");
                }
            }
        }
    }
    else if (cmd == "touch") {
        if (args.size() < 2) {
            shell_out("touch: missing file operand\n");
        } else {
            for (size_t i = 1; i < args.size(); i++) {
                if (!check_permission(args[i], true)) {
                    shell_out("touch: cannot touch '" + args[i] + "': Permission denied\n");
                    continue;
                }
                std::string resolved = resolve_path(args[i]);
                std::string real_path = translate_path(resolved);
                size_t last_slash = resolved.find_last_of('/');
                bool parent_ok = true;
                if (last_slash != std::string::npos && last_slash > 0) {
                    std::string parent = resolved.substr(0, last_slash);
                    if (!is_directory(parent)) {
                        shell_out("touch: cannot touch '" + args[i] + "': No such file or directory\n");
                        parent_ok = false;
                    }
                }
                if (parent_ok) {
                    std::ofstream out(real_path, std::ios::app);
                    if (out.is_open()) {
                        out.close();
                        fs::last_write_time(real_path, fs::file_time_type::clock::now());
                        sync_sandbox_to_virtual_files();
                        set_default_metadata(resolved, false);
                        mft.notify_change(resolved);
                        mft.update(resolved);
                    } else {
                        shell_out("touch: cannot touch '" + args[i] + "': Permission denied\n");
                    }
                }
            }
        }
    }
    else if (cmd == "lock") {
        system_locked = true;
        printf(ANSI_BOLD ANSI_YELLOW "[SYSTEM] Console locked. Press Enter to input passcode.\n" ANSI_RESET);
        fflush(stdout);
    } 
    else if (cmd == "sleep") {
        printf(ANSI_BOLD ANSI_BLUE "[SYSTEM] Putting OS to sleep. Thread suspended...\n" ANSI_RESET
               ANSI_YELLOW "[SYSTEM] Press ENTER to wake up the system.\n" ANSI_RESET);
        fflush(stdout);
        console_getline();
        printf(ANSI_BOLD ANSI_GREEN "[SYSTEM] System woke up successfully!\n" ANSI_RESET);
        fflush(stdout);
    } 
    else if (cmd == "reset") {
        printf(ANSI_BOLD ANSI_YELLOW "[SYSTEM] Rebooting system...\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(500);
        sys_reboot(SYS_REBOOT_COLD);
    } 
    else if (cmd == "shutdown") {
        printf(ANSI_BOLD ANSI_RED "[SYSTEM] Shutting down Vidya OS... Goodbye!\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(500);
        exit(0);
    } 
    else if (cmd == "gemini") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: gemini <query>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string telemetry = "{}";
        if (virtual_files.count("/var/run/telemetry.json")) {
            telemetry = virtual_files["/var/run/telemetry.json"];
        }
        printf(ANSI_BOLD ANSI_BLUE "Gemini CLI Assistant:\n" ANSI_RESET);
        printf("Analyzing telemetry data: %s\n", telemetry.c_str());
        k_msleep(1000);
        printf("Insight: Your hardware looks healthy based on current polling. If RAM usage exceeds 80%%, consider closing desktop windows.\n");
        fflush(stdout);
        
        add_terminal_log("Gemini: Hardware healthy. " + telemetry);
    } 
    else if (cmd == "desktop") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: desktop <list|open|close>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            printf(ANSI_BOLD ANSI_CYAN "Desktop Windows status:\n" ANSI_RESET);
            for (int i = 0; i < 8; i++) {
                if (i == 3 && !apt_installed_packages["chrome"]) continue;
                printf("  - %s: %s\n", windows[i].title,
                       windows[i].open ? (ANSI_GREEN "OPEN" ANSI_RESET) : (ANSI_RED "CLOSED" ANSI_RESET));
            }
            fflush(stdout);
        } 
        else if (sub == "open") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: desktop open <terminal|files|monitor|chrome|settings|controlpanel|editor|appstore>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            std::string app = args[2];
            if (app == "terminal") { windows[0].open = true; windows[0].minimized = false; bring_to_front(0); }
            else if (app == "files") { windows[1].open = true; windows[1].minimized = false; bring_to_front(1); }
            else if (app == "monitor") { windows[2].open = true; windows[2].minimized = false; bring_to_front(2); }
            else if (app == "chrome") {
                if (apt_installed_packages["chrome"]) {
                    windows[3].open = true;
                    windows[3].minimized = false;
                    bring_to_front(3);
                } else {
                    printf(ANSI_RED "Chrome is not installed. Use 'apt install chrome' first.\n" ANSI_RESET);
                }
            }
            else if (app == "settings") { windows[4].open = true; windows[4].minimized = false; bring_to_front(4); }
            else if (app == "controlpanel" || app == "panel") { windows[5].open = true; windows[5].minimized = false; bring_to_front(5); }
            else if (app == "editor" || app == "ide") { windows[6].open = true; windows[6].minimized = false; bring_to_front(6); }
            else if (app == "appstore" || app == "store") { windows[7].open = true; windows[7].minimized = false; bring_to_front(7); }
            else { printf(ANSI_RED "Unknown application name.\n" ANSI_RESET); }
            fflush(stdout);
        } 
        else if (sub == "close") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: desktop close <terminal|files|monitor|chrome|settings|controlpanel|editor|appstore>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            std::string app = args[2];
            if (app == "terminal") { windows[0].open = false; }
            else if (app == "files") { windows[1].open = false; }
            else if (app == "monitor") { windows[2].open = false; }
            else if (app == "chrome") { windows[3].open = false; chrome_address_active = false; }
            else if (app == "settings") { windows[4].open = false; }
            else if (app == "controlpanel" || app == "panel") { windows[5].open = false; }
            else if (app == "editor" || app == "ide") { windows[6].open = false; }
            else if (app == "appstore" || app == "store") { windows[7].open = false; }
            else { printf(ANSI_RED "Unknown application name.\n" ANSI_RESET); }
            fflush(stdout);
        }
    } 
    else if (cmd == "settings") {
        if (args.size() < 3 || args[1] != "set") {
            shell_out(ANSI_RED "Usage: settings set <key> <value>\n" ANSI_RESET);
            return;
        }
        std::string key = args[2];
        std::string val = (args.size() >= 4) ? args[3] : "";
        if (key == "theme") {
            is_dark_theme = (val == "dark");
            save_settings_to_file();
            push_notification("Theme changed to " + val);
            shell_out("Theme updated to: " + val + "\n");
        }
        else if (key == "wallpaper") {
            settings_wallpaper_idx = atoi(val.c_str());
            save_settings_to_file();
            shell_out("Wallpaper updated\n");
        }
        else if (key == "volume") {
            system_volume = atoi(val.c_str());
            save_settings_to_file();
            shell_out("Volume updated\n");
        }
        else if (key == "accent") {
            settings_accent_idx = atoi(val.c_str());
            save_settings_to_file();
            shell_out("Accent color updated\n");
        }
        else if (key == "fontSize") {
            settings_font_size = atoi(val.c_str());
            save_settings_to_file();
            shell_out("Font size updated\n");
        }
        else {
            shell_out(ANSI_RED "Unknown settings key.\n" ANSI_RESET);
        }
    }
    else if (cmd == "notifications") {
        if (args.size() >= 2 && args[1] == "clear") {
            notifications.clear();
            shell_out("Notifications cleared.\n");
        } else {
            shell_out(ANSI_BOLD "Recent Notifications:\n" ANSI_RESET);
            if (notifications.empty()) {
                shell_out("  No notifications.\n");
            } else {
                for (const auto& n : notifications) {
                    shell_out("  - " + n.message + "\n");
                }
            }
        }
    } 
    else if (cmd == "mouse") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: mouse <move|click|status>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string sub = args[1];
        if (sub == "move") {
            if (args.size() < 4) {
                printf(ANSI_RED "Usage: mouse move <x> <y>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            mouse_x = atoi(args[2].c_str());
            mouse_y = atoi(args[3].c_str());
            printf(ANSI_GREEN "Mouse cursor moved to (%d, %d)\n" ANSI_RESET, mouse_x, mouse_y);
            fflush(stdout);
        } 
        else if (sub == "status") {
            printf("Mouse Cursor Location: (%d, %d)\n", mouse_x, mouse_y);
            fflush(stdout);
        } 
        else if (sub == "click") {
            printf(ANSI_GREEN "Mouse Clicked at (%d, %d)\n" ANSI_RESET, mouse_x, mouse_y);
            fflush(stdout);
            handle_desktop_click(mouse_x, mouse_y);
        }
    } 
    else if (cmd == "sensor") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: sensor <status|log>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string sub = args[1];
        if (sub == "status") {
            if (sensor_dev && device_is_ready(sensor_dev)) {
                struct sensor_value sval;
                sensor_sample_fetch(sensor_dev);
                sensor_channel_get(sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &sval);
                printf(ANSI_GREEN "Sensor Reading: %d\n" ANSI_RESET, sval.val1);
            } else {
                // Software mock fallback
                static int mock_sensor_val = 0;
                mock_sensor_val += 5;
                if (mock_sensor_val > 100) mock_sensor_val = 0;
                printf(ANSI_GREEN "Sensor Reading (Software Mock): %d\n" ANSI_RESET, mock_sensor_val);
            }
            fflush(stdout);
        } 
        else if (sub == "log") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: sensor log <start|stop>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            std::string action = args[2];
            if (action == "start") {
                sensor_logging_enabled = true;
                printf(ANSI_GREEN "Periodic sensor logging started.\n" ANSI_RESET);
            } else if (action == "stop") {
                sensor_logging_enabled = false;
                printf(ANSI_GREEN "Periodic sensor logging stopped.\n" ANSI_RESET);
            } else {
                printf(ANSI_RED "Unknown log action. Use start or stop.\n" ANSI_RESET);
            }
            fflush(stdout);
        } else {
            printf(ANSI_RED "Unknown sensor subcommand.\n" ANSI_RESET);
            fflush(stdout);
        }
    } 
    else if (cmd == "device") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: device <list|status>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            const struct device *devices;
            size_t count = z_device_get_all_static(&devices);
            printf(ANSI_BOLD ANSI_CYAN "Found %zu device(s) registered in Zephyr:\n" ANSI_RESET, count);
            for (size_t i = 0; i < count; i++) {
                #ifdef __ZEPHYR__
                bool ready = device_is_ready(&devices[i]);
                #else
                bool ready = true;
                #endif
                printf("  - %s (%s)\n", devices[i].name, 
                       ready ? (ANSI_GREEN "READY" ANSI_RESET) : (ANSI_RED "NOT READY" ANSI_RESET));
            }
            fflush(stdout);
        } 
        else if (sub == "status") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: device status <name>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            std::string dev_name = args[2];
            const struct device *devices;
            size_t count = z_device_get_all_static(&devices);
            const struct device *found = NULL;
            for (size_t i = 0; i < count; i++) {
                if (dev_name == devices[i].name) {
                    found = &devices[i];
                    break;
                }
            }
            if (found) {
                #ifdef __ZEPHYR__
                bool ready = device_is_ready(found);
                #else
                bool ready = true;
                #endif
                printf("Device " ANSI_BOLD "%s" ANSI_RESET " is %s\n", dev_name.c_str(), 
                       ready ? (ANSI_GREEN "READY" ANSI_RESET) : (ANSI_RED "NOT READY" ANSI_RESET));
            } else {
                printf(ANSI_RED "Device '%s' not found.\n" ANSI_RESET, dev_name.c_str());
            }
            fflush(stdout);
        } else {
            printf(ANSI_RED "Unknown device subcommand.\n" ANSI_RESET);
            fflush(stdout);
        }
    } 
    else if (cmd == "file") {
        if (args.size() < 2) {
            printf(ANSI_RED "Usage: file <list|read|write|rm>\n" ANSI_RESET);
            fflush(stdout);
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            printf(ANSI_BOLD ANSI_CYAN "Virtual Files:\n" ANSI_RESET);
            if (virtual_files.empty()) {
                printf("  (No files exist)\n");
            } else {
                for (auto const& [name, content] : virtual_files) {
                    printf("  - %s (%zu bytes)\n", name.c_str(), content.size());
                }
            }
            fflush(stdout);
        } 
        else if (sub == "read") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: file read <name>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            if (!check_permission(args[2], false)) {
                printf(ANSI_RED "file: %s: Permission denied\n" ANSI_RESET, args[2].c_str());
                fflush(stdout);
                return;
            }
            std::string fname = resolve_path(args[2]);
            std::string real_path = translate_path(fname);
            std::ifstream in(real_path, std::ios::binary);
            if (in.is_open()) {
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                printf(ANSI_BOLD "--- Content of %s ---\n" ANSI_RESET, args[2].c_str());
                printf("%s\n", content.c_str());
                printf(ANSI_BOLD "-------------------------\n" ANSI_RESET);
            } else {
                printf(ANSI_RED "File '%s' not found.\n" ANSI_RESET, args[2].c_str());
            }
            fflush(stdout);
        } 
        else if (sub == "write") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: file write <name> <content>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            if (!check_permission(args[2], true)) {
                printf(ANSI_RED "file: %s: Permission denied\n" ANSI_RESET, args[2].c_str());
                fflush(stdout);
                return;
            }
            std::string fname = resolve_path(args[2]);
            std::string real_path = translate_path(fname);
            std::string content = "";
            for (size_t i = 3; i < args.size(); i++) {
                content += args[i] + (i == args.size() - 1 ? "" : " ");
            }
            size_t last_slash = fname.find_last_of('/');
            if (last_slash != std::string::npos && last_slash > 0) {
                std::string parent = fname.substr(0, last_slash);
                fs::create_directories(translate_path(parent));
            }
            std::ofstream out(real_path);
            if (out.is_open()) {
                out << content;
                out.close();
                set_default_metadata(fname, false);
                sync_sandbox_to_virtual_files();
                mft.notify_change(fname);
                mft.update(fname);
                printf(ANSI_GREEN "File '%s' written successfully.\n" ANSI_RESET, args[2].c_str());
            } else {
                printf(ANSI_RED "File '%s' write failed.\n" ANSI_RESET, args[2].c_str());
            }
            fflush(stdout);
        } 
        else if (sub == "rm") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: file rm <name>\n" ANSI_RESET);
                fflush(stdout);
                return;
            }
            if (!check_permission(args[2], true)) {
                printf(ANSI_RED "file: %s: Permission denied\n" ANSI_RESET, args[2].c_str());
                fflush(stdout);
                return;
            }
            std::string fname = resolve_path(args[2]);
            std::string real_path = translate_path(fname);
            if (fs::exists(real_path)) {
                fs::remove(real_path);
                sync_sandbox_to_virtual_files();
                mft.notify_change(fname);
                mft.update(fname);
                printf(ANSI_GREEN "File '%s' deleted successfully.\n" ANSI_RESET, args[2].c_str());
            } else {
                printf(ANSI_RED "File '%s' not found.\n" ANSI_RESET, args[2].c_str());
            }
            fflush(stdout);
        } else {
            printf(ANSI_RED "Unknown file subcommand.\n" ANSI_RESET);
            fflush(stdout);
        }
    } 
    else if (cmd == "upgrade") {
        printf(ANSI_BOLD ANSI_YELLOW "[UPGRADE] Fetching new image updates...\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(1000);
        
        printf(ANSI_BOLD ANSI_BLUE "[UPGRADE] Downloading firmware package: [                    ]  0%%" ANSI_RESET);
        fflush(stdout);
        for (int i = 1; i <= 20; i++) {
            k_msleep(150);
            std::string bar = "";
            for (int j = 0; j < i; j++) bar += "█";
            for (int j = i; j < 20; j++) bar += " ";
            printf("\r" ANSI_BOLD ANSI_BLUE "[UPGRADE] Downloading firmware package: [%s] %d%%" ANSI_RESET, bar.c_str(), i * 5);
            fflush(stdout);
        }
        printf("\n");
        k_msleep(500);

        printf(ANSI_BOLD ANSI_YELLOW "[UPGRADE] Verifying package integrity...\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(800);
        printf(ANSI_BOLD ANSI_GREEN "[UPGRADE] Checksum OK!\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(500);

        printf(ANSI_BOLD ANSI_YELLOW "[UPGRADE] Writing new image to flash...\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(1000);
        printf(ANSI_BOLD ANSI_GREEN "[UPGRADE] Firmware write complete. Rebooting to apply upgrades...\n" ANSI_RESET);
        fflush(stdout);
        k_msleep(1000);
        
        sys_reboot(SYS_REBOOT_COLD);
    } 
    else if (cmd == "apt") {
        if (args.size() < 2) {
            std::string msg1 = "apt - Package manager. Commands:";
            std::string msg2 = "  install <pkg>, remove <pkg>, list, update";
            printf("%s\n%s\n", msg1.c_str(), msg2.c_str()); fflush(stdout);
            add_terminal_log(msg1);
            add_terminal_log(msg2);
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            printf("Listing packages:\n");
            add_terminal_log("Listing packages:");
            for (auto const& [pkg, inst] : apt_installed_packages) {
                char buf[64];
                snprintf(buf, sizeof(buf), " - %s [installed: %s]", pkg.c_str(), inst ? "yes" : "no");
                printf("%s\n", buf);
                add_terminal_log(buf);
            }
            fflush(stdout);
        }
        else if (sub == "update") {
            printf("Hit:1 http://archive.vidyaos.org stable InRelease\nReading package lists... Done\n");
            fflush(stdout);
            add_terminal_log("Hit:1 http://archive.vidyaos.org");
            add_terminal_log("Reading package lists... Done");
        }
        else if (sub == "install") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: apt install <pkg1> [pkg2] ...\n" ANSI_RESET);
                fflush(stdout);
                add_terminal_log("Usage: apt install <pkg>");
                return;
            }
            printf("Reading package lists... Done\nBuilding dependency tree... Done\n");
            std::vector<std::string> to_install;
            for (size_t i = 2; i < args.size(); i++) {
                std::string pkg = args[i];
                if (apt_installed_packages.count(pkg) == 0) {
                    printf(ANSI_RED "E: Unable to locate package %s\n" ANSI_RESET, pkg.c_str());
                    fflush(stdout);
                    add_terminal_log("E: Unable to locate package " + pkg);
                    return;
                }
                if (apt_installed_packages[pkg]) {
                    printf("%s is already the newest version.\n", pkg.c_str());
                    fflush(stdout);
                    add_terminal_log(pkg + " is already installed.");
                } else {
                    to_install.push_back(pkg);
                }
            }
            if (to_install.empty()) return;

            printf("The following NEW packages will be installed:\n");
            for (const auto& pkg : to_install) {
                printf("  %s", pkg.c_str());
            }
            printf("\n");

            for (const auto& pkg : to_install) {
                add_terminal_log("Installing " + pkg + "...");
                fflush(stdout);
                for (int p = 0; p <= 100; p += 25) {
                    std::string progress = "Progress: " + std::to_string(p) + "% (" + pkg + ")";
                    printf("%s\r", progress.c_str());
                    fflush(stdout);
                    k_msleep(200);
                }
                printf("\nSetting up %s...\nDone!\n", pkg.c_str());
                fflush(stdout);
                apt_installed_packages[pkg] = true;
                
                std::string bin_path = "/usr/bin/" + pkg;
                std::string inst_path = "/etc/apt/installed/" + pkg;
                
                std::string bin_real = translate_path(bin_path);
                std::string inst_real = translate_path(inst_path);
                
                fs::create_directories(translate_path("/usr/bin"));
                fs::create_directories(translate_path("/etc/apt/installed"));
                
                std::ofstream out_bin(bin_real);
                if (out_bin.is_open()) { out_bin << "[binary for " << pkg << "]"; out_bin.close(); }
                std::ofstream out_inst(inst_real);
                if (out_inst.is_open()) { out_inst << "installed"; out_inst.close(); }
                
                sync_sandbox_to_virtual_files();
                mft.notify_change(bin_path);
                mft.update(bin_path);
                mft.notify_change(inst_path);
                mft.update(inst_path);
                
                add_terminal_log("Setting up " + pkg + "...");
                add_terminal_log("Done!");
            }
        }
        else if (sub == "remove" || sub == "purge") {
            if (args.size() < 3) {
                printf(ANSI_RED "Usage: apt remove <pkg1> [pkg2] ...\n" ANSI_RESET);
                fflush(stdout);
                add_terminal_log("Usage: apt remove <pkg>");
                return;
            }
            std::vector<std::string> to_remove;
            for (size_t i = 2; i < args.size(); i++) {
                std::string pkg = args[i];
                if (apt_installed_packages.count(pkg) == 0) {
                    printf(ANSI_RED "E: Package %s not found\n" ANSI_RESET, pkg.c_str());
                    fflush(stdout);
                    add_terminal_log("E: Package " + pkg + " not found");
                    return;
                }
                if (!apt_installed_packages[pkg]) {
                    printf("Package %s is not installed, so not removed.\n", pkg.c_str());
                    fflush(stdout);
                    add_terminal_log("Package " + pkg + " is not installed.");
                } else {
                    to_remove.push_back(pkg);
                }
            }
            for (const auto& pkg : to_remove) {
                printf("Removing %s...\nPurging configuration files...\nDone!\n", pkg.c_str());
                fflush(stdout);
                apt_installed_packages[pkg] = false;
                
                std::string bin_path = "/usr/bin/" + pkg;
                std::string inst_path = "/etc/apt/installed/" + pkg;
                
                fs::remove(translate_path(bin_path));
                fs::remove(translate_path(inst_path));
                
                sync_sandbox_to_virtual_files();
                mft.notify_change(bin_path);
                mft.update(bin_path);
                mft.notify_change(inst_path);
                mft.update(inst_path);
                
                if (pkg == "chrome") {
                    windows[3].open = false;
                    chrome_address_active = false;
                }
                else if (pkg == "cmatrix") {
                    cmatrix_active = false;
                }
                add_terminal_log("Removing " + pkg + "...");
                add_terminal_log("Done!");
            }
        }
    }
    else if (cmd == "neofetch") {
        if (!apt_installed_packages["neofetch"]) {
            printf("Command 'neofetch' not found. Try: apt install neofetch\n");
            fflush(stdout);
            add_terminal_log("Command 'neofetch' not found.");
            add_terminal_log("Try: apt install neofetch");
            return;
        }
        printf("     /\\_/\\      OS: Vidya OS 1.0 (Linux-like)\n");
        printf("    ( o.o )     Kernel: Standalone/Zephyr RTOS\n");
        printf("     > ^ <      Uptime: 2 min\n");
        printf("                Resolution: 320x240\n");
        printf("                DE: Vidya Desktop\n");
        printf("                Package Manager: apt\n");
        fflush(stdout);
        add_terminal_log("     /\\_/\\   OS: Vidya OS 1.0");
        add_terminal_log("    ( o.o )  Kernel: Standalone");
        add_terminal_log("     > ^ <   Resolution: 320x240");
        add_terminal_log("             DE: Vidya Desktop");
    }
    else if (cmd == "cmatrix") {
        if (!apt_installed_packages["cmatrix"]) {
            printf("Command 'cmatrix' not found. Try: apt install cmatrix\n");
            fflush(stdout);
            add_terminal_log("Command 'cmatrix' not found.");
            add_terminal_log("Try: apt install cmatrix");
            return;
        }
        cmatrix_active = true;
        printf("cmatrix started. Press any key in simulator window to exit.\n");
        fflush(stdout);
    }
    else if (cmd == "python" || cmd == "python3") {
        if (!apt_installed_packages["python"]) {
            printf("Command 'python' not found. Try: apt install python\n"); fflush(stdout);
            add_terminal_log("Command 'python' not found.");
            add_terminal_log("Try: apt install python");
            return;
        }
        if (args.size() > 1) {
            std::string fname = resolve_path(args[1]);
            std::string real_path = translate_path(fname);
            std::ifstream in(real_path);
            if (!in.is_open()) {
                printf("python: can't open file '%s': [Errno 2] No such file or directory\n", args[1].c_str()); fflush(stdout);
                add_terminal_log("python: can't open file '" + args[1] + "'");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                run_python_line(line);
            }
        } else {
            python_repl_active = true;
            printf("Python 3.10.12 (default, Jun 2026)\nType \"exit()\" or \"quit()\" to exit.\n"); fflush(stdout);
            add_terminal_log("Python 3.10.12 REPL");
            add_terminal_log("Type exit() to quit.");
        }
    }
    else if (cmd == "javac") {
        if (!apt_installed_packages["java"]) {
            printf("Command 'javac' not found. Try: apt install java\n"); fflush(stdout);
            add_terminal_log("Command 'javac' not found.");
            add_terminal_log("Try: apt install java");
            return;
        }
        if (args.size() < 2) {
            printf("javac: no source files\nUsage: javac <filename.java>\n"); fflush(stdout);
            add_terminal_log("javac: no source files");
            return;
        }
        std::string fname = resolve_path(args[1]);
        std::string real_path = translate_path(fname);
        std::ifstream in(real_path);
        if (!in.is_open()) {
            printf("error: file not found: %s\n", args[1].c_str()); fflush(stdout);
            add_terminal_log("error: file not found: " + args[1]);
            return;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t c_idx = content.find("class ");
        if (c_idx == std::string::npos) {
            printf("error: no class definition found in %s\n", args[1].c_str()); fflush(stdout);
            add_terminal_log("error: no class definition");
            return;
        }
        size_t name_start = c_idx + 6;
        size_t name_end = content.find_first_of(" \t{\n\r", name_start);
        if (name_end == std::string::npos) name_end = content.length();
        std::string classname = content.substr(name_start, name_end - name_start);
        
        size_t last_slash = fname.find_last_of('/');
        std::string dir_part = (last_slash == std::string::npos) ? "" : fname.substr(0, last_slash + 1);
        std::string out_class_path = dir_part + classname + ".class";
        
        java_compiled_classes[classname] = true;
        
        std::ofstream out(translate_path(out_class_path));
        if (out.is_open()) {
            out << "(compiled Java bytecode class)";
            out.close();
        }
        sync_sandbox_to_virtual_files();
        mft.notify_change(out_class_path);
        mft.update(out_class_path);
        
        printf("Compiled %s successfully.\n", args[1].c_str()); fflush(stdout);
        add_terminal_log("Compiled " + classname + ".class");
    }
    else if (cmd == "java") {
        if (!apt_installed_packages["java"]) {
            printf("Command 'java' not found. Try: apt install java\n"); fflush(stdout);
            add_terminal_log("Command 'java' not found.");
            add_terminal_log("Try: apt install java");
            return;
        }
        if (args.size() < 2) {
            printf("Usage: java <classname>\n"); fflush(stdout);
            add_terminal_log("Usage: java <classname>");
            return;
        }
        std::string classname = args[1];
        if (classname.length() > 6 && classname.substr(classname.length() - 6) == ".class") {
            classname = classname.substr(0, classname.length() - 6);
        }
        if (classname.length() > 5 && classname.substr(classname.length() - 5) == ".java") {
            classname = classname.substr(0, classname.length() - 5);
        }
        if (!java_compiled_classes[classname]) {
            printf("Error: Could not find or load main class %s\n", classname.c_str()); fflush(stdout);
            add_terminal_log("Error: class " + classname + " not compiled");
            return;
        }
        std::string src_name = resolve_path(classname + ".java");
        std::string real_src = translate_path(src_name);
        if (!fs::exists(real_src)) {
            std::string suffix = "/" + classname + ".java";
            for (const auto& [name, _] : virtual_files) {
                if (name.length() >= suffix.length() && name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0) {
                    src_name = name;
                    real_src = translate_path(src_name);
                    break;
                }
            }
        }
        std::ifstream in(real_src);
        if (!in.is_open()) {
            printf("Error: source code %s.java missing\n", classname.c_str()); fflush(stdout);
            add_terminal_log("Error: source code missing");
            return;
        }
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t idx = 0;
        bool found_print = false;
        while ((idx = src.find("System.out.println(", idx)) != std::string::npos) {
            size_t str_start = idx + 19;
            size_t str_end = src.find(")", str_start);
            if (str_end == std::string::npos) break;
            std::string print_arg = src.substr(str_start, str_end - str_start);
            if (print_arg.length() >= 2 && print_arg.front() == '"' && print_arg.back() == '"') {
                print_arg = print_arg.substr(1, print_arg.length() - 2);
            }
            else if (print_arg.length() >= 2 && print_arg.front() == '\'' && print_arg.back() == '\'') {
                print_arg = print_arg.substr(1, print_arg.length() - 2);
            }
            printf("%s\n", print_arg.c_str()); fflush(stdout);
            add_terminal_log(print_arg);
            idx = str_end;
            found_print = true;
        }
        if (!found_print) {
            printf("(Class ran with no output)\n"); fflush(stdout);
            add_terminal_log("(No output)");
        }
    }
    else if (cmd == "g++" || cmd == "gcc") {
        bool is_cpp = (cmd == "g++");
        std::string pkg = is_cpp ? "g++" : "gcc";
        if (!apt_installed_packages[pkg]) {
            printf("Command '%s' not found. Try: apt install %s\n", cmd.c_str(), pkg.c_str()); fflush(stdout);
            add_terminal_log("Command '" + cmd + "' not found.");
            add_terminal_log("Try: apt install " + pkg);
            return;
        }
        if (args.size() < 2) {
            printf("%s: fatal error: no input files\ncompilation terminated.\n", cmd.c_str()); fflush(stdout);
            add_terminal_log(cmd + ": fatal error: no input files");
            return;
        }
        std::string src_name = resolve_path(args[1]);
        std::string real_src = translate_path(src_name);
        if (!fs::exists(real_src)) {
            printf("%s: error: %s: No such file or directory\n", cmd.c_str(), args[1].c_str()); fflush(stdout);
            add_terminal_log(cmd + ": error: " + args[1] + " not found");
            return;
        }
        std::string out_name = "a.out";
        for (size_t a = 2; a < args.size(); a++) {
            if (args[a] == "-o" && a + 1 < args.size()) {
                out_name = args[a+1];
                break;
            }
        }
        std::string resolved_out = resolve_path(out_name);
        compiled_binaries[resolved_out] = src_name;
        
        std::ofstream out(translate_path(resolved_out));
        if (out.is_open()) {
            out << "(compiled executable binary)";
            out.close();
        }
        sync_sandbox_to_virtual_files();
        mft.notify_change(resolved_out);
        mft.update(resolved_out);
        
        printf("Compiled %s successfully.\n", out_name.c_str()); fflush(stdout);
        add_terminal_log("Compiled " + out_name);
    }
    else if (cmd == "node") {
        if (!apt_installed_packages["nodejs"]) {
            printf("Command 'node' not found. Try: apt install nodejs\n"); fflush(stdout);
            add_terminal_log("Command 'node' not found.");
            add_terminal_log("Try: apt install nodejs");
            return;
        }
        if (args.size() > 1) {
            std::string fname = resolve_path(args[1]);
            std::string real_path = translate_path(fname);
            std::ifstream in(real_path);
            if (!in.is_open()) {
                printf("node: can't open file '%s': [Errno 2] No such file or directory\n", args[1].c_str()); fflush(stdout);
                add_terminal_log("node: can't open file '" + args[1] + "'");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                run_javascript_line(line);
            }
        } else {
            javascript_repl_active = true;
            printf("Welcome to Node.js v18.16.0.\nType \".exit\" to exit.\n"); fflush(stdout);
            add_terminal_log("Node.js v18.16.0 REPL");
            add_terminal_log("Type .exit to quit.");
        }
    }
    else if (cmd.rfind("./", 0) == 0 && cmd.length() > 2) {
        std::string fname = resolve_path(cmd.substr(2));
        std::string real_path = translate_path(fname);
        if (!fs::exists(real_path)) {
            printf("bash: %s: No such file or directory\n", cmd.c_str()); fflush(stdout);
            add_terminal_log("bash: file not found");
            return;
        }
        if (fname.length() > 3 && fname.substr(fname.length() - 3) == ".sh") {
            std::ifstream in(real_path);
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    execute_os_command(line);
                }
            }
        }
        else if (fname.length() > 3 && fname.substr(fname.length() - 3) == ".py") {
            if (!apt_installed_packages["python"]) {
                printf("Error: python package required to run script.\n"); fflush(stdout);
                add_terminal_log("Error: python package required");
                return;
            }
            std::ifstream in(real_path);
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    run_python_line(line);
                }
            }
        }
        else if (fname.length() > 3 && fname.substr(fname.length() - 3) == ".js") {
            if (!apt_installed_packages["nodejs"]) {
                printf("Error: nodejs package required to run script.\n"); fflush(stdout);
                add_terminal_log("Error: nodejs package required");
                return;
            }
            std::ifstream in(real_path);
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    run_javascript_line(line);
                }
            }
        }
        else if (compiled_binaries.count(fname)) {
            if (!apt_installed_packages["g++"] && !apt_installed_packages["gcc"]) {
                printf("Error: compiler environment required to run compiled binaries.\n"); fflush(stdout);
                add_terminal_log("Error: compiler package required");
                return;
            }
            std::string src_name = compiled_binaries[fname];
            std::ifstream in(translate_path(src_name));
            if (!in.is_open()) {
                printf("bash: %s: missing source files for execution.\n", cmd.c_str()); fflush(stdout);
                add_terminal_log("bash: missing source files");
                return;
            }
            std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            size_t idx = 0;
            bool found_print = false;
            while ((idx = src.find("std::cout <<", idx)) != std::string::npos) {
                size_t str_start = src.find("\"", idx);
                if (str_start == std::string::npos) break;
                size_t str_end = src.find("\"", str_start + 1);
                if (str_end == std::string::npos) break;
                std::string print_arg = src.substr(str_start + 1, str_end - str_start - 1);
                printf("%s\n", print_arg.c_str()); fflush(stdout);
                add_terminal_log(print_arg);
                idx = str_end;
                found_print = true;
            }
            idx = 0;
            while ((idx = src.find("printf(", idx)) != std::string::npos) {
                size_t str_start = src.find("\"", idx);
                if (str_start == std::string::npos) break;
                size_t str_end = src.find("\"", str_start + 1);
                if (str_end == std::string::npos) break;
                std::string print_arg = src.substr(str_start + 1, str_end - str_start - 1);
                printf("%s\n", print_arg.c_str()); fflush(stdout);
                add_terminal_log(print_arg);
                idx = str_end;
                found_print = true;
            }
            if (!found_print) {
                printf("(Binary ran with no output)\n"); fflush(stdout);
                add_terminal_log("(No output)");
            }
        }
        else {
            printf("bash: %s: Command not found or permission denied\n", cmd.c_str()); fflush(stdout);
            add_terminal_log("bash: " + cmd + ": Command not found");
        }
    }
    else if (cmd == "bash" || cmd == "sh") {
        if (args.size() < 2) {
            printf("Usage: bash <script.sh>\n"); fflush(stdout);
            add_terminal_log("Usage: bash <script.sh>");
            return;
        }
        std::string fname = resolve_path(args[1]);
        std::string real_path = translate_path(fname);
        std::ifstream in(real_path);
        if (!in.is_open()) {
            printf("bash: %s: No such file or directory\n", args[1].c_str()); fflush(stdout);
            add_terminal_log("bash: file " + args[1] + " not found");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            execute_os_command(line);
        }
    }
    else if (cmd == "whoami") {
        printf("%s\n", currentUser.c_str()); fflush(stdout);
        add_terminal_log(currentUser);
    }
    else if (cmd == "user") {
        if (args.size() < 2) {
            printf("Usage: user <add|del|list|switch> [args]\n"); fflush(stdout);
            add_terminal_log("Usage: user <add|del|list|switch>");
            return;
        }
        std::string sub = args[1];
        if (sub == "add") {
            if (currentUser != "root") {
                printf("Error: Only root can add users.\n"); fflush(stdout);
                add_terminal_log("Error: root only");
                return;
            }
            if (args.size() < 4) {
                printf("Usage: user add <name> <passcode>\n"); fflush(stdout);
                add_terminal_log("Usage: user add <name> <passcode>");
                return;
            }
            std::string name = args[2];
            std::string pass = args[3];
#ifdef VIDYAOS_NATIVE
            if (add_user_native(name, pass)) {
                virtual_users = get_users_list_native();
                printf("User %s added successfully.\n", name.c_str()); fflush(stdout);
                add_terminal_log("User " + name + " added");
            } else {
                printf("Error: Failed to add user %s.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Error adding user");
            }
#else
            std::string pass_hash = sha256(pass);
            user_passwords[name] = pass_hash;
            bool found = false;
            for (const auto& u : virtual_users) {
                if (u == name) found = true;
            }
            if (!found) virtual_users.push_back(name);
            std::string home_dir = "/home/" + name;
            fs::create_directories(translate_path(home_dir));
            set_default_metadata(home_dir, true);
            file_metadata[resolve_path(home_dir)].owner = name;
            
            std::string passwd_file = translate_path("/etc/passwd");
            std::ofstream out(passwd_file, std::ios::app);
            if (out.is_open()) {
                out << name << ":" << pass_hash << ":" << (virtual_users.size() + 1000) << ":/home/" << name << "\n";
                out.close();
            }
            sync_sandbox_to_virtual_files();
            printf("User %s added successfully.\n", name.c_str()); fflush(stdout);
            add_terminal_log("User " + name + " added");
#endif
        }
        else if (sub == "del") {
            if (currentUser != "root") {
                printf("Error: Only root can delete users.\n"); fflush(stdout);
                add_terminal_log("Error: root only");
                return;
            }
            if (args.size() < 3) {
                printf("Usage: user del <name>\n"); fflush(stdout);
                add_terminal_log("Usage: user del <name>");
                return;
            }
            std::string name = args[2];
            if (name == "root" || name == "shri") {
                printf("Error: Cannot delete system user %s.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Error: cannot delete " + name);
                return;
            }
#ifdef VIDYAOS_NATIVE
            if (delete_user_native(name)) {
                virtual_users = get_users_list_native();
                printf("User %s deleted.\n", name.c_str()); fflush(stdout);
                add_terminal_log("User " + name + " deleted");
            } else {
                printf("Error: Failed to delete user %s.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Error deleting user");
            }
#else
            user_passwords.erase(name);
            for (auto it = virtual_users.begin(); it != virtual_users.end(); ++it) {
                if (*it == name) {
                    virtual_users.erase(it);
                    break;
                }
            }
            printf("User %s deleted.\n", name.c_str()); fflush(stdout);
            add_terminal_log("User " + name + " deleted");
#endif
        }
        else if (sub == "list") {
#ifdef VIDYAOS_NATIVE
            virtual_users = get_users_list_native();
#endif
            printf("Users:\n");
            for (const auto& u : virtual_users) {
                printf(" - %s\n", u.c_str());
            }
            fflush(stdout);
        }
        else if (sub == "switch") {
            if (args.size() < 3) {
                printf("Usage: user switch <name>\n"); fflush(stdout);
                add_terminal_log("Usage: user switch <name>");
                return;
            }
            std::string name = args[2];
#ifdef VIDYAOS_NATIVE
            virtual_users = get_users_list_native();
            bool user_exists = false;
            for (const auto& u : virtual_users) {
                if (u == name) user_exists = true;
            }
            if (!user_exists) {
                printf("Error: User %s does not exist.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Error: user not found");
                return;
            }
            printf("Enter passcode for %s: ", name.c_str()); fflush(stdout);
            char* pass = console_getline();
            std::string pass_str(pass);
            if (authenticate_user(name, pass_str)) {
#else
            if (user_passwords.count(name) == 0) {
                printf("Error: User %s does not exist.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Error: user not found");
                return;
            }
            printf("Enter passcode for %s: ", name.c_str()); fflush(stdout);
            char* pass = console_getline();
            std::string pass_str(pass);
            if (sha256(pass_str) == user_passwords[name]) {
#endif
                currentUser = name;
                current_working_directory = "/home/" + name;
                if (name == "root") current_working_directory = "/root";
                fs::create_directories(translate_path(current_working_directory));
                set_default_metadata(current_working_directory, true);
                printf("Switched to user %s.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Switched to " + name);
            } else {
                printf("Incorrect passcode.\n"); fflush(stdout);
                add_terminal_log("Auth failed for " + name);
            }
        }
    }
    else if (cmd == "network") {
        if (args.size() < 2) {
            printf("Usage: network <status|scan|connect|disconnect> [args]\n"); fflush(stdout);
            add_terminal_log("Usage: network <status|scan|connect|disconnect>");
            return;
        }
        std::string sub = args[1];
        if (sub == "status") {
#ifdef VIDYAOS_NATIVE
            update_network_telemetry_native();
#endif
            printf("Network Status:\n");
            printf("  Wi-Fi: %s\n", net_state.wifiOn ? "ON" : "OFF");
            printf("  Connected SSID: %s\n", net_state.wifiOn ? net_state.ssid.c_str() : "None");
            printf("  Signal Strength: %d/4\n", net_state.wifiOn ? net_state.signalStrength : 0);
            printf("  VPN: %s\n", net_state.vpn ? "ON" : "OFF");
            printf("  IP Address: %s\n", net_state.wifiOn ? net_state.ipAddress.c_str() : "0.0.0.0");
            fflush(stdout);
        }
        else if (sub == "scan") {
#ifdef VIDYAOS_NATIVE
            printf("Scanning for networks...\n"); fflush(stdout);
            std::vector<std::string> nw_list = scan_networks_native();
            for (const auto& nw : nw_list) {
                printf(" - SSID: %s\n", nw.c_str());
            }
            fflush(stdout);
#else
            printf("Scanning for networks...\n");
            for (const auto& nw : available_networks) {
                printf(" - SSID: %s [Signal: %d/4]\n", nw.c_str(), (nw == "VidyaNet" ? 4 : (nw == "HomeWiFi" ? 3 : 2)));
            }
            fflush(stdout);
#endif
        }
        else if (sub == "connect") {
            if (args.size() < 3) {
                printf("Usage: network connect <ssid>\n"); fflush(stdout);
                add_terminal_log("Usage: network connect <ssid>");
                return;
            }
            std::string ssid = args[2];
#ifdef VIDYAOS_NATIVE
            printf("Connecting to SSID: %s...\n", ssid.c_str()); fflush(stdout);
            if (connect_network_native(ssid)) {
                printf("Successfully connected to SSID: %s. IP: %s\n", ssid.c_str(), net_state.ipAddress.c_str()); fflush(stdout);
                add_terminal_log("Connected to " + ssid);
            } else {
                printf("Failed to connect to SSID: %s\n", ssid.c_str()); fflush(stdout);
                add_terminal_log("Connection failed");
            }
#else
            bool found = false;
            for (const auto& nw : available_networks) {
                if (nw == ssid) found = true;
            }
            if (found) {
                net_state.wifiOn = true;
                net_state.ssid = ssid;
                net_state.signalStrength = (ssid == "VidyaNet" ? 4 : (ssid == "HomeWiFi" ? 3 : 2));
                net_state.ipAddress = "192.168.1." + std::to_string(100 + (rand() % 50));
                printf("Successfully connected to SSID: %s. IP address assigned: %s\n", ssid.c_str(), net_state.ipAddress.c_str());
                add_terminal_log("Connected to " + ssid);
            } else {
                printf("SSID: %s not found in range.\n", ssid.c_str());
                add_terminal_log("SSID not found");
            }
            fflush(stdout);
#endif
        }
        else if (sub == "disconnect") {
#ifdef VIDYAOS_NATIVE
            disconnect_network_native();
            printf("Disconnected from Wi-Fi network.\n"); fflush(stdout);
            add_terminal_log("Disconnected from network");
#else
            net_state.wifiOn = false;
            net_state.ssid = "None";
            net_state.signalStrength = 0;
            net_state.ipAddress = "0.0.0.0";
            printf("Disconnected from Wi-Fi network.\n"); fflush(stdout);
            add_terminal_log("Disconnected from network");
#endif
        }
    }
    else if (cmd == "chmod") {
        if (args.size() < 3) {
            printf("Usage: chmod <mode> <file>\n"); fflush(stdout);
            add_terminal_log("Usage: chmod <mode> <file>");
            return;
        }
        std::string mode_str = args[1];
        std::string path = resolve_path(args[2]);
        if (virtual_files.count(path) == 0) {
            printf("chmod: cannot access '%s': No such file or directory\n", args[2].c_str()); fflush(stdout);
            add_terminal_log("chmod: file not found");
            return;
        }
#ifdef VIDYAOS_NATIVE
        try {
            int mode = std::stoi(mode_str, nullptr, 8);
            std::string real_path = translate_path(path);
            if (::chmod(real_path.c_str(), mode) == 0) {
                printf("Changed permissions of %s to %03o.\n", args[2].c_str(), mode); fflush(stdout);
                add_terminal_log("chmod " + mode_str + " successful");
            } else {
                printf("chmod: failed to change permissions of '%s': %s\n", args[2].c_str(), strerror(errno)); fflush(stdout);
                add_terminal_log("chmod failed");
            }
        } catch(...) {
            printf("chmod: invalid mode: '%s'\n", mode_str.c_str()); fflush(stdout);
        }
#else
        if (file_metadata.count(path) > 0) {
            if (currentUser != "root" && file_metadata[path].owner != currentUser) {
                printf("chmod: changing permissions of '%s': Permission denied\n", args[2].c_str()); fflush(stdout);
                add_terminal_log("chmod: permission denied");
                return;
            }
        }
        try {
            int mode = std::stoi(mode_str, nullptr, 8);
            if (file_metadata.count(path) == 0) {
                set_default_metadata(path, is_directory(path));
            }
            file_metadata[path].permissions = mode;
            printf("Changed permissions of %s to %03o.\n", args[2].c_str(), mode); fflush(stdout);
            add_terminal_log("chmod " + mode_str + " successful");
        } catch(...) {
            printf("chmod: invalid mode: '%s'\n", mode_str.c_str()); fflush(stdout);
        }
#endif
    }
    else if (cmd == "chown") {
        if (currentUser != "root") {
            printf("chown: changing ownership of: Permission denied (root only)\n"); fflush(stdout);
            add_terminal_log("chown: root only");
            return;
        }
        if (args.size() < 3) {
            printf("Usage: chown <user> <file>\n"); fflush(stdout);
            add_terminal_log("Usage: chown <user> <file>");
            return;
        }
        std::string owner = args[1];
        std::string path = resolve_path(args[2]);
        if (virtual_files.count(path) == 0) {
            printf("chown: cannot access '%s': No such file or directory\n", args[2].c_str()); fflush(stdout);
            add_terminal_log("chown: file not found");
            return;
        }
#ifdef VIDYAOS_NATIVE
        std::string real_path = translate_path(path);
        struct passwd *pwd = getpwnam(owner.c_str());
        if (pwd) {
            if (::chown(real_path.c_str(), pwd->pw_uid, pwd->pw_gid) == 0) {
                printf("Changed owner of %s to %s.\n", args[2].c_str(), owner.c_str()); fflush(stdout);
                add_terminal_log("chown successful");
            } else {
                printf("chown: failed to change owner of '%s': %s\n", args[2].c_str(), strerror(errno)); fflush(stdout);
                add_terminal_log("chown failed");
            }
        } else {
            printf("chown: invalid user: '%s'\n", owner.c_str()); fflush(stdout);
            add_terminal_log("chown: invalid user");
        }
#else
        bool user_exists = false;
        for (const auto& u : virtual_users) {
            if (u == owner) user_exists = true;
        }
        if (!user_exists) {
            printf("chown: invalid user: '%s'\n", owner.c_str()); fflush(stdout);
            add_terminal_log("chown: invalid user");
            return;
        }
        if (file_metadata.count(path) == 0) {
            set_default_metadata(path, is_directory(path));
        }
        file_metadata[path].owner = owner;
        printf("Changed owner of %s to %s.\n", args[2].c_str(), owner.c_str()); fflush(stdout);
        add_terminal_log("chown successful");
#endif
    }
    else if (cmd == "cloud") {
        if (args.size() < 2) {
            printf("Usage: cloud <sync|restore|status>\n"); fflush(stdout);
            add_terminal_log("Usage: cloud <sync|restore|status>");
            return;
        }
        std::string sub = args[1];
        std::string cmd_path = translate_path("/var/run/cloud_cmd.json");
        std::string status_path = translate_path("/var/run/cloud_status.json");
        if (sub == "sync") {
            printf("Cloud backup in progress...\n"); fflush(stdout);
            std::ofstream out(cmd_path);
            if (out.is_open()) {
                out << "{\"command\": \"sync\"}\n";
                out.close();
            }
            k_msleep(1000);
            std::ifstream in(status_path);
            if (in.is_open()) {
                printf("Cloud backup succeeded. Settings and directory footprints are synced.\n");
                add_terminal_log("Cloud sync complete");
            } else {
                printf("Cloud service unavailable.\n");
                add_terminal_log("Cloud sync failed");
            }
            fflush(stdout);
        }
        else if (sub == "restore") {
            printf("Restoring settings from cloud...\n"); fflush(stdout);
            std::ofstream out(cmd_path);
            if (out.is_open()) {
                out << "{\"command\": \"restore\"}\n";
                out.close();
            }
            k_msleep(1000);
            std::ifstream in(status_path);
            if (in.is_open()) {
                load_settings_from_file();
                printf("Cloud restore succeeded. System settings reloaded.\n");
                add_terminal_log("Cloud restore complete");
            } else {
                printf("Cloud service unavailable.\n");
                add_terminal_log("Cloud restore failed");
            }
            fflush(stdout);
        }
        else if (sub == "status") {
            std::ifstream in(status_path);
            if (in.is_open()) {
                printf("Cloud Sync Status: CONNECTED\nLast successful sync: Just Now\n");
            } else {
                printf("Cloud Sync Status: OFFLINE (Daemon offline)\n");
            }
            fflush(stdout);
        }
    }
    else if (cmd == "theme") {
        if (args.size() < 2) {
            printf("Usage: theme <list|apply|export|import> [args]\n"); fflush(stdout);
            add_terminal_log("Usage: theme <list|apply|export|import>");
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            printf("Installed Themes:\n");
            for (const auto& th : installed_themes) {
                printf(" - %s %s\n", th.name.c_str(), (th.name == active_theme.name ? "(active)" : ""));
            }
            fflush(stdout);
        }
        else if (sub == "apply") {
            if (args.size() < 3) {
                printf("Usage: theme apply <name>\n"); fflush(stdout);
                add_terminal_log("Usage: theme apply <name>");
                return;
            }
            std::string name = args[2];
            bool found = false;
            for (const auto& th : installed_themes) {
                if (th.name == name) {
                    active_theme = th;
                    is_dark_theme = (name != "light");
                    settings_wallpaper_idx = th.wallpaper;
                    save_settings_to_file();
                    found = true;
                    printf("Theme '%s' applied.\n", name.c_str()); fflush(stdout);
                    add_terminal_log("Applied theme " + name);
                    break;
                }
            }
            if (!found) {
                printf("Error: Theme '%s' not found.\n", name.c_str()); fflush(stdout);
                add_terminal_log("Theme not found");
            }
        }
        else if (sub == "export") {
            if (args.size() < 3) {
                printf("Usage: theme export <name>\n"); fflush(stdout);
                add_terminal_log("Usage: theme export <name>");
                return;
            }
            std::string name = args[2];
            bool found = false;
            for (const auto& th : installed_themes) {
                if (th.name == name) {
                    std::string exp_path = "/home/" + currentUser + "/" + name + ".json";
                    std::ofstream out(translate_path(exp_path));
                    if (out.is_open()) {
                        out << "{\n  \"name\": \"" << th.name << "\",\n  \"wallpaper\": " << th.wallpaper << "\n}\n";
                        out.close();
                        set_default_metadata(exp_path, false);
                        sync_sandbox_to_virtual_files();
                        printf("Theme '%s' exported to %s.\n", name.c_str(), exp_path.c_str()); fflush(stdout);
                        add_terminal_log("Exported theme " + name);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("Error: Theme '%s' not found.\n", name.c_str()); fflush(stdout);
            }
        }
        else if (sub == "import") {
            if (args.size() < 3) {
                printf("Usage: theme import <path>\n"); fflush(stdout);
                add_terminal_log("Usage: theme import <path>");
                return;
            }
            std::string path = resolve_path(args[2]);
            if (virtual_files.count(path) == 0) {
                printf("theme: file not found: %s\n", args[2].c_str()); fflush(stdout);
                return;
            }
            Theme new_theme = active_theme;
            new_theme.name = "imported_" + std::to_string(installed_themes.size());
            installed_themes.push_back(new_theme);
            printf("Theme imported successfully from %s.\n", args[2].c_str()); fflush(stdout);
            add_terminal_log("Imported theme");
        }
    }
    else if (cmd == "workspace") {
        if (args.size() < 2) {
            printf("Usage: workspace <0-3|list>\n"); fflush(stdout);
            add_terminal_log("Usage: workspace <0-3|list>");
            return;
        }
        std::string sub = args[1];
        if (sub == "list") {
            printf("Workspaces:\n");
            for (int w = 0; w < 4; w++) {
                printf(" Workspace %d:\n", w);
                int count = 0;
                for (int i = 0; i < 8; i++) {
                    if (windows[i].workspace == w && windows[i].open) {
                        printf("   - %s\n", windows[i].title);
                        count++;
                    }
                }
                if (count == 0) printf("   (Empty)\n");
            }
            fflush(stdout);
        } else {
            try {
                int ws = std::stoi(sub);
                if (ws >= 0 && ws <= 3) {
                    current_workspace = ws;
                    printf("Switched to workspace %d.\n", ws); fflush(stdout);
                    add_terminal_log("Workspace switched to " + sub);
                } else {
                    printf("Workspace index out of bounds (0-3).\n"); fflush(stdout);
                }
            } catch(...) {
                printf("Usage: workspace <0-3|list>\n"); fflush(stdout);
            }
        }
    }
    else {
        printf(ANSI_RED "Command '%s' not recognized. Type 'help' for options.\n" ANSI_RESET, cmd.c_str());
        fflush(stdout);
    }
}

void shell_thread_entry(void *, void *, void *) {
    k_msleep(200); // Wait slightly for booting sequence to complete printing
    console_getline_init();
    
    mft.load_from_disk("/var/footprint.db");
    
    print_help();

    while (true) {
        if (system_locked) {
            printf(ANSI_BOLD ANSI_RED "[LOCKED] Enter passcode to unlock: " ANSI_RESET);
            fflush(stdout);
            char *line = console_getline();
            std::string line_str(line);
            if (line_str == lock_password) {
                system_locked = false;
                printf(ANSI_BOLD ANSI_GREEN "[SYSTEM] Passcode correct. OS Unlocked!\n" ANSI_RESET);
            } else {
                printf(ANSI_BOLD ANSI_RED "[ERROR] Invalid passcode. Access denied.\n" ANSI_RESET);
            }
            fflush(stdout);
            continue;
        }

        if (python_repl_active) {
            printf(">>> ");
        } else if (javascript_repl_active) {
            printf("> ");
        } else {
            printf(ANSI_BOLD ANSI_CYAN "VidyaOS> " ANSI_RESET);
        }
        fflush(stdout);
        char *line = console_getline();
        std::string line_str(line);

        execute_os_command(line_str);
    }
}
