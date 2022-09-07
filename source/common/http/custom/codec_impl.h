#pragma once

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "envoy/common/optref.h"
#include "envoy/common/scope_tracker.h"
#include "envoy/config/core/v3/protocol.pb.h"
#include "envoy/http/codec.h"
#include "envoy/network/connection.h"

#include "source/common/buffer/watermark_buffer.h"
#include "source/common/common/assert.h"
#include "source/common/common/statusor.h"
#include "source/common/http/codec_helper.h"
#include "source/common/http/codes.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/custom/spex_codec.h"

namespace Envoy {
namespace Http {
namespace Custom {

class ConnectionImpl;

class StreamEncoderImpl: public virtual StreamEncoder,
                          public Stream,
                          public Logger::Loggable<Logger::Id::custom>,
                          public StreamCallbackHelper,
                          public Http1StreamEncoderOptions {
public:
  StreamEncoderImpl(ConnectionImpl& connection): connection_(connection){
    bytes_meter_ = std::make_shared<StreamInfo::BytesMeter>();
  }
  ~StreamEncoderImpl() override {}
  
  ConnectionImpl& connection() {
    return connection_;
  }

  // Http::StreamEncoder
  void encodeData(Buffer::Instance& data, bool end_stream) override;
  void encodeMetadata(const MetadataMapVector&) override;
  Stream& getStream() override { return *this; }
  Http1StreamEncoderOptionsOptRef http1StreamEncoderOptions() override { return *this; }

  void disableChunkEncoding() override {}

  // Http::Stream
  void resetStream(StreamResetReason reason) override;

  void addCallbacks(StreamCallbacks& callbacks) override;

  void removeCallbacks(StreamCallbacks& callbacks) override;

  void readDisable(bool disable) override;

  uint32_t bufferLimit() const override { return 10*1024; }

  absl::string_view responseDetails() override { return ""; }

  const Network::Address::InstanceConstSharedPtr& connectionLocalAddress() override;

  void setFlushTimeout(std::chrono::milliseconds timeout) override;

  void setAccount(Buffer::BufferMemoryAccountSharedPtr account) override;

  const StreamInfo::BytesMeterSharedPtr& bytesMeter() override;

private:
  ConnectionImpl& connection_;
  StreamInfo::BytesMeterSharedPtr bytes_meter_;
};

class ConnectionImpl : public virtual Connection,
                       protected Logger::Loggable<Logger::Id::custom>,
                       public ScopeTrackedObject {
public:
  ConnectionImpl(Network::Connection& connection);

  Network::Connection& connection() {
    ENVOY_LOG(error, "encode data");
    return connection_; 
  }

  const Network::Connection& connection() const {
    ENVOY_LOG(error, "encode data  ");
    return connection_;
  }

  // Http::Connection
  Http::Status dispatch(Buffer::Instance& data) override;
  void goAway() override {} // Called during connection manager drain flow
  Protocol protocol() override { return Protocol::Http2; }
  void shutdownNotice() override {} // Called during connection manager drain flow
  bool wantsToWrite() override { return false; }
  void onUnderlyingConnectionAboveWriteBufferHighWatermark() override { onAboveHighWatermark(); }
  void onUnderlyingConnectionBelowWriteBufferLowWatermark() override { onBelowLowWatermark(); }


  // ScopeTrackedObject
  void dumpState(std::ostream& os, int indent_level) const override;

  /**
   * Called when output_buffer_ or the underlying connection go from below a low watermark to over
   * a high watermark.
   */
  virtual void onAboveHighWatermark() PURE;

  /**
   * Called when output_buffer_ or the underlying connection  go from above a high watermark to
   * below a low watermark.
   */
  virtual void onBelowLowWatermark() PURE;

  Network::Connection& connection_;

  void onResetStreamBase(StreamResetReason reason);
};

class ResponseEncoderImpl : public StreamEncoderImpl, public ResponseEncoder {
public:
  ResponseEncoderImpl(ConnectionImpl& connection)
      : StreamEncoderImpl(connection),
      owned_output_buffer_(std::make_unique<Buffer::OwnedImpl>()) {

  }

  ~ResponseEncoderImpl() override {
  }

  void resetStream(StreamResetReason reason) override;

  void encode1xxHeaders(const ResponseHeaderMap& headers) override;

  void encodeData(Buffer::Instance& data, bool end_stream) override;

  void encodeHeaders(const ResponseHeaderMap& headers, bool end_stream) override;

  void encodeTrailers(const ResponseTrailerMap& trailers) override;

  bool streamErrorOnInvalidHttpMessage() const override { return true; };

//private:
  std::string id;
  Buffer::InstancePtr owned_output_buffer_;
};

class ServerConnectionImpl : public ServerConnection, public ConnectionImpl {
public:
  Http::Status dispatch(Buffer::Instance& data) override;
  void onAboveHighWatermark() override {}
  void onBelowLowWatermark() override {}
  ServerConnectionImpl(Network::Connection& connection,
                                         Http::ServerConnectionCallbacks& callbacks);

protected:
  struct ActiveRequest : public Event::DeferredDeletable {
    ActiveRequest(ServerConnectionImpl& connection)
        : response_encoder_(connection) {}
    ~ActiveRequest() override = default;

    void dumpState(std::ostream& os, int indent_level) const;
    // HeaderString request_url_;
    RequestDecoder* request_decoder_{};
    ResponseEncoderImpl response_encoder_;
    // bool remote_complete_{};
  };
  
  using ActiveRequestPtr = std::unique_ptr<ActiveRequest>;

//private:
  std::list<ActiveRequestPtr> active_requests_;
  Network::Connection& connection_;
  Http::ServerConnectionCallbacks& callbacks_;
  SpexCodec spex_codec_;
};

}// Custom
}// Http
}// Envoy