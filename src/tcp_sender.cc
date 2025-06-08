#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const {
  return _sequence_numbers_in_flight;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const {
  return _consecutive_retransmissions;
}

void TCPSender::push(const TransmitFunction &transmit) {
  // 计算有效窗口大小（处理零窗口特殊情况）
  uint64_t effective_window_size = _receiver_window_size;
  if (_receiver_window_size == 0) {
    effective_window_size = 1;  // 零窗口特殊处理
  }

  // 计算可用空间
  uint64_t available_space = 0;
  if (effective_window_size > _sequence_numbers_in_flight) {
    available_space = effective_window_size - _sequence_numbers_in_flight;
  }

  while (available_space > 0) {
    TCPSenderMessage msg = make_empty_message();
    bool segment_sent = false;

    // 发送SYN（如果还未发送）
    if (!syn_sent) {
      msg.SYN = true;
      syn_sent = true;
      available_space--;
      segment_sent = true;

      // 如果没有更多空间，直接发送SYN段
      if (available_space == 0) {
        _outstanding_segments[_min_seqno] = msg;
        _min_seqno++;
        _sequence_numbers_in_flight++;
        transmit(msg);

        // 启动计时器
        if (!_timer_running_) {
          _timer_running_ = true;
          _time_since_last_transmission = 0;
        }
        return;
      }
    }

    // 发送数据（如果有可用数据和空间）
    if (reader().bytes_buffered() > 0 && available_space > 0) {
      uint64_t payload_size = min({ available_space,
                                  static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE),
                                  reader().bytes_buffered() });

      read(reader(), payload_size, msg.payload);
      available_space -= payload_size;
      segment_sent = true;
    }

    // 发送FIN（如果流结束且还未发送FIN）
    if (!fin_sent && reader().is_finished() && available_space > 0) {
      msg.FIN = true;
      fin_sent = true;
      available_space--;
      segment_sent = true;
    }

    // 如果构造了一个段，发送它
    if (segment_sent) {
      uint64_t sequence_length = msg.sequence_length();
      _outstanding_segments[_min_seqno] = msg;
      _min_seqno += sequence_length;
      _sequence_numbers_in_flight += sequence_length;
      transmit(msg);

      // 启动计时器（如果段包含数据）
      if (sequence_length > 0 && !_timer_running_) {
        _timer_running_ = true;
        _time_since_last_transmission = 0;
      }
    } else {
      // 没有更多数据要发送，退出循环
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const {
  return { Wrap32::wrap(_min_seqno, isn_), false, {}, false, input_.has_error() };
}

void TCPSender::receive(const TCPReceiverMessage &msg) {
  if (input_.has_error()) {
    return;
  }

  if (msg.RST) {
    input_.set_error();
    return;
  }

  _receiver_window_size = msg.window_size;

  if (!msg.ackno.has_value()) {
    return;
  }

  uint64_t recv_ackno = msg.ackno->unwrap(isn_, _min_seqno);

  // 检查ackno是否合法（不能确认还未发送的数据）
  if (recv_ackno > _min_seqno) {
    return;  // 确认号超出已发送范围
  }

  // 检查是否确认了新数据
  bool acknowledged_new_data = false;

  // 移除所有被完全确认的段
  auto it = _outstanding_segments.begin();
  while (it != _outstanding_segments.end()) {
    uint64_t seg_start = it->first;
    uint64_t seg_end = seg_start + it->second.sequence_length();

    if (seg_end <= recv_ackno) {
      // 这个段被完全确认了
      _sequence_numbers_in_flight -= it->second.sequence_length();
      acknowledged_new_data = true;
      it = _outstanding_segments.erase(it);
    } else {
      break;  // 后面的段还没被确认
    }
  }

  // 如果确认了新数据，更新重传相关状态
  if (acknowledged_new_data) {
    _current_RTO = initial_RTO_ms_;  // 重置RTO为初始值
    _consecutive_retransmissions = 0;  // 重置连续重传计数

    // 如果还有未确认数据，重启计时器
    if (!_outstanding_segments.empty()) {
      _timer_running_ = true;
      _time_since_last_transmission = 0;
    } else {
      _timer_running_ = false;  // 没有未确认数据，停止计时器
    }
  }
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction &transmit) {
  if (!_timer_running_) {
    return;
  }

  _time_since_last_transmission += ms_since_last_tick;
  if (_time_since_last_transmission >= _current_RTO) {
    if (_outstanding_segments.empty()) {
      _timer_running_ = false;
      return;
    }

    // 重传最早的segment
    transmit(_outstanding_segments.begin()->second);

    // 更新重传状态
    _consecutive_retransmissions++;
    if (_receiver_window_size > 0) {
      _current_RTO *= 2;
    }
    _time_since_last_transmission = 0;
  }
}
