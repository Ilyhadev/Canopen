# libcanopen

Small, bounded CANopen building blocks for embedded applications. The core is
hardware-independent and uses an injected transport API. The first release
supports NMT Start and passive TPDO observation; SDO and object dictionaries are
intentionally out of scope.

The application supplies the transport callbacks. MCU drivers and physical CAN
controller selection remain application-owned.
