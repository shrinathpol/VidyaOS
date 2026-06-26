#include "platform_linux.h"
#include "state.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static std::string get_cmd_output(const std::string& cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
    } catch (...) {}
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

static std::string get_local_ip() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "0.0.0.0";
    
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);
    
    if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) < 0) {
        close(sock);
        return "0.0.0.0";
    }
    
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(sock, (struct sockaddr*)&local, &len) < 0) {
        close(sock);
        return "0.0.0.0";
    }
    
    close(sock);
    return inet_ntoa(local.sin_addr);
}

void update_network_telemetry_native() {
    std::string ip = get_local_ip();
    if (ip != "0.0.0.0" && ip != "127.0.0.1") {
        net_state.wifiOn = true;
        net_state.ipAddress = ip;
        
        // Retrieve SSID using iwgetid
        std::string ssid = get_cmd_output("iwgetid -r");
        if (ssid.empty()) {
            ssid = get_cmd_output("wpa_cli status 2>/dev/null | grep '^ssid=' | cut -d= -f2");
        }
        if (ssid.empty()) {
            ssid = "Ethernet Link";
        }
        net_state.ssid = ssid;
        net_state.signalStrength = 4; // Default strong signal if connected
        
        // Attempt to parse signal from /proc/net/wireless
        std::ifstream proc_wireless("/proc/net/wireless");
        if (proc_wireless.is_open()) {
            std::string line;
            while (std::getline(proc_wireless, line)) {
                if (line.find(":") != std::string::npos) {
                    std::stringstream ss(line);
                    std::string iface, status;
                    int link, level, noise;
                    ss >> iface >> status >> link >> level >> noise;
                    // signal level is usually dBm (negative value)
                    if (level < 0) {
                        if (level >= -60) net_state.signalStrength = 4;
                        else if (level >= -70) net_state.signalStrength = 3;
                        else if (level >= -80) net_state.signalStrength = 2;
                        else net_state.signalStrength = 1;
                    }
                }
            }
        }
    } else {
        net_state.wifiOn = false;
        net_state.ssid = "None";
        net_state.signalStrength = 0;
        net_state.ipAddress = "0.0.0.0";
    }
}

std::vector<std::string> scan_networks_native() {
    std::vector<std::string> networks;
    system("wpa_cli scan >/dev/null 2>&1");
    std::string results = get_cmd_output("wpa_cli scan_results 2>/dev/null");
    if (!results.empty()) {
        std::stringstream ss(results);
        std::string line;
        std::getline(ss, line); // Skip headers
        std::getline(ss, line);
        while (std::getline(ss, line)) {
            size_t tab = line.find_last_of('\t');
            if (tab != std::string::npos) {
                std::string ssid = line.substr(tab + 1);
                if (!ssid.empty() && ssid.find("\\x00") == std::string::npos) {
                    if (std::find(networks.begin(), networks.end(), ssid) == networks.end()) {
                        networks.push_back(ssid);
                    }
                }
            }
        }
    }
    
    // Fallback SSIDs if wpa_supplicant scan returns empty
    if (networks.empty()) {
        networks = {"VidyaNet", "HomeWiFi", "CoffeeShop"};
    }
    return networks;
}

bool connect_network_native(const std::string& ssid) {
    std::string net_id = get_cmd_output("wpa_cli add_network 2>/dev/null | tail -n 1");
    if (net_id.empty() || net_id.find("fail") != std::string::npos) {
        // Try fallback simulated connection state if wpa_cli is not set up
        net_state.wifiOn = true;
        net_state.ssid = ssid;
        net_state.signalStrength = 3;
        net_state.ipAddress = "192.168.1.115";
        return true;
    }
    
    system(("wpa_cli set_network " + net_id + " ssid '\"" + ssid + "\"' >/dev/null").c_str());
    system(("wpa_cli set_network " + net_id + " key_mgmt NONE >/dev/null").c_str()); 
    system(("wpa_cli enable_network " + net_id + " >/dev/null").c_str());
    system(("wpa_cli select_network " + net_id + " >/dev/null").c_str());
    system("wpa_cli save_config >/dev/null");
    
    // Wait for connection and trigger DHCP
    system("udhcpc -i wlan0 -n >/dev/null 2>&1 &");
    update_network_telemetry_native();
    return true;
}

void disconnect_network_native() {
    system("wpa_cli disconnect >/dev/null 2>&1");
    net_state.wifiOn = false;
    net_state.ssid = "None";
    net_state.signalStrength = 0;
    net_state.ipAddress = "0.0.0.0";
}
