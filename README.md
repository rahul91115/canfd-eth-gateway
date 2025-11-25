# canfd-eth-gateway

A lightweight, real-time C++ gateway that bridges CAN-FD (Controller Area Network with Flexible Data-rate) and Ethernet — ideal for automotive diagnostics, telematics, and embedded IoT.

- C++17, no external dependencies
- Uses Linux SocketCAN with CAN-FD support
- Binary serialization (~80 bytes/packet)
- Monotonic timestamps (ns resolution)
- Real-time scheduling (`SCHED_FIFO`)
- systemd service ready
- Yocto-compatible
