#include "platform_linux.h"
#include "state.h"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

namespace fs = std::filesystem;

bool check_permission_native(const std::string& path, bool write_req) {
    std::string resolved = resolve_path(path);
    std::string real_path = translate_path(resolved);
    
    // root has access to everything
    if (currentUser == "root") return true;
    
    struct stat st;
    if (stat(real_path.c_str(), &st) != 0) {
        // If file doesn't exist, check parent directory write permission for write requests
        if (write_req) {
            size_t last_slash = resolved.find_last_of('/');
            if (last_slash != std::string::npos) {
                std::string parent = resolved.substr(0, last_slash);
                if (parent.empty()) parent = "/";
                return check_permission_native(parent, true);
            }
        }
        return true; 
    }
    
    std::string owner = "root";
    struct passwd *pwd = getpwuid(st.st_uid);
    if (pwd) owner = pwd->pw_name;
    
    int mode = st.st_mode;
    if (owner == currentUser) {
        int mask = write_req ? S_IWUSR : S_IRUSR;
        return (mode & mask) != 0;
    }
    
    int mask = write_req ? S_IWOTH : S_IROTH;
    return (mode & mask) != 0;
}

void sync_sandbox_to_virtual_files_native() {
    virtual_files.clear();
    
    // 1. Sidebar directories
    std::vector<std::string> tree_paths = {"/", "/home", "/etc", "/etc/apt", "/usr", "/var"};
    for (const auto& d : tree_paths) {
        virtual_files[d] = "<dir>";
    }
    
    // 2. Active folder contents
    std::string current_dir = file_manager_current_dir;
    if (current_dir.empty()) current_dir = "/";
    
    virtual_files[current_dir] = "<dir>";
    
    try {
        if (fs::exists(current_dir) && fs::is_directory(current_dir)) {
            for (const auto& entry : fs::directory_iterator(current_dir)) {
                std::string path = entry.path().string();
                if (path.empty()) continue;
                if (path[0] != '/') path = "/" + path;
                
                if (entry.is_directory()) {
                    virtual_files[path] = "<dir>";
                } else if (entry.is_regular_file()) {
                    auto size = fs::file_size(entry.path());
                    if (size < 512 * 1024) {
                        std::ifstream file(entry.path(), std::ios::binary);
                        if (file.is_open()) {
                            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            virtual_files[path] = content;
                        } else {
                            virtual_files[path] = "";
                        }
                    } else {
                        virtual_files[path] = std::string(size, ' ');
                    }
                }
            }
        }
    } catch (...) {}
}
