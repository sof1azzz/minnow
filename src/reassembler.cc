#include "reassembler.hh"
#include <ranges>

void Reassembler::insert(uint64_t first_index, std::string data, bool is_last_substring) {
  if (output_.writer().is_closed()) {
    return;
  }

  if (is_last_substring) {
    last_index_ = first_index + data.size();
  }

  if (last_index_.has_value() && left_most_index >= last_index_) {
    output_.writer().close();
    return;
  }

  auto available_capacity = output_.writer().available_capacity();
  uint64_t max_acceptable_index = left_most_index + available_capacity;

  // 如果数据开始位置超出容量范围，直接返回
  if (first_index >= max_acceptable_index) {
    return;
  }

  // 处理数据在期待索引之前的情况
  if (first_index < left_most_index) {
    uint64_t right_index = first_index + data.size();
    if (right_index <= left_most_index) {
      return; // 数据完全过时
    }
    // 截取有用部分
    data = data.substr(left_most_index - first_index);
    first_index = left_most_index;
  }

  // 限制数据长度，确保不超出容量范围
  uint64_t right_index = first_index + data.size();
  if (right_index > max_acceptable_index) {
    data.resize(max_acceptable_index - first_index);
    right_index = first_index + data.size();
  }

  // 如果截断后数据为空，直接返回
  if (data.empty()) {
    return;
  }

  // 处理与现有片段的重叠
  auto it = segments_.lower_bound(first_index);

  // 检查前一个片段是否重叠
  if (it != segments_.begin()) {
    auto prev_it = std::prev(it);
    uint64_t prev_right = prev_it->first + prev_it->second.size();

    if (prev_right > first_index) {
      if (prev_right >= right_index) {
        return; // 新数据完全被包含
      }

      // 合并前一个片段
      size_t old_size = prev_it->second.size();
      prev_it->second += data.substr(prev_right - first_index);
      first_index = prev_it->first;
      data = prev_it->second;
      right_index = first_index + data.size();

      bytes_pending -= old_size;
      segments_.erase(prev_it);
    }
  }

  // 处理后续重叠的片段
  while (it != segments_.end() && it->first < right_index) {
    uint64_t seg_right = it->first + it->second.size();

    if (seg_right > right_index) {
      // 部分重叠，合并后续部分
      data += it->second.substr(right_index - it->first);
      right_index = first_index + data.size();
    }

    bytes_pending -= it->second.size();
    auto temp = it++;
    segments_.erase(temp);
  }

  // 插入新片段
  segments_[first_index] = data;
  bytes_pending += data.size();

  // 尝试输出连续数据
  while (!segments_.empty() && segments_.begin()->first == left_most_index) {
    auto &segment = segments_.begin()->second;
    auto curr_available_capacity = output_.writer().available_capacity();

    if (segment.size() <= curr_available_capacity) {
      // 完整写入
      output_.writer().push(segment);
      left_most_index += segment.size();
      bytes_pending -= segment.size();
      segments_.erase(segments_.begin());
    } else {
      // 部分写入
      std::string to_write = segment.substr(0, curr_available_capacity);
      output_.writer().push(to_write);
      left_most_index += curr_available_capacity;
      bytes_pending -= curr_available_capacity;

      // 更新剩余部分
      std::string remaining = segment.substr(curr_available_capacity);
      segments_.erase(segments_.begin());
      segments_[left_most_index] = remaining;
      break;
    }
  }

  if (last_index_.has_value() && left_most_index >= last_index_.value()) {
    output_.writer().close();
  }
}

uint64_t Reassembler::count_bytes_pending() const {
  return bytes_pending;
}
