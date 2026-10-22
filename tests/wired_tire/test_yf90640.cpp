#include "test_support.h"
#include "WiredTireCore.h"
#include <fstream>
#include <iterator>
using namespace wired_tire;
static void rejected(const std::vector<uint8_t>& bytes, ParseResult expected, uint8_t address = 1) {
  ThermalFrame frame;
  frame.temp10.fill(777);
  frame.valid.fill(0xA5);
  CHECK(decodeYf65(bytes.data(), bytes.size(), address, frame) == expected);
  for (auto value : frame.temp10) CHECK(value == 777);
  for (auto value : frame.valid) CHECK(value == 0xA5);
}
int main(int argc, char** argv) {
  runTest("exception_framing_does_not_read_two_byte_count", [] {
    const uint8_t exception[]{1, 0xE5, 2, 0xAA};
    CHECK(expectedYfResponseBytes(exception, 1) == 0);
    CHECK(expectedYfResponseBytes(exception, 2) == 5);
    CHECK(expectedYfResponseBytes(exception, 4) == 5);
    const uint8_t normal[]{1, 0x65, 6, 0x22};
    CHECK(expectedYfResponseBytes(normal, 3) == 0);
    CHECK(expectedYfResponseBytes(normal, 4) == 1576);
    const uint8_t oversized[]{1, 0x65, 0xFF, 0xFF};
    CHECK(expectedYfResponseBytes(oversized, 4) > 2048);
  });
  runTest("modbus_crc_known_vector", [] {
    CHECK(modbusCrc(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0x4B37);
  });
  runTest("full_matrix_signed_zero_and_last_pixel", [] {
    const auto bytes = yfFixture(); ThermalFrame frame;
    CHECK(decodeYf65(bytes.data(), bytes.size(), 1, frame) == ParseResult::Ok);
    CHECK(frame.temp10[0] == -100 && frame.temp10[1] == 0 && frame.temp10[767] == 2000);
    for (size_t i = 0; i < 768; ++i) CHECK(frame.isValid(i));
    CHECK(!frame.isValid(768));
  });
  runTest("out_of_range_preserved_but_invalid", [] {
    auto bytes = yfFixture(); bytes[36] = 0x7F; bytes[37] = 0xFF; repairFixtureCrc(bytes);
    ThermalFrame frame;
    CHECK(decodeYf65(bytes.data(), bytes.size(), 1, frame) == ParseResult::Ok);
    CHECK(frame.temp10[0] == 32767 && !frame.isValid(0) && frame.isValid(1));
  });
  runTest("sensor_range_boundaries", [] {
    auto bytes = yfFixture();
    bytes[36] = 0xFE; bytes[37] = 0x70; // -400
    bytes[38] = 0x0B; bytes[39] = 0xB8; // 3000
    bytes[40] = 0xFE; bytes[41] = 0x6F; // -401
    bytes[42] = 0x0B; bytes[43] = 0xB9; // 3001
    repairFixtureCrc(bytes); ThermalFrame frame;
    CHECK(decodeYf65(bytes.data(), bytes.size(), 1, frame) == ParseResult::Ok);
    CHECK(frame.isValid(0) && frame.isValid(1) && !frame.isValid(2) && !frame.isValid(3));
  });
  runTest("wrong_address_crc_valid_preserves_output", [] {
    auto bytes = yfFixture(); bytes[0] = 2; repairFixtureCrc(bytes); rejected(bytes, ParseResult::WrongAddress);
  });
  runTest("wrong_function_crc_valid", [] {
    auto bytes = yfFixture(); bytes[1] = 3; repairFixtureCrc(bytes); rejected(bytes, ParseResult::WrongFunction);
  });
  runTest("exception_response", [] {
    std::vector<uint8_t> bytes{1, 0xE5, 2, 0, 0}; repairFixtureCrc(bytes); rejected(bytes, ParseResult::Exception);
  });
  runTest("declared_count_mismatch_crc_valid", [] {
    auto bytes = yfFixture(); bytes[3] = 0x20; repairFixtureCrc(bytes); rejected(bytes, ParseResult::LengthMismatch);
  });
  runTest("crc_error_and_truncation", [] {
    auto bytes = yfFixture(); bytes[60] ^= 1; rejected(bytes, ParseResult::CrcMismatch);
    bytes = yfFixture(); bytes.resize(100); rejected(bytes, ParseResult::LengthMismatch);
  });
  runTest("oversize_and_empty", [] {
    rejected(std::vector<uint8_t>(2049, 0), ParseResult::TooLong);
    rejected({}, ParseResult::TooShort); ThermalFrame frame;
    CHECK(decodeYf65(nullptr, 1576, 1, frame) == ParseResult::TooShort);
  });
  runTest("different_register_span_is_not_full_layout", [] {
    auto bytes = yfFixture(); bytes.insert(bytes.end() - 2, {0, 0});
    bytes[3] = 0x24; repairFixtureCrc(bytes); rejected(bytes, ParseResult::UnsupportedLayout);
  });
  if (argc == 3 && std::string(argv[1]) == "--capture") {
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) { std::cerr << "Cannot open capture\n"; return 2; }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    ThermalFrame frame;
    const auto result = decodeYf65(bytes.data(), bytes.size(), 1, frame);
    if (result != ParseResult::Ok) { std::cerr << "Capture rejected: " << static_cast<int>(result) << '\n'; return 2; }
    std::cout << "CAPTURE valid bytes=" << bytes.size() << " first=" << frame.temp10.front()
              << " last=" << frame.temp10.back() << " (counts, 0.1 C)\n";
  } else if (argc != 1) { std::cerr << "Usage: test_yf90640 [--capture frame.bin]\n"; return 2; }
  std::cout << "PASS wired_tire yf90640\n";
}
