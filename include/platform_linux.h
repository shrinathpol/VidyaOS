#ifndef PLATFORM_LINUX_H
#define PLATFORM_LINUX_H

#include <string>
#include <vector>

// --- User Database Operations ---
bool authenticate_user(const std::string& username, const std::string& password);
bool add_user_native(const std::string& username, const std::string& password);
bool delete_user_native(const std::string& username);
std::vector<std::string> get_users_list_native();

// --- Network Operations ---
void update_network_telemetry_native();
std::vector<std::string> scan_networks_native();
bool connect_network_native(const std::string& ssid);
void disconnect_network_native();

// --- Filesystem Operations ---
bool check_permission_native(const std::string& path, bool write_req);
void sync_sandbox_to_virtual_files_native();

#endif // PLATFORM_LINUX_H
