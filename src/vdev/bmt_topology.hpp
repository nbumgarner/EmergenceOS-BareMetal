#ifndef EMERGENCE_TOPOLOGY_HPP
#define EMERGENCE_TOPOLOGY_HPP

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>

namespace Emergence {

// ============================================================
// Constants
// ============================================================

constexpr int      LENS_COUNT        = 1024;
constexpr uint16_t LENS_MASK         = 0x3FF;
constexpr uint16_t HALT_ROUTE        = 0x3FF;
constexpr uint16_t ERROR_ROUTE       = 0x3FE;
constexpr uint16_t FREE_ROUTE        = 0x3FD;
constexpr uint64_t PHI_HI            = 0x9E3779B97F4A7C15ULL;
constexpr uint64_t PHI_LO            = 0x517CC1B727220A95ULL;
constexpr size_t   ENTRIES_PER_BLOCK = 1024;
constexpr size_t   PAYLOAD_BYTES_PER_HOP = 13;
constexpr size_t   BLOCK_SIZE_BYTES  = ENTRIES_PER_BLOCK * 16;
constexpr size_t   BYTES_PER_BLOCK   = ENTRIES_PER_BLOCK * PAYLOAD_BYTES_PER_HOP;

constexpr uint32_t KDF_ROUNDS        = 250000;
constexpr size_t   KDF_MEM_BLOCKS    = 1024;
constexpr uint32_t KDF_MEM_PASSES    = 4;

// ============================================================
// 128-bit value
// ============================================================

struct Value128 {
    uint64_t hi;
    uint64_t lo;

    Value128() : hi(0), lo(0) {}
    Value128(uint64_t h, uint64_t l) : hi(h), lo(l) {}

    uint16_t route() const {
        return (uint16_t)(hi >> 54) & LENS_MASK;
    }

    uint8_t crc() const {
        return (uint8_t)(hi >> 52) & 0x03;
    }

    uint8_t compute_crc() const {
        uint64_t payload_hi = hi & 0x000FFFFFFFFFFFFFULL;
        uint64_t fold = payload_hi ^ lo;
        fold ^= (fold >> 32); fold ^= (fold >> 16);
        fold ^= (fold >> 8);  fold ^= (fold >> 4);
        fold ^= (fold >> 2);
        return (uint8_t)(fold & 0x03);
    }

    bool crc_valid() const { return crc() == compute_crc(); }

    void pack_route(uint16_t r) {
        hi &= 0x000FFFFFFFFFFFFFULL;
        hi |= ((uint64_t)(r & LENS_MASK) << 54);
        uint8_t c = compute_crc();
        hi |= ((uint64_t)(c & 0x03) << 52);
    }

    size_t get_payload(uint8_t* out, size_t max_bytes) const {
        size_t copied = 0;
        for (size_t i = 0; i < 8 && copied < max_bytes; i++, copied++)
            out[copied] = (uint8_t)(lo >> (i * 8));
        for (size_t i = 0; i < 5 && copied < max_bytes; i++, copied++)
            out[copied] = (uint8_t)(hi >> (i * 8));
        return copied;
    }

    void set_payload(const uint8_t* data, size_t len) {
        lo = 0;
        uint64_t top_bits = hi & 0xFFF0000000000000ULL;
        hi = 0;
        size_t pos = 0;
        for (size_t i = 0; i < 8 && pos < len; i++, pos++)
            lo |= ((uint64_t)data[pos] << (i * 8));
        for (size_t i = 0; i < 5 && pos < len; i++, pos++)
            hi |= ((uint64_t)data[pos] << (i * 8));
        hi |= top_bits;
    }

    void write_to(uint8_t* buf) const {
        memcpy(buf, &lo, 8);
        memcpy(buf + 8, &hi, 8);
    }

