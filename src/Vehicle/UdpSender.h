#pragma once
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#ifdef _WIN32
using SocketHandle = SOCKET;
static const SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
static const SocketHandle kInvalidSocket = -1;
#endif

class UdpSender {
    public:
    static UdpSender& instance();

    bool init(const char* ip, uint16_t port);

    bool sendtype(uint16_t type, const void* payload, uint16_t len);

    void shutdown();


    private:
    UdpSender() = default;

    uint64_t now_us() const;
    SocketHandle sock{kInvalidSocket};
    sockaddr_in dst{};
    bool inited{false};

};
