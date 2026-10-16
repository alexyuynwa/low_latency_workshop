
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <cstddef>
#include <string_view>


#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

namespace v6 {

struct Slot {
    const std::string* value;
};

class RealLikeSwissTable {

private:
    static constexpr size_t kGroupWidth = 16;

    static_assert(kGroupWidth > 0 && ((kGroupWidth & (kGroupWidth - 1)) == 0), 
                  "compile time table requires kGroupWidth to be a power of two.");


    // SwissTable control-byte semantics:
    // - 0x80: empty
    // - 0xFE: deleted
    // - 0x00..0x7F: occupied, stores H2

    size_t capacity_;       // Number of table slots, usually a power of two
    size_t group_count_;    // Number of groups = capacity / 16

    std::vector<uint8_t> ctrl_;
    std::vector<Slot> slots_;

    static constexpr size_t next_power_of_two(size_t n) {
        size_t p = kGroupWidth;
        while (p < n) {
            if (p > std::numeric_limits<size_t>::max() / 2) {
                throw std::length_error("requested table capacity is too large");
            }
            p <<= 1;
        }
        return p;
    }

    static constexpr size_t capacity_for(size_t element_count) {
        // Keep the load factor at or below 7/8 so every lookup can stop at an empty slot.
        const size_t extra = element_count / 7 + (element_count % 7 != 0);
        if (element_count > std::numeric_limits<size_t>::max() - extra) {
            throw std::length_error("requested table capacity is too large");
        }
        return next_power_of_two(element_count + extra);
    }

    // h1: the hash used to calculate the bucket index
    static uint64_t h1(uint64_t hash) noexcept {
        return hash >> 7;  // Upper 57 bits
    }

    // h2: the actual hash value of the key
    static uint8_t h2(uint64_t hash) noexcept {
        return static_cast<uint8_t>(hash & 0x7F); // Lower 7 bits
    }

    // Calculate the bucket index.
   [[nodiscard]] size_t bucket_index(uint64_t hash) const noexcept {
        // Use H1 to select the starting bucket.
        // Since capacity is a power of two, masking is equivalent to modulo.
        return static_cast<size_t>(h1(hash) & (capacity_ - 1));
    }

    // Calculate the group index.
    [[nodiscard]] size_t group_index(uint64_t hash) const noexcept {        
        return bucket_index(hash) / kGroupWidth;
    }

public:
    explicit RealLikeSwissTable(size_t initial_capacity = 16)
        : capacity_(capacity_for(initial_capacity)),
        group_count_(capacity_ / kGroupWidth),
        ctrl_(capacity_, 0x80),
        slots_(capacity_) {
    }

    template<typename Collection>
    explicit RealLikeSwissTable(const Collection& words)
        : capacity_(capacity_for(words.size())),
        group_count_(capacity_ / kGroupWidth),
        ctrl_(capacity_, 0x80),
        slots_(capacity_) {
            
        for(const std::string& s : words)
		{
			insert(s);
		}
    }

    void insert(const std::string& value) noexcept {
        uint64_t hash = std::hash<std::string>{}(value);
        uint8_t hp = h2(hash);

        size_t start = bucket_index(hash);
        size_t g = start / kGroupWidth;
        __m128i target = _mm_set1_epi8(static_cast<char>(hp));

        // group_count_ is a power of two, so a mask replaces modulo here.
        for (size_t probe = 0; probe < group_count_; ++probe, g = (g + 1) & (group_count_ - 1)) {
            size_t base = g * kGroupWidth;

            // 1) Check the control bytes in this group.
            /*
                Load 16 bytes (128 bits) at once from &ctrl_[base] into the SIMD
                register ctrl_vec. These bytes contain the metadata for all 16
                slots in this group.
            */
            __m128i ctrl_vec = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(&ctrl_[base]));

            __m128i eq = _mm_cmpeq_epi8(ctrl_vec, target);

            // 2) Use SIMD to produce a bitmask.
            uint32_t mask = _mm_movemask_epi8(eq);

            while (mask != 0) {
                int idx = __builtin_ctz(mask);
                size_t slot_idx = base + idx;

                if (*slots_[slot_idx].value == value) {
                    return;
                }

                mask &= mask - 1; // Clear the bit that was just checked.
            }

            // 3) Find an empty slot and insert the value.
            for (size_t i = 0; i < kGroupWidth; ++i) {
                size_t slot_idx = base + i;
                if (ctrl_[slot_idx] == 0x80) { // Empty
                    ctrl_[slot_idx] = hp;
                    slots_[slot_idx].value = &value;
                    return;
                }
            }
        }
    }

