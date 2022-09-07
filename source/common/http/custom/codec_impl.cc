#include "source/common/common/hex.h"

#include "source/common/http/custom/codec_impl.h"
#include "source/common/common/scope_tracker.h"
#include "source/common/http/custom/spex_codec.pb.h"

namespace Envoy {
namespace Http {
namespace Custom {

void StreamEncoderImpl::encodeData(Buffer::Instance& data, bool end_stream) {
    (void)(end_stream);
    (void)(data);
    ENVOY_LOG(trace, "encode data {}", data.toString());
}

void StreamEncoderImpl::encodeMetadata(const MetadataMapVector&) {

}

// Http::Stream
void StreamEncoderImpl::resetStream(StreamResetReason reason) {
  connection_.onResetStreamBase(reason);
}


void StreamEncoderImpl::addCallbacks(StreamCallbacks& ) {

}

void StreamEncoderImpl::removeCallbacks(StreamCallbacks& ) {

}

void StreamEncoderImpl::readDisable(bool) {

}

const Network::Address::InstanceConstSharedPtr& StreamEncoderImpl::connectionLocalAddress() {
  return connection_.connection().connectionInfoProvider().localAddress();
}

void StreamEncoderImpl::setFlushTimeout(std::chrono::milliseconds) {

}

void StreamEncoderImpl::setAccount(Buffer::BufferMemoryAccountSharedPtr) {

}

const StreamInfo::BytesMeterSharedPtr& StreamEncoderImpl::bytesMeter() {
    return this->bytes_meter_;
}



ConnectionImpl::ConnectionImpl(Network::Connection& connection):
    connection_(connection){

}

Http::Status ConnectionImpl::dispatch(Buffer::Instance& data){
  // Add self to the Dispatcher's tracked object stack.
  ScopeTrackerScopeState scope(this, connection_.dispatcher());
  ENVOY_CONN_LOG(trace, "parsing {} bytes", connection_, data.length());    
  
  return Http::okStatus();
}

void ConnectionImpl::onResetStreamBase(StreamResetReason reason){
  (void)(reason);
}

void ConnectionImpl::dumpState(std::ostream& os, int indent_level) const {
    os << indent_level << std::endl;
}

void ResponseEncoderImpl::resetStream(StreamResetReason reason) {
  // For H1, we use idleTimeouts to cancel streams unless there was an
  // explicit protocol error prior to sending a response to the downstream
  // in which case we send a local reply.
  // TODO(kbaichoo): If we want snappier resets of H1 streams we can
  //  1) Send local reply if no response data sent yet
  //  2) Invoke the idle timeout sooner to close underlying connection
  StreamEncoderImpl::resetStream(reason);
}

void ResponseEncoderImpl::encode1xxHeaders(const ResponseHeaderMap& headers) {
    (void)(headers);
    ENVOY_LOG(trace, "encode 1xx headers {}", headers.size());
}

void ResponseEncoderImpl::encodeData(Buffer::Instance& data, bool end_stream) {
    (void)(end_stream);
    (void)(data);
    ENVOY_LOG(trace, "encode data {}, end_stream {}", data.length(), end_stream);
    if(!end_stream){
      this->connection().connection_.write(data, end_stream);
    }
}

void ResponseEncoderImpl::encodeHeaders(const ResponseHeaderMap& headers, bool end_stream) {
  (void)(end_stream);
  headers.dumpState(std::cout, 2);
  ENVOY_LOG(trace, "encode header {}", headers.size());

  SpexMessage msg;
  msg.header_.set_id(this->id);
  msg.header_.set_flag(sp::common::Constant_SpexHeaderFlag::Constant_SpexHeaderFlag_RPC_REPLY);
  std::string header_content;
  msg.header_.SerializeToString(&header_content);

  owned_output_buffer_ = std::make_unique<Buffer::OwnedImpl>();
  int body_len = std::stoi(std::string(headers.ContentLength()->value().getStringView()));
  int total_len = header_content.size() + 2 + body_len;

  owned_output_buffer_->writeLEInt<uint32_t>(total_len);
  owned_output_buffer_->writeLEInt<uint16_t>(header_content.size());
  owned_output_buffer_->addFragments({header_content});
  
  Buffer::InstancePtr buffer = std::move(owned_output_buffer_);
  this->connection().connection_.write(*buffer, false);
}

void ResponseEncoderImpl::encodeTrailers(const ResponseTrailerMap& trailers) {
    (void)(trailers);
}

ServerConnectionImpl::ServerConnectionImpl(Network::Connection& connection,
    Http::ServerConnectionCallbacks& callbacks):
    ConnectionImpl(connection),
    connection_(connection), callbacks_(callbacks),
    spex_codec_() {
        
}

Http::Status ServerConnectionImpl::dispatch(Buffer::Instance& data) {
  // TODO, change to callback instead
  this->spex_codec_.buffer_->move(data);
  ENVOY_CONN_LOG(trace, "buffering {} bytes", this->connection_, this->spex_codec_.buffer_->length());

  CodecStatus status = this->spex_codec_.decode();
  while(status == CodecStatus::MESSAGE_COMPLETE){
    auto msg = this->spex_codec_.drainMessage();
    auto id = msg->header_.id();
    ENVOY_CONN_LOG(trace, "got message: traceid-{}", this->connection_, 
      Envoy::Hex::encode(reinterpret_cast<uint8_t *>(id.data()), id.length()));
    
    ActiveRequestPtr active_request = std::make_unique<ActiveRequest>(*this);
    active_request->response_encoder_.id = id;
    active_request->request_decoder_ = &callbacks_.newStream(active_request->response_encoder_);
    
    RequestHeaderMapPtr headers = RequestHeaderMapImpl::create();
    Envoy::Http::LowerCaseString host_key("host");
    Envoy::Http::LowerCaseString cmd_key("sp-cmd");

    Envoy::Http::LowerCaseString path_key(":path"), path_value(std::string("/"));
    Envoy::Http::LowerCaseString method_key(":method"), method_value(std::string("POST"));

    headers->addCopy(host_key, msg->header_.command());
    headers->addCopy(cmd_key, msg->header_.command());

    headers->addCopy(path_key, path_value);
    headers->addCopy(method_key, method_value);

    active_request->request_decoder_->decodeHeaders(std::move(headers), false);
    Buffer::InstancePtr body = std::move(msg->body_);
    active_request->request_decoder_->decodeData(*body, true);

    this->active_requests_.push_back(std::move(active_request));

    break;
  }
  return Http::okStatus();
}

}// Custom
}// Http
}// Envoy
