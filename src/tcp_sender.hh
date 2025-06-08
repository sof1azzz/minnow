#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <map>

class TCPSender {
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender(ByteStream &&input, Wrap32 isn, uint64_t initial_RTO_ms)
    : input_(std::move(input)), isn_(isn), initial_RTO_ms_(initial_RTO_ms),
    _timer_running_(false), _time_since_last_transmission(0), _current_RTO(initial_RTO_ms),
    _sequence_numbers_in_flight(0), _consecutive_retransmissions(0),
  syn_sent(false), fin_sent(false), _min_seqno(0), _receiver_window_size(1) {
  }

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive(const TCPReceiverMessage &msg);

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void(const TCPSenderMessage &)>;

  /* Push bytes from the outbound stream */
  void push(const TransmitFunction &transmit);

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick(uint64_t ms_since_last_tick, const TransmitFunction &transmit);

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer &writer() const { return input_.writer(); }
  const Reader &reader() const { return input_.reader(); }
  Writer &writer() { return input_.writer(); }


private:
  Reader &reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool _timer_running_;
  uint64_t _time_since_last_transmission;
  uint64_t _current_RTO;

  uint64_t _sequence_numbers_in_flight;
  uint64_t _consecutive_retransmissions;
  
  bool syn_sent;
  bool fin_sent;

  uint64_t _min_seqno;
  uint16_t _receiver_window_size;

  std::map<uint64_t, TCPSenderMessage> _outstanding_segments{};
};
