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
    ENVOY_LOG(trace, "encode response data {}, end_stream {}", data.length(), end_stream);
    this->connection().connection_.write(data, end_stream);
}

void ResponseEncoderImpl::encodeHeaders(const ResponseHeaderMap& headers, bool end_stream) {
  (void)(end_stream);
  ENVOY_LOG(trace, "encode response header {}", headers.size());
  headers.dumpState(std::cout, 2);

  auto spex_msg = headers.get_extension<SpexMessage *>();
  if(spex_msg.has_value()){
    ENVOY_LOG(trace, "encode response header spex, msg {}", spex_msg.value()->header_.DebugString());

    Buffer::InstancePtr buffer = std::move(spex_msg.value()->raw_header_);
    this->connection().connection_.write(*buffer, false);
  } else {
    Envoy::Http::LowerCaseString id_key("sp-id");
    const auto id_value = headers.get(id_key);

    SpexMessage msg;
    std::vector<uint8_t> sp_id = Envoy::Hex::decode(std::string(id_value[0]->value().getStringView()));
    msg.header_.set_id(std::string(sp_id.begin(), sp_id.end()));
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

}

void ResponseEncoderImpl::encodeTrailers(const ResponseTrailerMap& trailers) {
    (void)(trailers);
}

void RequestEncoderImpl::encodeData(Buffer::Instance& data, bool end_stream) {
    (void)(end_stream);
    (void)(data);
    ENVOY_LOG(trace, "encode request data {}, end_stream {}", data.length(), end_stream);
    this->connection().connection_.write(data, end_stream);
}

Http::Status RequestEncoderImpl::encodeHeaders(const RequestHeaderMap& headers, bool end_stream) {
  (void)(end_stream);
  headers.dumpState(std::cout, 2);

  auto spex_msg = headers.get_extension<SpexMessage *>();
  //Buffer::InstancePtr buffer = std::move(spex_msg.value()->raw_header_);
  ENVOY_LOG(trace, "encode request header {}, msg {}", headers.size(), spex_msg.value()->header_.DebugString());

  Envoy::Http::LowerCaseString authority_key(":authority");
  const auto authority_value = headers.get(authority_key);
  spex_msg.value()->header_.set_destination(std::string(authority_value[0]->value().getStringView()));

  std::string header_content;
  spex_msg.value()->header_.SerializeToString(&header_content);

  auto output_buffer = std::make_unique<Buffer::OwnedImpl>();
  int body_len = std::stoi(std::string(headers.ContentLength()->value().getStringView()));
  int total_len = header_content.size() + 2 + body_len;

  output_buffer->writeLEInt<uint32_t>(total_len);
  output_buffer->writeLEInt<uint16_t>(header_content.size());
  output_buffer->addFragments({header_content});
    
  Buffer::InstancePtr buffer = std::move(output_buffer);

  this->connection().connection_.write(*buffer, false);

  return Http::okStatus();
}

void RequestEncoderImpl::encodeTrailers(const RequestTrailerMap& trailers) {
    (void)(trailers);
}

void RequestEncoderImpl::resetStream(StreamResetReason reason) {
  StreamEncoderImpl::resetStream(reason);
}

ServerConnectionImpl::ServerConnectionImpl(Network::Connection& connection,
    Http::ServerConnectionCallbacks& callbacks):
    ConnectionImpl(connection),
    connection_(connection), callbacks_(callbacks),
    spex_codec_() {
  connection.enableHalfClose(true);
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
    Envoy::Http::LowerCaseString id_key("sp-id");

    Envoy::Http::LowerCaseString path_key(":path"), path_value(std::string("/"));
    Envoy::Http::LowerCaseString method_key(":method"), method_value(std::string("POST"));

    headers->setContentLength(msg->body_->length());

    headers->addCopy(host_key, msg->header_.command());
    headers->addCopy(cmd_key, msg->header_.command());
    Buffer::InstancePtr body = std::move(msg->body_);

    headers->set_extension(msg.release());

    headers->addCopy(id_key, Envoy::Hex::encode(reinterpret_cast<uint8_t *>(id.data()), id.length()));

    headers->addCopy(path_key, path_value);
    headers->addCopy(method_key, method_value);

    active_request->request_decoder_->decodeHeaders(std::move(headers), false);
    active_request->request_decoder_->decodeData(*body, false);

    this->active_requests_.push_back(std::move(active_request));

    break;
  }
  return Http::okStatus();
}

Http::Status ClientConnectionImpl::dispatch(Buffer::Instance& data) {
  // TODO, change to callback instead
  this->spex_codec_.buffer_->move(data);
  ENVOY_CONN_LOG(trace, "buffering {} bytes", this->connection_, this->spex_codec_.buffer_->length());

  CodecStatus status = this->spex_codec_.decode();
  while(status == CodecStatus::MESSAGE_COMPLETE){
    auto msg = this->spex_codec_.drainMessage();
    auto id = msg->header_.id();
    ENVOY_CONN_LOG(trace, "got message: traceid-{}", this->connection_, 
      Envoy::Hex::encode(reinterpret_cast<uint8_t *>(id.data()), id.length()));
    
    ResponseHeaderMapPtr headers = ResponseHeaderMapImpl::create();
    Envoy::Http::LowerCaseString host_key("host");
    Envoy::Http::LowerCaseString cmd_key("sp-cmd");
    Envoy::Http::LowerCaseString id_key("sp-id");

    Envoy::Http::LowerCaseString path_key(":path"), path_value(std::string("/"));
    Envoy::Http::LowerCaseString method_key(":method"), method_value(std::string("POST"));

    headers->setContentLength(msg->body_->length());
    headers->setStatus(200);

    headers->addCopy(host_key, msg->header_.command());
    headers->addCopy(cmd_key, msg->header_.command());
    headers->addCopy(id_key, Envoy::Hex::encode(reinterpret_cast<uint8_t *>(id.data()), id.length()));

    headers->addCopy(path_key, path_value);
    headers->addCopy(method_key, method_value);
    
    Buffer::InstancePtr body = std::move(msg->body_);

    headers->set_extension(msg.release());

    this->decoder_->decodeHeaders(std::move(headers), false);
    this->decoder_->decodeData(*body, false);

    break;
  }

  return Http::okStatus();
}
}// Custom
}// Http
}// Envoy
