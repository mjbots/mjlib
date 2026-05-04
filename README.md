# mjlib

[![CI Status](https://github.com/mjbots/mjlib/actions/workflows/ci.yml/badge.svg)](https://github.com/mjbots/mjlib/actions/workflows/ci.yml)

mjlib is a C++20 robotics library by mjbots. It provides foundational
utilities, async I/O built on Boost.Asio, the multiplex communication
protocol, binary telemetry/data recording, and microcontroller
abstractions targeting STM32G4.

## Modules

- `mjlib/base/` — error handling, streams, serialization archives,
  algorithms (PID, CRC, windowed average).
- `mjlib/io/` — async I/O: stream abstractions, stream factory
  (serial/TCP/stdio), timers, realtime executor.
- `mjlib/micro/` — embedded layer: static allocation, async
  primitives without `std::future`, command manager, persistent
  config over flash.
- `mjlib/multiplex/` — frame-based protocol over RS-485 / SocketCAN /
  FDCAN-USB, register-based RPC, client/server implementations,
  Python bindings.
- `mjlib/telemetry/` — binary data recording / playback with snappy
  compression, schema system, JSON export, Python reader.
- `mjlib/imgui/` — ImGui integration for visualization.

## Building

A vendored bazel binary is at `./tools/bazel`.

```bash
# Run all host tests (C++ and Python).
./tools/bazel test --config=host //:host

# Cross-compile for STM32G4.
./tools/bazel build --config=target //:target
```

## License

Apache 2.0. See [LICENSE](LICENSE).
