#include "UdpProto.h"
#include "UdpSender.h"

#include <cstring>
#include <chrono>
#include <cstdio>

#ifdef _WIN32
#include <BaseTsd.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef _WIN32
typedef SSIZE_T ssize_t;
#endif

static void close_socket(SocketHandle fd) {
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

static bool socket_init() {
#ifdef _WIN32
    WSADATA wsaData{};
    return ::WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

static void socket_cleanup() {
#ifdef _WIN32
    ::WSACleanup();
#endif
}

UdpSender& UdpSender::instance() {
    static UdpSender inst;
    return inst;
}

bool UdpSender::init(const char* ip, uint16_t port)
{
    if (inited) return true;

    if (!socket_init()) return false;

    sock = ::socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == kInvalidSocket) {
#ifdef _WIN32
        socket_cleanup();
#endif
        return false;
    }

    std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = PF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = inet_addr(ip);
    if(::inet_pton(PF_INET, ip, &dst.sin_addr) != 1) {
        close_socket(sock);
        sock = kInvalidSocket;
#ifdef _WIN32
        socket_cleanup();
#endif
        return false;
    }

    inited = true;
    printf("Server Successfully initialized!!\n");
    return true;
}

uint64_t UdpSender::now_us() const {
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

bool UdpSender::sendtype(uint16_t type, const void* payload, uint16_t len) {
    if (len > 1000) return false;

    if (!inited || (sock == kInvalidSocket))  {
        if (!this->init("127.0.0.1", 9190)) return false;
    }

    uint8_t buf[sizeof(UdpHeader) + 1000];

    UdpHeader h{};
    h.type = type;
    h.length = len;
    h.t_us = now_us();

    std::memcpy(buf, &h, sizeof(h));
    if (len > 0 && payload) {
        std::memcpy(buf + sizeof(h), payload, len);
    }

    const size_t total = sizeof(h) + len;
    ssize_t n = ::sendto(sock, reinterpret_cast<const char*>(buf), total, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    //printf("Send Bytes : %ld, total Bytes : %ld \n", n, total);

    return n==(ssize_t)total;
}

void UdpSender::shutdown() {
    if (sock != kInvalidSocket) {
        close_socket(sock);
        sock = kInvalidSocket;
    }
#ifdef _WIN32
    socket_cleanup();
#endif
    printf("Server Closed!!\n");
    inited = false;
}
