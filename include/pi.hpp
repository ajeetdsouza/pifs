#pragma once

#include "pi.hpp"

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

static constexpr double series(int d, int j) {
  double sum = 0;

  for (auto k = 0; k <= d; ++k) {
    sum += static_cast<double>(binpow(16, d - k, 8 * k + j)) /
           static_cast<double>(8 * k + j);
    sum -= std::floor(sum);
  }

  for (auto k = d + 1;; ++k) {
    const auto inc =
        std::pow(static_cast<double>(16), static_cast<double>(d - k)) /
        static_cast<double>(8 * k + j);

    if (inc < 1e-7) break;

    sum += inc;
    sum -= std::floor(sum);
  }

  return sum;
}

constexpr int pi(int digit) {
  const auto s1 = series(digit, 1);
  const auto s4 = series(digit, 4);
  const auto s5 = series(digit, 5);
  const auto s6 = series(digit, 6);

  auto pi_digit = (4.0 * s1) - (2.0 * s4) - s5 - s6;
  pi_digit -= std::floor(pi_digit);

  const auto hex = static_cast<int>(16.0 * pi_digit);
  return hex;
}

class PiEncoder {
 private:
  std::array<int, 0x100> byte_to_idx;

 public:
  constexpr PiEncoder() : byte_to_idx() {
    for (auto& idx : byte_to_idx) idx = -1;  // std::fill is not a constexpr

    auto idx = 0;
    auto bits = pi(idx + 1) << 12 | pi(idx) << 8;

    for (size_t filled = 0; filled < byte_to_idx.size(); idx += 2) {
      bits = (pi(idx + 3) << 12 | pi(idx + 2) << 8 | bits >> 8) & 0xFFFF;

      for (auto bit = 0; bit < 8; ++bit) {
        const auto byte = (bits >> bit) & 0xFF;

        if (byte_to_idx[byte] == -1) {
          byte_to_idx[byte] = (8 * idx) + bit;
          if (++filled >= byte_to_idx.size()) break;
        }
      }
    }
  }

  constexpr int operator[](int byte) const { return byte_to_idx[byte]; }

  constexpr size_t size() const { return byte_to_idx.size(); }
};

class PiDecoder {
 private:
  std::unordered_map<int, int> idx_to_byte;

 public:
  explicit PiDecoder(const PiEncoder& encoder) : idx_to_byte() {
    for (size_t byte = 0x00; byte < encoder.size(); ++byte) {
      const auto idx = encoder[byte];
      idx_to_byte[idx] = byte;
    }
  }

  int operator[](int idx) { return idx_to_byte.at(idx); }

  size_t size() const {
    return idx_to_byte.size();
  }
};
