#pragma once

#include <cstddef>

namespace Envoy {
namespace Http {

/**
 * Possible HTTP connection/request protocols. The parallel NumProtocols constant allows defining
 * fixed arrays for each protocol, but does not pollute the enum.
 */
enum class Protocol { Http10, Http11, Http2, Http3, CUSTOM };
const size_t NumProtocols = 5;

} // namespace Http
} // namespace Envoy
