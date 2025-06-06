#pragma once

#include "byte_stream.hh"
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <map>

class Reassembler {
public:
  explicit Reassembler(ByteStream &&output)
    : output_(std::move(output)),
    segments_{},
    left_most_index(0),
    last_index_(),
    bytes_pending(0) {
  }

  void insert(uint64_t first_index, std::string data, bool is_last_substring);
  uint64_t count_bytes_pending() const;

  Reader &reader() { return output_.reader(); }
  const Reader &reader() const { return output_.reader(); }
  const Writer &writer() const { return output_.writer(); }

private:
  ByteStream output_;
  std::map<uint64_t, std::string> segments_;
  uint64_t left_most_index;
  std::optional<uint64_t> last_index_;
  uint64_t bytes_pending;
};