#include "footprint.h"
#include "state.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <cstdint>
#include <cstring>

// Global instance of the FootprintTracker
FootprintTracker mft;

// -------------------------------------------------------------
//  SHA-256 implementation
// -------------------------------------------------------------
const uint32_t SHA256::k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

SHA256::SHA256() {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    bitlen = 0;
    datalen = 0;
}

void SHA256::transform(const uint8_t *data) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256::update(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        this->data[datalen] = data[i];
        datalen++;
        if (datalen == 64) {
            transform(this->data);
            bitlen += 512;
            datalen = 0;
        }
    }
}

void SHA256::update(const std::string& str) {
    update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::string SHA256::finalize() {
    uint8_t i = datalen;

    if (datalen < 56) {
        data[i++] = 0x80;
        while (i < 56)
            data[i++] = 0x00;
    } else {
        data[i++] = 0x80;
        while (i < 64)
            data[i++] = 0x00;
        transform(data);
        memset(data, 0, 56);
    }

    bitlen += datalen * 8;
    data[63] = bitlen;
    data[62] = bitlen >> 8;
    data[61] = bitlen >> 16;
    data[60] = bitlen >> 24;
    data[59] = bitlen >> 32;
    data[58] = bitlen >> 40;
    data[57] = bitlen >> 48;
    data[56] = bitlen >> 56;
    transform(data);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << std::setw(8) << std::setfill('0') << state[i];
    }
    return ss.str();
}

std::string sha256(const std::string& input) {
    SHA256 sha;
    sha.update(input);
    return sha.finalize();
}

// -------------------------------------------------------------
//  Helper utilities for path traversal
// -------------------------------------------------------------
static std::string getParent(const std::string& path) {
    if (path == "/") return "";
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos) return "";
    if (last_slash == 0) return "/";
    return path.substr(0, last_slash);
}

static std::vector<std::string> get_immediate_children(const std::unordered_map<std::string, std::string>& footprint, const std::string& parent) {
    std::vector<std::string> children;
    for (const auto& [k, v] : footprint) {
        if (k != parent && getParent(k) == parent) {
            children.push_back(k);
        }
    }
    return children;
}

// -------------------------------------------------------------
//  FootprintTracker implementation
// -------------------------------------------------------------
FootprintTracker::FootprintTracker() {}

void FootprintTracker::propagate_hash(std::unordered_map<std::string, std::string>& footprint, const std::string& path) {
    std::string curr = getParent(path);
    while (!curr.empty()) {
        std::vector<std::string> children = get_immediate_children(footprint, curr);
        std::sort(children.begin(), children.end());

        std::string concat_hashes = "";
        for (const auto& child : children) {
            concat_hashes += footprint[child];
        }
        footprint[curr] = sha256(concat_hashes);

        if (curr == "/") break;
        curr = getParent(curr);
    }
}

void FootprintTracker::snapshot() {
    sync_sandbox_to_virtual_files();
    active_footprint.clear();

    // 1. Discover all files and directories
    std::set<std::string> directories;
    directories.insert("/");

    for (const auto& [path, content] : virtual_files) {
        if (content == "<dir>") {
            directories.insert(path);
        } else {
            // It's a file. Set its active hash directly from content.
            active_footprint[path] = sha256(content);
            // Trace and insert parent directories
            std::string parent = getParent(path);
            while (!parent.empty()) {
                directories.insert(parent);
                if (parent == "/") break;
                parent = getParent(parent);
            }
        }
    }

    // Include directory entries in active footprint
    for (const auto& dir : directories) {
        if (active_footprint.count(dir) == 0) {
            active_footprint[dir] = ""; // temporary hash, will compute bottom-up
        }
    }

    // 2. Sort all paths by length in descending order for bottom-up computation
    std::vector<std::string> sorted_paths;
    for (const auto& [path, _] : active_footprint) {
        sorted_paths.push_back(path);
    }
    std::sort(sorted_paths.begin(), sorted_paths.end(), [](const std::string& a, const std::string& b) {
        return a.length() > b.length();
    });

    // 3. Pre-build children map for fast O(N) lookup
    std::unordered_map<std::string, std::vector<std::string>> parent_to_children;
    for (const auto& path : sorted_paths) {
        std::string parent = getParent(path);
        if (!parent.empty()) {
            parent_to_children[parent].push_back(path);
        }
    }

    // 4. Compute hashes bottom-up
    for (const auto& path : sorted_paths) {
        // If it's a directory (i.e. present in our directories set)
        if (directories.count(path)) {
            std::vector<std::string> children = parent_to_children[path];
            std::sort(children.begin(), children.end());
            
            std::string concat = "";
            for (const auto& child : children) {
                concat += active_footprint[child];
            }
            active_footprint[path] = sha256(concat);
        }
    }

    // 5. Store snapshot in memory
    stored_footprint = active_footprint;

    // 6. Save to virtual filesystem /var/footprint.db and host footprint.db
    save_to_disk("/var/footprint.db");
}

