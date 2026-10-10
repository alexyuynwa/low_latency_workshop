#pragma once

#include <charconv>
#include <cstddef>
#include <string_view>

namespace fix {

enum class ParseStatus {
    ok,
    incomplete,
    invalid,
};

struct MessageView {
    std::string_view raw;
    std::string_view msg_type;
    std::string_view sender;
    std::string_view target;
    std::string_view sequence;
    std::string_view client_order_id;
};

inline bool parse_unsigned(std::string_view text, std::size_t& value) {
    if (text.empty()) {
        return false;
    }
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

inline ParseStatus parse_message(const char* data, std::size_t length,
                                std::size_t& consumed, MessageView& message) {
    constexpr char soh = '\x01';
    consumed = 0;
    if (data == nullptr) {
        return ParseStatus::invalid;
    }

    const std::string_view input(data, length);
    const auto next_field = [&input](std::size_t& position,
                                     std::string_view& field) {
        if (position >= input.size()) {
            return ParseStatus::incomplete;
        }
        const std::size_t end = input.find('\x01', position);
        if (end == std::string_view::npos) {
            return ParseStatus::incomplete;
        }
        field = input.substr(position, end - position);
        position = end + 1;
        return ParseStatus::ok;
    };

    std::size_t position = 0;
    std::string_view field;
    if (next_field(position, field) != ParseStatus::ok) {
        return ParseStatus::incomplete;
    }
    if (field.size() <= 2 || field.substr(0, 2) != "8=") {
        return ParseStatus::invalid;
    }

    if (next_field(position, field) != ParseStatus::ok) {
        return ParseStatus::incomplete;
    }
    if (field.size() < 2 || field.substr(0, 2) != "9=") {
        return ParseStatus::invalid;
    }

    std::size_t body_length = 0;
    if (!parse_unsigned(field.substr(2), body_length)) {
        return ParseStatus::invalid;
    }
    const std::size_t body_start = position;
    if (body_length > input.size() - body_start) {
        return ParseStatus::incomplete;
    }
    const std::size_t checksum_start = body_start + body_length;
    if (checksum_start == body_start || input[checksum_start - 1] != soh) {
        return ParseStatus::invalid;
    }

    if (input.size() - checksum_start < 7) {
        return ParseStatus::incomplete;
    }
    if (input.substr(checksum_start, 3) != "10=" ||
        input[checksum_start + 6] != soh) {
        return ParseStatus::invalid;
    }

    const std::string_view checksum_text = input.substr(checksum_start + 3, 3);
    std::size_t expected_checksum = 0;
    if (!parse_unsigned(checksum_text, expected_checksum) ||
        expected_checksum > 255) {
        return ParseStatus::invalid;
    }
    unsigned int checksum = 0;
    for (std::size_t index = 0; index < checksum_start; ++index) {
        checksum = (checksum + static_cast<unsigned char>(input[index])) & 0xffU;
    }
    if (checksum != expected_checksum) {
        return ParseStatus::invalid;
    }

    MessageView parsed{
        input.substr(0, checksum_start + 7), {}, {}, {}, {}, {}};
    position = body_start;
    while (position < checksum_start) {
        const std::size_t end = input.find(soh, position);
        if (end == std::string_view::npos || end >= checksum_start) {
            return ParseStatus::invalid;
        }
        const std::string_view body_field =
            input.substr(position, end - position);
        const std::size_t equals = body_field.find('=');
        if (equals == std::string_view::npos || equals == 0) {
            return ParseStatus::invalid;
        }
        const std::string_view tag = body_field.substr(0, equals);
        const std::string_view value = body_field.substr(equals + 1);
        if (tag == "35") {
            parsed.msg_type = value;
        } else if (tag == "49") {
            parsed.sender = value;
        } else if (tag == "56") {
            parsed.target = value;
        } else if (tag == "34") {
            parsed.sequence = value;
        } else if (tag == "11") {
            parsed.client_order_id = value;
        }
        position = end + 1;
    }
    if (position != checksum_start || parsed.msg_type.empty()) {
        return ParseStatus::invalid;
    }

    consumed = parsed.raw.size();
    message = parsed;
    return ParseStatus::ok;
}

}  // namespace fix
