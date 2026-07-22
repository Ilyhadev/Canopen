# Roadmap

Each milestone is complete only when it includes:

- automated smoke tests;
- hardware tests with real CANopen devices;
- sample code demonstrating the new API in a module.

## 1. Non-blocking LSS

1. Convert the tested global and selective switching, node-ID configuration,
   and configuration storage operations to non-blocking transactions.
2. Let the caller provide the timeout and advance each transaction from its
   normal processing loop.
3. Add LSS identity and node-ID inquiry services.
4. Add Fastscan after the normal identity operations have been verified on
   hardware with more than one device.
5. Keep bit-timing configuration unsupported until the local CAN controller and
   remote device can change bitrate safely together.

## 2. NMT and heartbeat

1. Retain and test the existing NMT Start, Stop, Pre-operational, and Reset
   commands for individual nodes and broadcast node ID `0`.
2. Detect CANopen boot-up messages and expose the last known NMT state of each
   monitored node.
3. Receive heartbeat messages and update their timestamps without blocking.
4. Add caller-configured heartbeat timeouts and report state changes, timeouts,
   recovery, and remote resets.
5. Verify the complete boot-up, configuration, operational, reset, and recovery
   sequence on real devices.

## 3. SDO client

1. Parse SDO responses and expose the full SDO abort code.
2. Implement non-blocking expedited upload for values up to four bytes.
3. Implement non-blocking expedited download for values up to four bytes.
4. Add cancellation, caller-controlled timeouts, malformed-response handling,
   and transaction state reporting.
5. Add segmented upload and download with fixed-capacity buffers.
6. Add block transfer only when a supported device or firmware-update use case
   requires it.

## 4. PDO handling

1. Retain and extend passive TPDO reception by caller-supplied COB-ID and DLC.
2. Validate malformed and stale PDO data and expose reception timestamps.
3. Test multiple devices and multiple PDOs sharing one CAN bus.
4. Add RPDO transmission only when the bridge needs to command a CANopen device.
5. Add SYNC-controlled PDO behavior only for devices that require it.

## 5. Emergency handling

Receive EMCY frames, preserve their error code, error register, and
manufacturer-specific data, and expose device fault and recovery state.

## 6. Unified CANopen processing

After the individual protocol behavior is tested, unite LSS, SDO, PDO, NMT,
heartbeat, and EMCY under the main CANopen class. Ensure that received frames are
processed once and are not discarded by a competing protocol operation. Avoid
introducing additional public abstractions until concrete usage requires them.

## 7. FDCAN interrupt integration

In the longer term, add a platform-specific FDCAN adapter with interrupt-driven
RX and TX. Keep a fixed-capacity internal queue, perform only bounded work in the
interrupt handlers, and process protocol state through flags from normal
application context. Include queue overflow and CAN error diagnostics.
