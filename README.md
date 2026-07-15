# libcanopen

Small, bounded CANopen building blocks for embedded applications. The core is
hardware-independent and uses an injected transport API. The first release
supports NMT Start and passive TPDO observation; SDO and object dictionaries are
intentionally out of scope.

The STM32 FDCAN adapter requires the application to inject the physical HAL
handles before initialization. It never declares or selects board handles.
