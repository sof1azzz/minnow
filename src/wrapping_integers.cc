#include "wrapping_integers.hh"
#include "debug.hh"
#include <cstdlib>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
  return Wrap32((static_cast<uint64_t>(zero_point.raw_value_) + (n % (1ULL << 32))) % (1ULL << 32));
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t base = checkpoint & 0xFFFFFFFF00000000ULL;
  uint64_t candidate = base + offset;

  // 计算与 checkpoint 的距离（无符号差值，避免符号溢出）
  auto abs_diff = [](uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
    };

  uint64_t best = candidate;
  uint64_t best_diff = abs_diff(candidate, checkpoint);

  // 检查前一个周期的候选值（避免下溢）
  if (candidate >= (1ULL << 32)) {
    uint64_t candidate_prev = candidate - (1ULL << 32);
    uint64_t diff_prev = abs_diff(candidate_prev, checkpoint);
    if (diff_prev < best_diff) {
      best = candidate_prev;
      best_diff = diff_prev;
    }
  }

  // 检查下一个周期的候选值
  uint64_t candidate_next = candidate + (1ULL << 32);
  uint64_t diff_next = abs_diff(candidate_next, checkpoint);
  if (diff_next < best_diff) {
    best = candidate_next;
  }

  return best;
}