# CanNodeSimulator

CanNodeSimulator is a C++20 CANopen node simulator that:
- Parses an EDS file into an in-memory object dictionary.
- Derives RPDO/TPDO definitions from standard CANopen PDO communication/mapping objects.
- Exposes Robot Framework Remote Library keywords.
- Supports Linux SocketCAN transport.

## Build

This repository uses git submodules for dependencies.

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
```

Notes:
- `yaml-cpp` and `RobotRemoteServerCpp` are consumed from `external/` submodules.
- `spdlog` is resolved with `find_package(spdlog)` when available, otherwise fetched via CMake `FetchContent`.

## Run

```bash
./build/CanNodeSimulator \
  --eds ./example/smoke.eds \
  --node-id 1 \
  --rf-port 8270 \
  --can-iface can0 \
  --log-level info \
  --log-file ./logs/can-node-sim.log \
  --config ./example/smoke_config.yaml
```

CLI options:
- `--eds` is required.
- `--node-id` is optional and defaults to `1`.
- `--can-iface` is optional. If omitted, CAN transport is disabled.
- `--rf-port` defaults to `8270`.
- `--log-level` supports `debug|info|warn|error`.
- `--log-file` is optional. If provided, logs are written to both console and file.
- `--config` is optional.

Runtime notes:
- If both `--node-id` and YAML `node_id` are provided, CLI `--node-id` wins.
- If the directory in `--log-file` does not exist, startup fails when the file sink is created.
- Robot remote keyword docs page is enabled at `http://127.0.0.1:<rf-port>/`.

### Logging

Logging uses `spdlog` and is configured in `main` during startup.

- Global log level is controlled by `--log-level`.
- If `--log-file <path>` is set, a file sink is enabled in append mode.
- Console output remains enabled when file logging is used.

Example (debug logs to console and file):

```bash
./build/CanNodeSimulator \
  --eds ./example/smoke.eds \
  --log-level debug \
  --log-file ./logs/simulator-debug.log
```

## Robot Smoke Example

`example/smoke.robot` provides a basic OD/PDO keyword flow.

Run simulator first:

```bash
./build/CanNodeSimulator --eds ./example/smoke.eds
```

Then run Robot smoke test:

```bash
robot example/smoke.robot
```

## YAML Config Semantics

Configuration is loaded from `--config <path.yaml>`.

### Schema

```yaml
node_id: <uint8>
od_defaults:
  "0xINDEX:subindex": <value-as-string-or-scalar>
pdo_overrides:
  rpdo:
    <pdo_number>:
      cob_id: <uint32>
      transmission_type: <uint8>
      inhibit_time: <uint16>
      event_timer: <uint16>
  tpdo:
    <pdo_number>:
      cob_id: <uint32>
      transmission_type: <uint8>
      inhibit_time: <uint16>
      event_timer: <uint16>
```

### `node_id`

- Optional.
- Used for EDS `$NODEID` expression evaluation.
- Precedence: CLI `--node-id` overrides YAML `node_id`.

### `od_defaults`

- Optional map of object key to override value.
- Key format must be canonical: `"0xXXXX:sub"`.
- Keys must exist in parsed EDS dictionary; unknown keys cause startup error.
- Value parsing uses the OD entry `DataType`:
- Integer types: parsed with base auto-detection (`0x..` hex supported).
- Boolean: accepts `1/0`, `true/false` (case-insensitive for provided forms).
- Real: parsed as floating-point.
- String/unknown: kept as string.

### `pdo_overrides`

- Optional map with `rpdo` and/or `tpdo` sections.
- Section keys are PDO numbers (1-based).
- Overrides are applied to communication objects:
- RPDO `n` -> object `0x1400 + (n - 1)`
- TPDO `n` -> object `0x1800 + (n - 1)`
- Sub-index mapping:
- `cob_id` -> sub `1`
- `transmission_type` -> sub `2`
- `inhibit_time` -> sub `3`
- `event_timer` -> sub `5`

If a target communication entry does not exist, the override creates/updates the entry in-memory before PDO build.

## Supported EDS Scope

Current parser coverage intentionally targets the simulator baseline:
- INI-style section/key/value parsing with comments.
- Object sections in forms like `[1000]` and `[1018sub1]`.
- Integer literals in decimal/hex.
- `$NODEID` expressions in simple forms:
- `$NODEID`
- `$NODEID + literal`
- `literal + $NODEID`

Out-of-scope forms currently fail parsing or are ignored:
- Complex arithmetic expressions in defaults.
- Full vendor-specific EDS extensions outside parsed keys.

## SocketCAN Behavior Boundaries

Implemented:
- RPDO decode to OD.
- TPDO encode and send on demand.
- SYNC-triggered TPDO sending for synchronous transmission types (`1..240`).
- Asynchronous TPDO periodic sending for transmission types (`254/255`) when `event_timer` is set.
- NMT command transmission and heartbeat transmission.
- EMCY frame send and receive logging.

Current limits:
- PDO signal packing/unpacking supports byte-aligned mappings only.
- Full CANopen state-machine fidelity is not implemented.

## Example Config

See `example/smoke_config.yaml`.

## Tests

```bash
ctest --test-dir build --output-on-failure
```
