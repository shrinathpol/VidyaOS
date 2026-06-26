#ifndef FOOTPRINT_H
#define FOOTPRINT_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// Lightweight, self-contained SHA-256 implementation
class SHA256 {
private:
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    uint32_t datalen;

    void transform(const uint8_t *data);
    static inline uint32_t CH(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static inline uint32_t MAJ(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint32_t EP0(uint32_t x) { return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22); }
    static inline uint32_t EP1(uint32_t x) { return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25); }
    static inline uint32_t SIG0(uint32_t x) { return ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3); }
    static inline uint32_t SIG1(uint32_t x) { return ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10); }
    static inline uint32_t ROTR(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

    static const uint32_t k[64];

public:
    SHA256();
    void update(const uint8_t *data, size_t len);
    void update(const std::string& str);
    std::string finalize();
};

// Utility function to compute SHA-256 of a string
std::string sha256(const std::string& input);

class FootprintTracker {
private:
    std::unordered_map<std::string, std::string> stored_footprint;
    std::unordered_map<std::string, std::string> active_footprint;

    // Helper to propagate hash modifications up to the root
    void propagate_hash(std::unordered_map<std::string, std::string>& footprint, const std::string& path);

public:
    FootprintTracker();

    // Rebuild active footprint entirely from virtual_files, and sync to stored_footprint
    void snapshot();

    // Rebuild active footprint on-demand from current virtual_files
    void rebuild_active_footprint();

    // Diff stored_footprint and active_footprint
    void diff(std::vector<std::string>& added, 
              std::vector<std::string>& removed, 
              std::vector<std::string>& modified);

    // Updates a path in stored_footprint from the active footprint, propagating changes up
    void update(const std::string& path);

    // Triggered upon file system changes. Recalculates active footprint incrementally.
    void notify_change(const std::string& path);

    // Returns the root hash of the directory "/" (active or stored)
    std::string get_root_hash(bool active = true) const;

    // Checks if a path is a directory in active/stored footprint
    bool is_stored_dir(const std::string& path) const;
    bool is_active_dir(const std::string& path) const;

    // Save and load stored footprint from virtual file and host disk backing store
    bool save_to_disk(const std::string& virtual_path);
    bool load_from_disk(const std::string& virtual_path);

    bool isPathChanged(const std::string& path) const;
};

// Global instance of the tracker
extern FootprintTracker mft;

#endif // FOOTPRINT_H
