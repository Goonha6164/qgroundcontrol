#include "UdpProto.h"
#include "UdpSender.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <time.h>
#include <cstdio>

UdpSender& UdpSender::instance() {
    static UdpSender inst;
    return inst;
}

bool UdpSender::init(const char* ip, uint16_t port)
{
    if (inited) return true;

    sock = ::socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = PF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = inet_addr(ip);
    if(::inet_pton(PF_INET, ip, &dst.sin_addr) != 1) {
        ::close(sock);
        sock = -1;
        return false;
    }

    inited = true;
    printf("Server Successfully initialized!!\n");
    return true;
}

uint64_t UdpSender::now_us() const {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000000ULL + (uint64_t)ts.tv_nsec/1000ULL;
}

bool UdpSender::sendtype(uint16_t type, const void* payload, uint16_t len) {
    if (len > 1000) return false;

    if (!inited || (sock < 0))  {
        this->init("127.0.0.1", 9190);
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
    ssize_t n = ::sendto(sock, buf, total, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    //printf("Send Bytes : %ld, total Bytes : %ld \n", n, total);

    return n==(ssize_t)total;
}

void UdpSender::shutdown() {
    if (sock >= 0) {
        ::close(sock);
        sock = -1;
    }
    printf("Server Closed!!\n");
    inited = false;
}