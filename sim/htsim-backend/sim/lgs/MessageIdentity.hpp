#ifndef ATLAHS_MESSAGE_IDENTITY_HPP
#define ATLAHS_MESSAGE_IDENTITY_HPP

#include <compare>
#include <cstdint>

// Runtime identity is compared component by component, never packed or hashed
// into a bounded tag. Legacy records retain their original tag-based matching.
// GPU endpoints are separate routing metadata. The queue matcher checks them
// as well as this identity for explicit-field messages.
struct MessageIdentity {
    bool explicit_fields = false;
    std::uint64_t communicator = 0;
    std::uint64_t channel = 0;
    std::uint64_t sequence = 0;
    std::uint64_t kind = 0;
    std::uint64_t chunk = 0;
    auto operator<=>(const MessageIdentity&) const = default;
};

inline bool same_message(const MessageIdentity& left, std::uint32_t left_tag,
                         const MessageIdentity& right, std::uint32_t right_tag) {
    if (left.explicit_fields != right.explicit_fields) return false;
    if (left.explicit_fields) return left == right;
    constexpr auto any_tag = UINT32_MAX;
    return left_tag == any_tag || right_tag == any_tag || left_tag == right_tag;
}

#endif