    void read_from(const uint8_t* buf) {
        memcpy(&lo, buf, 8);
        memcpy(&hi, buf + 8, 8);
    }
};

// ============================================================
// Seed
// ============================================================

struct Seed {
    uint64_t hi;
    uint64_t lo;
};

// ============================================================
// Memory-hard Key Derivation
// ============================================================

struct KeyDerivation {
    static Seed derive(const char* password, const char* hwkey) {
        size_t pw_len = strlen(password);
        size_t hk_len = strlen(hwkey);

        uint64_t hi = 0xCBF29CE484222325ULL;
        uint64_t lo = 0x00000100000001B3ULL;

        for (size_t i = 0; i < pw_len; i++) {
            hi ^= (uint8_t)password[i]; hi *= 0x00000100000001B3ULL;
            lo ^= (uint8_t)password[i]; lo *= 0x01000193ULL;
        }
        for (size_t i = 0; i < hk_len; i++) {
            hi ^= (uint8_t)hwkey[i]; hi *= 0x00000100000001B3ULL;
            lo ^= (uint8_t)hwkey[i]; lo *= 0x01000193ULL;
        }

        // Memory-hard: fill buffer with dependent chain
        struct Cell { uint64_t a, b; };
        std::vector<Cell> mem(KDF_MEM_BLOCKS);
        mem[0] = {hi, lo};
        for (size_t i = 1; i < KDF_MEM_BLOCKS; i++) {
            mem[i].a = ((mem[i-1].a << 13) | (mem[i-1].a >> 51)) ^ (mem[i-1].b * PHI_HI);
            mem[i].b = ((mem[i-1].b << 17) | (mem[i-1].b >> 47)) ^ (mem[i-1].a * PHI_LO);
        }

        // Random-walk passes
        uint64_t s_hi = hi, s_lo = lo;
        for (uint32_t pass = 0; pass < KDF_MEM_PASSES; pass++) {
            for (size_t i = 0; i < KDF_MEM_BLOCKS; i++) {
                size_t idx = (size_t)(s_hi % KDF_MEM_BLOCKS);
                s_hi ^= mem[idx].a; s_lo ^= mem[idx].b;
                s_hi = ((s_hi << 31) | (s_hi >> 33)) ^ (s_lo * PHI_HI);
                s_lo = ((s_lo << 29) | (s_lo >> 35)) ^ (s_hi * PHI_LO);
                mem[idx].a ^= s_hi; mem[idx].b ^= s_lo;
            }
        }

        // Final compression
        for (uint32_t r = 0; r < KDF_ROUNDS; r++) {
            s_hi = ((s_hi << 31) | (s_hi >> 33)) ^ (s_lo * PHI_HI);
            s_lo = ((s_lo << 29) | (s_lo >> 35)) ^ (s_hi * PHI_LO);
        }

        return {s_hi, s_lo};
    }
};

// ============================================================
// Lens
// ============================================================

struct LensKey { uint64_t hi, lo; };

struct LensGenerator {
    static void generate(Seed master, LensKey keys[LENS_COUNT]) {
        for (int i = 0; i < LENS_COUNT; i++) {
            uint64_t mix_hi = master.hi ^ (uint64_t)i;
            uint64_t mix_lo = master.lo ^ ((uint64_t)i << 32);
            for (int r = 0; r < 64; r++) {
                mix_hi = ((mix_hi << 7) | (mix_hi >> 57)) ^ (mix_lo * PHI_HI);
                mix_lo = ((mix_lo << 11) | (mix_lo >> 53)) ^ (mix_hi * PHI_LO);
                mix_hi += (uint64_t)i;
            }
            keys[i] = {mix_hi, mix_lo};
        }
    }
};

// ============================================================
// Block
// ============================================================

struct Block {
    Value128 entries[ENTRIES_PER_BLOCK];

    Block() {
        for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++) {
            entries[i] = Value128();
            entries[i].pack_route(FREE_ROUTE);
        }
    }

    void serialize(uint8_t* buf) const {
        for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++)
            entries[i].write_to(buf + i * 16);
    }

    void deserialize(const uint8_t* buf) {
        for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++)
            entries[i].read_from(buf + i * 16);
    }
};

