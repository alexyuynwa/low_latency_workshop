#include "fix_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string make_message(const std::string& body) {
    const std::string prefix = "8=FIX.4.4\x01" "9=" +
                               std::to_string(body.size()) + "\x01" + body;
    unsigned int checksum = 0;
    for (const unsigned char character : prefix) {
        checksum = (checksum + character) & 0xffU;
    }
    char checksum_field[8];
    std::snprintf(checksum_field, sizeof(checksum_field), "10=%03u\x01",
                  checksum);
    return prefix + checksum_field;
}

}  // namespace

int main() {
    const std::string message =
        make_message("35=D\x01" "49=CLIENT\x01" "56=EXCHANGE\x01"
                     "34=7\x01" "11=order-42\x01");
    fix::MessageView parsed;
    std::size_t consumed = 0;
    require(fix::parse_message(message.data(), message.size(), consumed,
                               parsed) == fix::ParseStatus::ok,
            "valid message parses");
    require(consumed == message.size(), "consumed length matches message");
    require(parsed.msg_type == "D", "MsgType is extracted");
    require(parsed.sender == "CLIENT", "SenderCompID is extracted");
    require(parsed.target == "EXCHANGE", "TargetCompID is extracted");
    require(parsed.sequence == "7", "MsgSeqNum is extracted");
    require(parsed.client_order_id == "order-42", "ClOrdID is extracted");

    require(fix::parse_message(message.data(), message.size() - 2, consumed,
                               parsed) == fix::ParseStatus::incomplete,
            "truncated message is incomplete");

    std::string bad_checksum = message;
    bad_checksum[bad_checksum.size() - 3] =
        bad_checksum[bad_checksum.size() - 3] == '0' ? '1' : '0';
    require(fix::parse_message(bad_checksum.data(), bad_checksum.size(),
                               consumed, parsed) == fix::ParseStatus::invalid,
            "bad checksum is rejected");

    std::string bad_body_length = message;
    const std::size_t length_start = bad_body_length.find("9=") + 2;
    bad_body_length[length_start] = '0';
    require(fix::parse_message(bad_body_length.data(), bad_body_length.size(),
                               consumed, parsed) == fix::ParseStatus::invalid,
            "incorrect BodyLength is rejected");

    std::cout << "All FIX parser tests passed\n";
}
