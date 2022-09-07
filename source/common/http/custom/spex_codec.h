#pragma once

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"
#include "source/common/http/custom/spex_codec.pb.h"

namespace Envoy {
namespace Http {
namespace Custom {

struct SpexMessage : public Logger::Loggable<Logger::Id::spex> {
  SpexMessage() {
    this->body_ = std::make_unique<Buffer::OwnedImpl>();
  }

  static const uint32_t HEADER_SIZE = 6;
  uint32_t total_len_;
  uint32_t header_len_;
  ::sp::common::SpexHeader header_;
  std::unique_ptr<Buffer::OwnedImpl> body_;
};

using SpexMessagePtr = std::unique_ptr<SpexMessage>;

enum CodecStatus {
  MORE_DATA = 0,
  MESSAGE_COMPLETE = 1,
  ERROR = 2,
};

enum ParseState {
  HEADER_LENGTH = 0,
  HEADER_BODY = 1,
  BODY = 2,
};

class SpexCodec : public Logger::Loggable<Logger::Id::spex> {
public:
  SpexCodec() {
    buffer_ = std::make_unique<Buffer::OwnedImpl>();
    state_ = ParseState::HEADER_LENGTH;
    pending_msg_ = std::make_unique<SpexMessage>();
  }

  CodecStatus decode();
  std::unique_ptr<Buffer::OwnedImpl> encode(SpexMessage& buffer);

  SpexMessagePtr drainMessage() {
    auto result = std::move(this->pending_msg_);
    this->reset();
    return result;
  }

//private:
  std::unique_ptr<Buffer::OwnedImpl> buffer_;
private:
  void reset() {
    this->state_ = ParseState::HEADER_LENGTH;
    this->pending_msg_.reset(new SpexMessage());
  }

  SpexMessagePtr pending_msg_;
  ParseState state_;
  uint32_t total_len_;
  uint32_t header_len_;
};

}// Custom
}// Http
}// Envoy