// ============================================================
// Topology
// ============================================================

class Topology {
private:
    Seed seed_;
    LensKey lens_keys_[LENS_COUNT];
    Block root_;
    std::vector<std::unique_ptr<Block>> level1_;
    bool dirty_;

public:
    Topology() : dirty_(false) { level1_.resize(LENS_COUNT); }

    void initialize(Seed s) {
        seed_ = s;
        LensGenerator::generate(seed_, lens_keys_);
        root_ = Block();
        for (int i = 0; i < LENS_COUNT; i++) {
            level1_[i] = std::make_unique<Block>();
            expand_block(i);
        }
        dirty_ = true;
    }

    void expand_block(int lens_index) {
        LensKey& lk = lens_keys_[lens_index];
        uint64_t sh = lk.hi, sl = lk.lo;
        for (size_t j = 0; j < ENTRIES_PER_BLOCK; j++) {
            sh = ((sh << 13) | (sh >> 51)) ^ (sl * PHI_HI);
            sl = ((sl << 17) | (sl >> 47)) ^ (sh * PHI_LO);
            Value128 v; v.hi = sh; v.lo = sl;
            v.pack_route(FREE_ROUTE);
            level1_[lens_index]->entries[j] = v;
        }
    }

    // Write data into a single block, return bytes written
    size_t write_block_data(uint16_t idx, const uint8_t* data, size_t len) {
        if (idx >= LENS_COUNT || !level1_[idx]) return 0;
        Block* block = level1_[idx].get();
        size_t written = 0, entry = 0;

        while (written < len && entry < ENTRIES_PER_BLOCK) {
            size_t chunk = std::min(PAYLOAD_BYTES_PER_HOP, len - written);
            Value128& v = block->entries[entry];
            v.set_payload(data + written, chunk);
            if (written + chunk < len && entry + 1 < ENTRIES_PER_BLOCK)
                v.pack_route((uint16_t)(entry + 1));
            else
                v.pack_route(HALT_ROUTE);
            written += chunk;
            entry++;
        }
        while (entry < ENTRIES_PER_BLOCK) {
            block->entries[entry].pack_route(FREE_ROUTE);
            entry++;
        }
        dirty_ = true;
        return written;
    }

    // Read data from a single block
    size_t read_block_data(uint16_t idx, uint8_t* out, size_t max_len) const {
        if (idx >= LENS_COUNT || !level1_[idx]) return 0;
        const Block* block = level1_[idx].get();
        size_t total = 0, entry = 0;

        while (total < max_len && entry < ENTRIES_PER_BLOCK) {
            const Value128& v = block->entries[entry];
            uint16_t route = v.route();
            uint8_t payload[PAYLOAD_BYTES_PER_HOP];
            size_t got = v.get_payload(payload, PAYLOAD_BYTES_PER_HOP);
            size_t cp = std::min(got, max_len - total);
            memcpy(out + total, payload, cp);
            total += cp;
            if (route == HALT_ROUTE || route == FREE_ROUTE) break;
            if (route < ENTRIES_PER_BLOCK) entry = route; else break;
        }
        return total;
    }

    // Multi-block write
    size_t write_chain(const std::vector<uint16_t>& blocks,
                       const uint8_t* data, size_t len) {
        size_t written = 0;
        for (size_t b = 0; b < blocks.size() && written < len; b++) {
            size_t to_write = std::min(BYTES_PER_BLOCK, len - written);
            written += write_block_data(blocks[b], data + written, to_write);
        }
        return written;
    }

    // Multi-block read
    size_t read_chain(const std::vector<uint16_t>& blocks,
                      uint8_t* out, size_t max_len) const {
        size_t total = 0;
        for (size_t b = 0; b < blocks.size() && total < max_len; b++) {
            size_t r = read_block_data(blocks[b], out + total, max_len - total);
            total += r;
            if (r < BYTES_PER_BLOCK) break;
        }
        return total;
    }

