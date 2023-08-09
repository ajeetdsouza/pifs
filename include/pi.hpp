#pragma once

#include "pi.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>

static constexpr uint64_t binpow(uint64_t a, uint64_t b, uint64_t mod) {
  a %= mod;
  uint64_t result = 1;
  while (b > 0) {
    if (b & 1) result = (result * a) % mod;
    a = (a * a) % mod;
    b >>= 1;
  }
  return result;
}

static constexpr double series(uint16_t d, int j) {
  double sum = 0;

  for (uint16_t k = 0; k <= d; ++k) {
    sum += static_cast<double>(binpow(16, d - k, 8 * k + j)) / (8 * k + j);
    sum -= std::floor(sum);
  }

  for (uint16_t k = d + 1;; ++k) {
    const double inc = std::pow(16., static_cast<double>(d - k)) / (8 * k + j);

    if (inc < 1e-7) {
      break;
    }

    sum += inc;
    sum -= std::floor(sum);
  }

  return sum;
}

constexpr uint8_t pi(uint16_t digit) {
  const double s1 = series(digit, 1);
  const double s4 = series(digit, 4);
  const double s5 = series(digit, 5);
  const double s6 = series(digit, 6);

  double pi_digit = 4 * s1 - 2 * s4 - s5 - s6;
  pi_digit -= std::floor(pi_digit);

  return 16 * pi_digit;
}

class PiEncoder {
 private:
  std::array<uint16_t, 0x100> byte_to_idx;

 public:
  constexpr PiEncoder() : byte_to_idx{} {
    std::array<bool, 0x100> bitset{};

    uint16_t idx = 0;
    uint16_t bits = pi(idx + 1) << 12 | pi(idx) << 8;

    for (size_t filled = 0; filled < byte_to_idx.size(); idx += 2) {
      bits = (pi(idx + 3) << 12 | pi(idx + 2) << 8 | bits >> 8);

      for (int bit = 0; bit < 8; ++bit) {
        const uint8_t byte = (bits >> bit);
        if (!bitset[byte]) {
          bitset[byte] = true;
          byte_to_idx[byte] = 8 * idx + bit;
          if (++filled >= byte_to_idx.size()) {
            break;
          }
        }
      }
    }
  }

  constexpr uint16_t operator[](uint8_t byte) const {
    return byte_to_idx.at(byte);
  }

  constexpr size_t size() const { return byte_to_idx.size(); }
};

class PiDecoder {
 private:
  std::unordered_map<int, int> idx_to_byte;

 public:
  explicit PiDecoder(const PiEncoder& encoder) : idx_to_byte() {
    for (size_t byte = 0x00; byte < encoder.size(); ++byte) {
      const uint16_t idx = encoder[byte];
      idx_to_byte[idx] = byte;
    }
  }

  uint8_t operator[](uint16_t idx) { return idx_to_byte.at(idx); }

  size_t size() const { return idx_to_byte.size(); }
};
