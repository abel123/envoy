#!/usr/bin/sh
bazel build -c dbg  --apple_generate_dsym  --spawn_strategy=standalone //source/exe:envoy-static