    void clear_blocks(const std::vector<uint16_t>& blocks) {
        for (uint16_t idx : blocks) {
            if (idx < LENS_COUNT && level1_[idx]) {
                for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++)
                    level1_[idx]->entries[i].pack_route(FREE_ROUTE);
            }
        }
        dirty_ = true;
    }

    // Single-block convenience wrappers
    size_t write_chain(uint16_t idx, const uint8_t* data, size_t len) {
        std::vector<uint16_t> v = {idx};
        return write_chain(v, data, len);
    }
    size_t read_chain(uint16_t idx, uint8_t* out, size_t max_len) const {
        std::vector<uint16_t> v = {idx};
        return read_chain(v, out, max_len);
    }

    // Serialization
    bool save_image(const char* path) const {
        FILE* fp = fopen(path, "wb");
        if (!fp) return false;
        uint64_t tag = seed_.hi ^ seed_.lo ^ 0xA5A5A5A5A5A5A5A5ULL;
        uint64_t ver = 4;
        fwrite(&tag, 8, 1, fp); fwrite(&ver, 8, 1, fp);

        uint8_t buf[BLOCK_SIZE_BYTES];
        root_.serialize(buf);
        obfuscate(buf, BLOCK_SIZE_BYTES, seed_, 0);
        fwrite(buf, BLOCK_SIZE_BYTES, 1, fp);

        for (int i = 0; i < LENS_COUNT; i++) {
            if (level1_[i]) level1_[i]->serialize(buf);
            else memset(buf, 0, BLOCK_SIZE_BYTES);
            obfuscate(buf, BLOCK_SIZE_BYTES, seed_, i + 1);
            fwrite(buf, BLOCK_SIZE_BYTES, 1, fp);
        }
        fclose(fp);
        return true;
    }

    bool load_image(const char* path, Seed s) {
        FILE* fp = fopen(path, "rb");
        if (!fp) return false;
        seed_ = s;
        LensGenerator::generate(seed_, lens_keys_);

        uint64_t tag, ver;
        if (fread(&tag, 8, 1, fp) != 1 || fread(&ver, 8, 1, fp) != 1) {
            fclose(fp); return false;
        }
        if (tag != (seed_.hi ^ seed_.lo ^ 0xA5A5A5A5A5A5A5A5ULL)) {
            fclose(fp); return false;
        }

        uint8_t buf[BLOCK_SIZE_BYTES];
        if (fread(buf, BLOCK_SIZE_BYTES, 1, fp) != 1) { fclose(fp); return false; }
        obfuscate(buf, BLOCK_SIZE_BYTES, seed_, 0);
        root_.deserialize(buf);

        level1_.resize(LENS_COUNT);
        for (int i = 0; i < LENS_COUNT; i++) {
            if (fread(buf, BLOCK_SIZE_BYTES, 1, fp) != 1) { fclose(fp); return false; }
            obfuscate(buf, BLOCK_SIZE_BYTES, seed_, i + 1);
            level1_[i] = std::make_unique<Block>();
            level1_[i]->deserialize(buf);
        }
        fclose(fp);
        dirty_ = false;
        return true;
    }

    bool is_dirty() const { return dirty_; }
    const Seed& seed() const { return seed_; }

private:
    static void obfuscate(uint8_t* data, size_t len, Seed s, uint64_t bi) {
        uint64_t sh = s.hi ^ (bi * PHI_HI), sl = s.lo ^ (bi * PHI_LO);
        for (size_t i = 0; i < len; i += 8) {
            sh = ((sh << 13) | (sh >> 51)) ^ (sl * PHI_HI);
            sl = ((sl << 17) | (sl >> 47)) ^ (sh * PHI_LO);
            size_t rem = std::min((size_t)8, len - i);
            for (size_t j = 0; j < rem; j++)
                data[i + j] ^= (uint8_t)(sl >> (j * 8));
        }
    }
};

} // namespace Emergence
#endif
