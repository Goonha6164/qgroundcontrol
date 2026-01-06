#pragma once
#include <cstdint>
#include <cstddef>
#include <netinet/in.h>

class UdpSender {
    public:
    static UdpSender& instance();

    bool init(const char* ip, uint16_t port);

    bool sendtype(uint16_t type, const void* payload, uint16_t len);

    void shutdown();


    private:
    UdpSender() = default;

    uint64_t now_us() const;
    int sock{-1};
    sockaddr_in dst{};
    bool inited{false};

};