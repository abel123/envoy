load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

# compile proto
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])  # Apache 2

envoy_package()

api_proto_package(
    deps = [
        #"@com_github_cncf_udpa//udpa/annotations:pkg"
        ],
)


# compile proto
#load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

#load("@rules_proto//proto:defs.bzl", "proto_library")

envoy_cc_library(
    name = "codec_lib",
    srcs = [
        "codec_impl.cc",
        "spex_codec.cc",
        ],

    hdrs = [
        "codec_impl.h",
        "spex_codec.h",
        ],

    deps = [
        "//envoy/buffer:buffer_interface",
        "//envoy/common:scope_tracker_interface",
        "//envoy/http:codec_interface",
        "//envoy/http:header_map_interface",
        "//envoy/network:connection_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/buffer:watermark_buffer_lib",
        "//source/common/common:assert_lib",
        "//source/common/common:cleanup_lib",
        "//source/common/common:hex_lib",
        "//source/common/common:dump_state_utils",
        "//source/common/common:statusor_lib",
        "//source/common/common:utility_lib",
        "//source/common/grpc:common_lib",
        "//source/common/http:codec_helper_lib",
        "//source/common/http:codes_lib",
        "//source/common/http:exception_lib",
        "//source/common/http:header_map_lib",
        "//source/common/http:header_utility_lib",
        "//source/common/http:headers_lib",
        "//source/common/http:status_lib",
        "//source/common/http:utility_lib",
        "//source/common/runtime:runtime_features_lib",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
        ":pkg_cc_proto",
    ]
)

envoy_cc_library(
    name = "conn_pool_lib",
    srcs = ["conn_pool.cc"],
    hdrs = ["conn_pool.h"],
    deps = [
         "//envoy/event:dispatcher_interface",
        "//envoy/upstream:upstream_interface",
        "//source/common/http:codec_client_lib",
        "//source/common/http:conn_pool_base_lib",
    ],
)