    const std::string* find(std::string_view value) const noexcept {
        uint64_t hash = std::hash<std::string_view>{}(value);
        uint8_t hp = h2(hash);

        size_t start = bucket_index(hash);
        size_t g = start / kGroupWidth;

        // Broadcast the target fingerprint: _mm_set1_epi8 copies the 7-bit
        // fingerprint hp 16 times to fill the 128-bit target register
        // (for example, [hp, hp, ..., hp]).
        __m128i target = _mm_set1_epi8(static_cast<char>(hp));

        // Keep the probe order in sync with insert; use a mask instead of modulo.
        for (size_t probe = 0; probe < group_count_; ++probe, g = (g + 1) & (group_count_ - 1)) {
            size_t base = g * kGroupWidth;
            
            // Vectorized load: _mm_loadu_si128 (VMOVDQU in assembly) loads
            // 16 bytes (128 bits) at once from &ctrl_[base] into the SIMD
            // register ctrl_vec. These bytes contain the metadata for all 16
            // slots in this group.
            __m128i ctrl_vec = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(&ctrl_[base]));

            // Batch comparison: _mm_cmpeq_epi8 (VPCMPEQB in assembly) compares
            // the 16 corresponding bytes in ctrl_vec and target in parallel.
            // Equal bytes produce 0xFF (all bits set); unequal bytes produce
            // 0x00 (all bits clear).
            __m128i eq = _mm_cmpeq_epi8(ctrl_vec, target);

            // Generate a bitmask: _mm_movemask_epi8 (VPMOVMSKB in assembly)
            // extracts the sign bit of each of the 16 bytes in eq and combines
            // them into a 16-bit integer (the lower 16 bits of match_mask).
            uint32_t match_mask = static_cast<uint32_t>(_mm_movemask_epi8(eq));

            // Process every matching bit.
            /*
                -   A nonzero match_mask indicates that this group still has
                    slots that may match.

                -   __builtin_ctz(match_mask) counts trailing zero bits (using
                    an instruction such as TZCNT or BSF), identifying the
                    index of the first set bit, idx (in the range 0-15).

                -   Calculate the actual data slot.

                    slot_idx = base + idx gives the element's absolute index
                    in the slots_ array.

                -   Access the std::string pointed to by
                    slots_[slot_idx].value. If it exactly matches value, the
                    lookup succeeds and returns a pointer to that string.

                -   Clear the current matching bit:

                    match_mask &= match_mask - 1 is a classic bit-manipulation
                    technique that clears the lowest set bit, allowing the
                    next loop iteration to process the next match with
                    __builtin_ctz.
            */
            while (match_mask != 0) {
                int idx = __builtin_ctz(match_mask);
                size_t slot_idx = base + idx;

                if (*slots_[slot_idx].value == value) {
                    return slots_[slot_idx].value;
                }

                match_mask &= match_mask - 1;
            }

            // Early-miss optimization (early termination).
            /*
                After checking every slot in the current group whose H2 tag
                matches, how can we tell whether to search the next group or
                report that the key was not found?

                -   Extract the empty-slot mask.

                    -   Use _mm_movemask_epi8(ctrl_vec) again.
                    -   The key detail is that SwissTable marks an empty slot
                        with 0x80 (binary 10000000), whose highest bit is 1.
                        Occupied slots have H2 values in the range 0x00-0x7F,
                        whose highest bit is 0.
                    -   Therefore, applying _mm_movemask_epi8 directly to the
                        original ctrl_vec produces an empty_mask where each
                        set bit indicates an empty slot.
            */

            // A never-used slot ends this key's probe sequence, so misses stop here.
            // Occupied tags have a clear high bit; 0x80 empty slots set it.
            const uint32_t empty_mask = static_cast<uint32_t>(_mm_movemask_epi8(ctrl_vec));
            if (empty_mask != 0) {
                return nullptr;
            }
        }

        return nullptr;
    }
};

class Dictionary
{
public:
	template<typename Collection>
	explicit Dictionary(const Collection& words) :
		table_(words)
	{}

	bool in_dictionary(std::string_view word) const
	{
        return table_.find(word) != nullptr;
    }

private:
    RealLikeSwissTable table_;
};

}