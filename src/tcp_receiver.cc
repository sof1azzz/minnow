#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
  if (message.RST) {
    reader().set_error();
    return;
  }
  if (message.SYN) {
    zero_point_ = Wrap32(message.seqno);
    ack_.emplace(message.seqno);
  }
  if (ack_.has_value()) {
    const uint64_t check_point = writer().bytes_pushed() + 1;
    uint64_t first_index
      = Wrap32(message.SYN ? message.seqno + 1 : message.seqno).unwrap(zero_point_, check_point) - 1;
    reassembler_.insert(first_index, std::move(message.payload), message.FIN);
    ack_ = ack_->wrap(writer().bytes_pushed() + 1 + writer().is_closed(), zero_point_);
  }
}

TCPReceiverMessage TCPReceiver::send() const {
  return { ack_,
           static_cast<uint16_t>(min(reassembler_.writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX))),
           reader().has_error() };
}