void FootprintTracker::rebuild_active_footprint() {
    active_footprint.clear();

    // 1. Discover all files and directories
    std::set<std::string> directories;
    directories.insert("/");

    for (const auto& [path, content] : virtual_files) {
        if (content == "<dir>") {
            directories.insert(path);
        } else {
            // It's a file. Set its active hash directly from content.
            active_footprint[path] = sha256(content);
            // Trace and insert parent directories
            std::string parent = getParent(path);
            while (!parent.empty()) {
                directories.insert(parent);
                if (parent == "/") break;
                parent = getParent(parent);
            }
        }
    }

    // Include directory entries in active footprint
    for (const auto& dir : directories) {
        if (active_footprint.count(dir) == 0) {
            active_footprint[dir] = ""; // temporary hash, will compute bottom-up
        }
    }

    // 2. Sort all paths by length in descending order for bottom-up computation
    std::vector<std::string> sorted_paths;
    for (const auto& [path, _] : active_footprint) {
        sorted_paths.push_back(path);
    }
    std::sort(sorted_paths.begin(), sorted_paths.end(), [](const std::string& a, const std::string& b) {
        return a.length() > b.length();
    });

    // 3. Pre-build children map for fast O(N) lookup
    std::unordered_map<std::string, std::vector<std::string>> parent_to_children;
    for (const auto& path : sorted_paths) {
        std::string parent = getParent(path);
        if (!parent.empty()) {
            parent_to_children[parent].push_back(path);
        }
    }

    // 4. Compute hashes bottom-up
    for (const auto& path : sorted_paths) {
        // If it's a directory (i.e. present in our directories set)
        if (directories.count(path)) {
            std::vector<std::string> children = parent_to_children[path];
            std::sort(children.begin(), children.end());
            
            std::string concat = "";
            for (const auto& child : children) {
                concat += active_footprint[child];
            }
            active_footprint[path] = sha256(concat);
        }
    }
}

void FootprintTracker::diff(std::vector<std::string>& added, 
                            std::vector<std::string>& removed, 
                            std::vector<std::string>& modified) {
    added.clear();
    removed.clear();
    modified.clear();

    // Loop through current active footprint to find added and modified files
    for (const auto& [path, active_hash] : active_footprint) {
        auto it = stored_footprint.find(path);
        if (it == stored_footprint.end()) {
            added.push_back(path);
        } else if (it->second != active_hash) {
            modified.push_back(path);
        }
    }

    // Loop through stored footprint to find removed files
    for (const auto& [path, stored_hash] : stored_footprint) {
        if (active_footprint.count(path) == 0) {
            removed.push_back(path);
        }
    }

    std::sort(added.begin(), added.end());
    std::sort(removed.begin(), removed.end());
    std::sort(modified.begin(), modified.end());

    footprint_added = added;
    footprint_removed = removed;
    footprint_modified = modified;
    footprint_has_diff = true;
}

void FootprintTracker::update(const std::string& path) {
    if (path.empty()) {
        // Sync whole stored_footprint to active_footprint
        stored_footprint = active_footprint;
        save_to_disk("/var/footprint.db");
        return;
    }

    // Check if the path exists in the active footprint
    auto it = active_footprint.find(path);
    if (it != active_footprint.end()) {
        stored_footprint[path] = it->second;
    } else {
        // Erase path and all descendants from stored footprint
        std::string prefix = path + "/";
        for (auto sit = stored_footprint.begin(); sit != stored_footprint.end(); ) {
            if (sit->first == path || sit->first.rfind(prefix, 0) == 0) {
                sit = stored_footprint.erase(sit);
            } else {
                ++sit;
            }
        }
    }

    // Propagate change up the directory tree in stored footprint
    propagate_hash(stored_footprint, path);
    save_to_disk("/var/footprint.db");
}

void FootprintTracker::notify_change(const std::string& path) {
    // Check if file still exists in virtual_files
    auto it = virtual_files.find(path);
    if (it != virtual_files.end()) {
        if (it->second == "<dir>") {
            // Keep empty hash temporarily, will be updated via children propagation if any exist.
            active_footprint[path] = sha256("");
        } else {
            active_footprint[path] = sha256(it->second);
        }
    } else {
        // Path was deleted. Erase it and all descendants from active footprint.
        std::string prefix = path + "/";
        for (auto ait = active_footprint.begin(); ait != active_footprint.end(); ) {
            if (ait->first == path || ait->first.rfind(prefix, 0) == 0) {
                ait = active_footprint.erase(ait);
            } else {
                ++ait;
            }
        }
    }

    // Propagate hash changes up to the root in active footprint
    propagate_hash(active_footprint, path);
}

std::string FootprintTracker::get_root_hash(bool active) const {
    const auto& m = active ? active_footprint : stored_footprint;
    auto it = m.find("/");
    if (it != m.end()) {
        return it->second;
    }
    return "";
}

