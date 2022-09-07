#include "source/common/http/custom/spex_codec.h"

namespace Envoy {
namespace Http {
namespace Custom {

CodecStatus SpexCodec::decode() {
    if(buffer_->length() < SpexMessage::HEADER_SIZE){
        return CodecStatus::MORE_DATA;
    }

    while(true){
        switch (this->state_) {
            case ParseState::HEADER_LENGTH:{
                this->total_len_ = buffer_->drainLEInt<uint32_t>();
                this->header_len_ = buffer_->drainLEInt<uint16_t>();

                this->state_ = ParseState::HEADER_BODY;
                ENVOY_LOG(trace, "message length: {}, {}", this->total_len_, this->header_len_);
            }
            break;
            case ParseState::HEADER_BODY: {
                if(this->buffer_->length() < this->header_len_){
                    return CodecStatus::MORE_DATA;
                }

                void* start = this->buffer_->linearize(this->header_len_);
                bool ok = this->pending_msg_->header_.ParseFromArray(static_cast<char*>(start), this->header_len_);
                this->buffer_->drain(this->header_len_);
                ENVOY_LOG(trace, "header {}, {}, buf len {}", this->pending_msg_->header_.DebugString(), ok, this->buffer_->length());
                this->state_ = ParseState::BODY;
            }
            break;
            case ParseState::BODY:{
                uint64_t body_len = this->total_len_ - sizeof(u_int16_t) - this->header_len_;

                if(this->buffer_->length() < body_len){
                    return CodecStatus::MORE_DATA;
                }

                this->pending_msg_->body_->move(*(this->buffer_.get()), body_len);
                ENVOY_LOG(trace, "body length {}, buf left {}", this->pending_msg_->body_->length(), this->buffer_->length());

                return CodecStatus::MESSAGE_COMPLETE;
                //this->reset();
            }
            break;
        }
    }

    return CodecStatus::MORE_DATA;
}

std::unique_ptr<Buffer::OwnedImpl> SpexCodec::encode(SpexMessage& msg){
    (void)(msg);
    std::unique_ptr<Buffer::OwnedImpl> buffer_ = std::make_unique<Buffer::OwnedImpl>();
    
    return buffer_;
}

}// Custom
}// Http
}// Envoy