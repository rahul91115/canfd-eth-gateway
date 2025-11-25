// CAN-FD to Ethernet Gateway
// Author: Rahul Velimineti
// License: MIT

#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/mman.h>

// Compact binary packet format
#pragma pack(push, 1)
struct GatewayPacket {
    uint64_t timestamp_ns;   // CLOCK_MONOTONIC
    uint32_t can_id;         // includes CAN_EFF_FLAG, CAN_RTR_FLAG
    uint8_t  dlc;            // actual data length (1..64)
    uint8_t  flags;          // Bit 0: BRS, Bit 1: ESI
    uint8_t  data[64];       // zero-padded
};
#pragma pack(pop)

static constexpr size_t PACKET_SIZE = sizeof(GatewayPacket);

// Open CAN socket (SocketCAN, FD-enabled)
int open_can_socket(const char* ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW_FD_FRAMES);
    if (s < 0) {
        perror("Failed to create CAN socket");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(s);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind CAN socket");
        close(s);
        exit(EXIT_FAILURE);
    }

    printf("CAN socket opened on %s\n", ifname);
    return s;
}

// Open UDP socket
int open_udp_socket(const char* ip, int port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("UDP socket");
        exit(EXIT_FAILURE);
    }

    // Increase send buffer (helps under burst traffic)
    int sndbuf = 1 << 20; // 1 MB
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    printf("UDP socket opened to %s:%d\n", ip, port);
    return s;
}

// Set real-time priority (requires root)
void set_realtime_priority() {
    struct sched_param param = {};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        fprintf(stderr, "WARNING: Failed to set SCHED_FIFO (run as root for best latency)\n");
    } else {
        printf("Real-time priority (SCHED_FIFO) set successfully\n");
    }
}

// Main gateway loop
void run_gateway(int can_sock, int udp_sock, const struct sockaddr_in& dest) {
    struct canfd_frame frame;
    GatewayPacket pkt;

    printf("Starting gateway loop...\n");
    while (true) {
        // Read CAN-FD frame
        ssize_t nbytes = read(can_sock, &frame, sizeof(frame));
        if (nbytes != sizeof(frame)) {
            if (nbytes < 0 && errno != EINTR) {
                perror("read CAN");
            }
            continue;
        }

        // Timestamp (monotonic)
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        pkt.timestamp_ns = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;

        // Copy metadata
        pkt.can_id = frame.can_id;
        pkt.dlc = frame.len;
        pkt.flags = 0;
        if (frame.flags & CANFD_BRS) pkt.flags |= 0x01;
        if (frame.flags & CANFD_ESI) pkt.flags |= 0x02;

        // Copy data (safe, zero-padded)
        std::memset(pkt.data, 0, 64);
        if (frame.len > 0) {
            std::memcpy(pkt.data, frame.data, frame.len);
        }

        // Send UDP
        if (sendto(udp_sock, &pkt, PACKET_SIZE, 0,
                   reinterpret_cast<const struct sockaddr*>(&dest),
                   sizeof(dest)) < 0) {
            perror("sendto UDP");
        }
    }
}

// Parse IP:port (e.g., "192.168.1.100:5000")
bool parse_address(const char* addr_str, struct sockaddr_in& addr) {
    char ip[64];
    int port;
    if (sscanf(addr_str, "%63[^:]:%d", ip, &port) != 2) {
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    return inet_pton(AF_INET, ip, &addr.sin_addr) == 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <can_interface> <dest_ip:port>\n", argv[0]);
        fprintf(stderr, "Example: %s can0 192.168.1.100:5000\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* can_if = argv[1];
    const char* dest_str = argv[2];

    struct sockaddr_in dest_addr = {};
    if (!parse_address(dest_str, dest_addr)) {
        fprintf(stderr, "Invalid destination: %s (expected IP:port)\n", dest_str);
        return EXIT_FAILURE;
    }

    // Optional: Lock memory to avoid page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        fprintf(stderr, "WARNING: mlockall failed (run as root for best real-time behavior)\n");
    }

    set_realtime_priority();

    int can_sock = open_can_socket(can_if);
    int udp_sock = open_udp_socket(inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));

    run_gateway(can_sock, udp_sock, dest_addr);

    close(can_sock);
    close(udp_sock);
    return EXIT_SUCCESS;
}
