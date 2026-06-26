#include "platform_linux.h"
#include "state.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <crypt.h>
#include <unistd.h>
#include <shadow.h>
#include <pwd.h>

bool authenticate_user(const std::string& username, const std::string& password) {
    std::string encrypted_hash = "";
    
    // 1. Attempt to read from the shadow password database
    struct spwd *sh = getspnam(username.c_str());
    if (sh && sh->sp_pwdp) {
        encrypted_hash = sh->sp_pwdp;
    } else {
        // 2. Fall back to standard passwd database
        struct passwd *pw = getpwnam(username.c_str());
        if (pw && pw->pw_passwd) {
            encrypted_hash = pw->pw_passwd;
        }
    }
    
    if (encrypted_hash.empty() || encrypted_hash == "x" || encrypted_hash == "*" || encrypted_hash == "!") {
        // If system database doesn't have it or is locked, check our fallback virtual database
        if (user_passwords.count(username) > 0) {
            extern std::string sha256(const std::string& str);
            return sha256(password) == user_passwords[username];
        }
        return false;
    }
    
    char *res = crypt(password.c_str(), encrypted_hash.c_str());
    if (res) {
        return strcmp(res, encrypted_hash.c_str()) == 0;
    }
    return false;
}

bool add_user_native(const std::string& username, const std::string& password) {
    // 1. Spawns useradd or BusyBox adduser
    std::string cmd = "useradd -m -s /bin/bash " + username + " >/dev/null 2>&1";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        cmd = "useradd -m " + username + " >/dev/null 2>&1";
        ret = system(cmd.c_str());
    }
    if (ret != 0) {
        cmd = "adduser -D -h /home/" + username + " -s /bin/sh " + username + " >/dev/null 2>&1";
        ret = system(cmd.c_str());
    }
    
    // 2. Set passcode via chpasswd
    if (ret == 0 || ret == 9 || ret == 256) { // 9 or 256 indicates user already exists in some systems
        std::string pwd_cmd = "echo \"" + username + ":" + password + "\" | chpasswd >/dev/null 2>&1";
        int pwd_ret = system(pwd_cmd.c_str());
        return pwd_ret == 0;
    }
    return false;
}

bool delete_user_native(const std::string& username) {
    int ret = system(("userdel -r " + username + " >/dev/null 2>&1").c_str());
    if (ret != 0) {
        ret = system(("deluser --remove-home " + username + " >/dev/null 2>&1").c_str());
    }
    return ret == 0;
}

std::vector<std::string> get_users_list_native() {
    std::vector<std::string> user_list;
    std::ifstream file("/etc/passwd");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string username, dummy, uid_str;
            if (std::getline(ss, username, ':') &&
                std::getline(ss, dummy, ':') &&
                std::getline(ss, uid_str, ':')) {
                try {
                    int uid = std::stoi(uid_str);
                    if (uid == 0 || uid >= 1000) {
                        user_list.push_back(username);
                    }
                } catch (...) {
                    if (username == "root") {
                        user_list.push_back(username);
                    }
                }
            }
        }
    }
    
    if (user_list.empty()) {
        user_list = {"root", "shri"};
    }
    return user_list;
}
