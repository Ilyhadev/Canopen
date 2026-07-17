# libcanopen

Small, bounded CANopen building blocks for embedded applications. The core is
hardware-independent and uses an injected transport API without heap allocation.

Implemented services:

- NMT Start, Stop, Enter Pre-operational, and Reset Node;
- passive TPDO observation;
- LSS global and selective state switching;
- LSS node-ID configuration and persistent configuration storage.

The LSS manager uses standard Classical CAN frames with COB-IDs `0x7E5` and
`0x7E4`. Request/response functions block and busy-poll the injected receive
function for up to 500 ms by default. They consume unrelated received frames, so
they are intended for an explicit configuration flow, not concurrent operational
traffic.

LSS bit-timing configuration and activation deliberately return `UNSUPPORTED`
without transmitting. They will remain disabled until a platform transport can
coordinate changing both the remote CANopen server and its local CAN controller.

SDO, object dictionaries, heartbeat, EMCY, LSS Fastscan, and LSS inquiry services
are currently out of scope.

PIHER protocol reference:
<https://www.piher.net/wp-content/uploads/Piher_CANOpen_PROTOCOL_V0.pdf>