// -------------------------------------------------------------
//  Serialization & Deserialization
// -------------------------------------------------------------
static std::string serialize_map_to_json(const std::unordered_map<std::string, std::string>& m) {
    std::string out = "{\n";
    size_t i = 0;
    for (const auto& [k, v] : m) {
        out += "  \"" + k + "\": \"" + v + "\"";
        if (++i < m.size()) out += ",";
        out += "\n";
    }
    out += "}";
    return out;
}

static std::unordered_map<std::string, std::string> deserialize_json_to_map(const std::string& json) {
    std::unordered_map<std::string, std::string> m;
    size_t pos = 0;
    while (true) {
        size_t key_start = json.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = json.find('"', key_start + 1);
        if (key_end == std::string::npos) break;
        std::string key = json.substr(key_start + 1, key_end - key_start - 1);
        
        size_t colon = json.find(':', key_end + 1);
        if (colon == std::string::npos) break;
        
        size_t val_start = json.find('"', colon + 1);
        if (val_start == std::string::npos) break;
        size_t val_end = json.find('"', val_start + 1);
        if (val_end == std::string::npos) break;
        std::string val = json.substr(val_start + 1, val_end - val_start - 1);
        
        m[key] = val;
        pos = val_end + 1;
    }
    return m;
}

bool FootprintTracker::save_to_disk(const std::string& virtual_path) {
    std::string json_str = serialize_map_to_json(stored_footprint);
    virtual_files[virtual_path] = json_str;

    // Translate virtual path to sandboxed physical path
    std::string real_path = translate_path(virtual_path);
    try {
        std::filesystem::create_directories(std::filesystem::path(real_path).parent_path());
    } catch (...) {}

    std::ofstream f1(real_path);
    if (f1.is_open()) {
        f1 << json_str;
        f1.close();
        return true;
    }
    return false;
}

bool FootprintTracker::load_from_disk(const std::string& virtual_path) {
    std::string json_str = "";

    // Translate virtual path to sandboxed physical path
    std::string real_path = translate_path(virtual_path);
    std::ifstream f1(real_path);
    if (f1.is_open()) {
        json_str.assign((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
        f1.close();
    }

    // If still empty, check if it exists in virtual_files
    if (json_str.empty()) {
        auto it = virtual_files.find(virtual_path);
        if (it != virtual_files.end()) {
            json_str = it->second;
        }
    }

    if (!json_str.empty()) {
        stored_footprint = deserialize_json_to_map(json_str);
        virtual_files[virtual_path] = json_str;
        
        // Build active footprint from current live virtual_files
        active_footprint.clear();
        std::set<std::string> directories;
        directories.insert("/");

        for (const auto& [path, content] : virtual_files) {
            if (content == "<dir>") {
                directories.insert(path);
            } else {
                active_footprint[path] = sha256(content);
                std::string parent = getParent(path);
                while (!parent.empty()) {
                    directories.insert(parent);
                    if (parent == "/") break;
                    parent = getParent(parent);
                }
            }
        }
        for (const auto& dir : directories) {
            if (active_footprint.count(dir) == 0) {
                active_footprint[dir] = "";
            }
        }

        std::vector<std::string> sorted_paths;
        for (const auto& [path, _] : active_footprint) {
            sorted_paths.push_back(path);
        }
        std::sort(sorted_paths.begin(), sorted_paths.end(), [](const std::string& a, const std::string& b) {
            return a.length() > b.length();
        });

        std::unordered_map<std::string, std::vector<std::string>> parent_to_children;
        for (const auto& path : sorted_paths) {
            std::string parent = getParent(path);
            if (!parent.empty()) {
                parent_to_children[parent].push_back(path);
            }
        }

        for (const auto& path : sorted_paths) {
            if (directories.count(path)) {
                std::vector<std::string> children = parent_to_children[path];
                std::sort(children.begin(), children.end());
                std::string concat = "";
                for (const auto& child : children) {
                    concat += active_footprint[child];
                }
                active_footprint[path] = sha256(concat);
            }
        }
        return true;
    }
    return false;
}

bool FootprintTracker::is_stored_dir(const std::string& path) const {
    if (path == "/") return true;
    std::string prefix = path + "/";
    for (const auto& [k, v] : stored_footprint) {
        if (k.rfind(prefix, 0) == 0) return true;
    }
    auto it = virtual_files.find(path);
    if (it != virtual_files.end() && it->second == "<dir>") return true;
    return false;
}

bool FootprintTracker::is_active_dir(const std::string& path) const {
    if (path == "/") return true;
    auto it = virtual_files.find(path);
    if (it != virtual_files.end() && it->second == "<dir>") return true;
    std::string prefix = path + "/";
    for (const auto& [k, v] : active_footprint) {
        if (k.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool FootprintTracker::isPathChanged(const std::string& path) const {
    if (!footprint_has_diff) return false;
    for (const auto& p : footprint_added) {
        if (p == path) return true;
    }
    for (const auto& p : footprint_modified) {
        if (p == path) return true;
    }
    for (const auto& p : footprint_removed) {
        if (p == path) return true;
    }
    return false;
}

