// - Thread A: UDP recv(non-blocking) -> translate -> UDP send(non-blocking)
// - Thread B: GStreamer relay (udpsrc -> depay/parse/pay -> multiudpsink)
// - extern "C": Start/Stop

#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>
#else
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "UdpProto.h"

// GStreamer
#if defined(QGC_GST_STREAMING)
#include <gst/gst.h>
#include <glib.h>
#endif

// ======================== 설정 ========================
// (A) UDP 변환/포워딩
static constexpr const char* UDP_IP   = "127.0.0.1";
static constexpr uint16_t    UDP_IN_PORT   = 9190;
static constexpr uint16_t    UDP_OUT_PORT  = 9192;

// (B) 릴레이
static constexpr uint16_t IN_PORT = 5700;
static constexpr uint16_t QGC_PORT = 5600;
static constexpr uint16_t EXT_PORT = 5800;
//static constexpr const char* RTP_ENCODING = "H264";
// ====================================================================

// ======================== 상태/스레드 ========================
static std::atomic_bool g_running{false};
static std::atomic_bool g_stop{false};
static std::thread g_udpThread;
static std::thread g_rtpThread;

// ======================== non-blocking ========================
using SocketHandle =
#ifdef _WIN32
    SOCKET;
static const SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
    int;
static const SocketHandle kInvalidSocket = -1;
#endif

#ifdef _WIN32
typedef SSIZE_T ssize_t;
#endif

static bool set_nonblocking(SocketHandle fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ::ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

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
// static std::atomic_int sock{-1};
// static std::atomic_int ssck{-1};
// ======================== Thread A: UDP forward ========================
static void udp_thread_main() {
#ifdef _WIN32
    if (!socket_init()) {
        printf("WSAStartup failed.");
        return;
    }
#endif
    ////////////////////////////////////////////
    // receiver socket ////////////////
    ////////////////////////////////////////////
    struct sockaddr_in serv, clnt;
    socklen_t clnt_sz = sizeof(clnt);

    SocketHandle sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == kInvalidSocket) {
        printf("Cannot create receive socket.");
#ifdef _WIN32
        socket_cleanup();
#endif
        return;
    }
        
    int bufsize = 1024*4;
    #ifdef _WIN32
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
                reinterpret_cast<const char*>(&bufsize),
                sizeof(bufsize));
    #else
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    #endif

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = PF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(UDP_IN_PORT);
    
    //printf("Server start.\n");
    
    if(::bind(sock, (sockaddr*)&serv, sizeof(serv)) != 0) {
        printf("binding failed.\n");
        close_socket(sock);
#ifdef _WIN32
        socket_cleanup();
#endif
        return;
    }
    
    if(!set_nonblocking(sock)) {
        printf("socket setting failed.\n");
        close_socket(sock);
#ifdef _WIN32
        socket_cleanup();
#endif
        return;
    }
    std::cout << "FROM UDP Pipeline Connected!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "listening 127.0.0.1:" << UDP_IN_PORT << std::endl;

    ////////////////////////////////////////////
    // Transmitter socket ////////////////
    ////////////////////////////////////////////
    
    uint8_t buf[1024];
    Payload pay{};

    int lp_count = 0;
    int att_count = 0;
    int gim_count = 0;
    int gp_count = 0;

    SocketHandle ssck = socket(PF_INET, SOCK_DGRAM, 0);
    if (ssck == kInvalidSocket) {
        printf("Cannot create transport socket.");
#ifdef _WIN32
        close_socket(sock);
        socket_cleanup();
#endif
        return;
    }

    sockaddr_in dst{};
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = PF_INET;
    dst.sin_port = htons(UDP_OUT_PORT);
    dst.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(::inet_pton(PF_INET, "127.0.0.1", &dst.sin_addr) != 1) {
    // dst.sin_addr.s_addr = inet_addr("192.168.50.102");
    // if(::inet_pton(PF_INET, "192.168.50.102", &dst.sin_addr) != 1) {
        close_socket(sock);
        close_socket(ssck);
#ifdef _WIN32
        socket_cleanup();
#endif
        ssck = kInvalidSocket;
        printf("transport socket link failed.");
        return;
    }
    std::cout << "TO UDP Pipeline Created----------------------" << std::endl;
    std::cout << "Talking to 127.0.0.1:" << UDP_OUT_PORT << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (1) {
        if (g_stop.load(std::memory_order_relaxed)) break;
        
        while (!g_stop.load(std::memory_order_relaxed))
        {
            ssize_t n = ::recvfrom(sock, reinterpret_cast<char*>(buf), sizeof(buf), 0, (sockaddr*)&clnt, &clnt_sz);
           
            if (n < (ssize_t)sizeof(UdpHeader)) continue;
            //printf("Received bytes : %ld \n", n);
            UdpHeader h{};
            memcpy(&h, buf, sizeof(h));
            
            if ((ssize_t)(sizeof(UdpHeader) + h.length) > n) continue;
            //printf("Received bytes : %ld \n", n);

            switch (h.type) {
                case LOCAL_POS : {
                    localpos lp{};
                    memcpy(&lp, buf+sizeof(h), sizeof(lp));
                    //pay.lts = h.t_us;
                    pay.x = lp.x;
                    pay.y = lp.y;
                    pay.z = lp.z;
                    pay.vx = lp.vx;
                    pay.vy = lp.vy;
                    pay.vz = lp.vz;
                    lp_count = 1;
                    //printf("local x : %.2f, y : %.2f, z : %.2f, vx : %.2f, vy : %.2f, vz : %.2f \n",pay.x, pay.y, pay.z, pay.vx, pay.vy, pay.vz);
                    break;
                }

                case ATTITUDE : {
                    att attitude{};
                    memcpy(&attitude, buf+sizeof(h), sizeof(attitude));
                    //pay.lts = h.t_us;
                    pay.roll = attitude.roll;
                    pay.pitch = attitude.pitch;
                    pay.yaw = attitude.yaw;
                    pay.rollrate = attitude.rollrate;
                    pay.pitchrate = attitude.pitchrate;
                    pay.yawrate = attitude.yawrate;
                    att_count = 1;
                    // printf("vehicle roll : %.2f, pitch : %.2f, yaw : %.2f, rr : %.2f, pr : %.2f, yr : %.2f \n",pay.roll, pay.pitch, pay.yaw
                    //    , pay.rollrate, pay.pitchrate, pay.yawrate);
                    break;
                }

                case GIMBAL_INFO : {
                    gimbalinfo gimbal{};
                    memcpy(&gimbal, buf+sizeof(h), sizeof(gimbal));
                    //pay.lts = h.t_us;
                    pay.gimbal_roll = gimbal.roll;
                    pay.gimbal_pitch = gimbal.pitch;
                    pay.gimbal_yaw = gimbal.yaw;
                    pay.gimbal_abyaw = gimbal.abyaw;
                    gim_count = 1;
                    //printf("gimbal roll : %.2f, pitch : %.2f, yaw : %.2f, absolute yaw : %.2f \n",pay.gimbal_roll, pay.gimbal_pitch, pay.gimbal_yaw, pay.gimbal_abyaw);
                    break;
                }

                case GLOBAL_POS : {
                    globalpos gp{};
                    memcpy(&gp, buf+sizeof(h),sizeof(gp));
                    pay.lat = gp.lat;
                    pay.lon = gp.lon;
                    pay.alt = gp.alt;
                    pay.rel_alt = gp.rel_alt;
                    pay.gvx = gp.vx;
                    pay.gvy = gp.vy;
                    pay.gvz = gp.vz;
                    gp_count = 1;
                    //pay.gts = h.t_us;
                    //printf("global lat : %.2lf, lon : %.2lf, alt : %.2lf, vx : %.2f, vy : %.2f, vz : %.2f \n", pay.lat, pay.lon, pay.rel_alt, pay.x, pay.y, pay.z);
                    break;
                }
            }
            //if (att_count == 1 || lp_count == 1 && att_count == 1 && gim_count == 1 && gp_count == 1) {
            if (lp_count == 1 && att_count == 1 && gim_count == 1 && gp_count == 1) {
                ssize_t n = ::sendto(ssck, reinterpret_cast<const char*>(&pay), sizeof(pay), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
                lp_count = 0;
                att_count = 0;
                gim_count = 0;
                gp_count = 0;
                //if (n == (ssize_t)sizeof(Payload)) printf("Sending Success !!!! \n %ld Bytes Sended.\n", n);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    printf("Shut down.");
    close_socket(sock);
    close_socket(ssck);
#ifdef _WIN32
    socket_cleanup();
#endif

    printf("Close socket.");
    return;
}

// ======================== Thread B: relay GStreamer ========================
#if defined(QGC_GST_STREAMING)
struct GstCtx {
    GMainLoop* loop = nullptr;
    GstElement* pipeline = nullptr;
};

static gboolean gst_bus_cb(GstBus*, GstMessage* msg, gpointer data) {
    auto* ctx = (GstCtx*)data;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            std::fprintf(stderr, "[GstError] %s\n", err ? err->message : "unknown");
            if (debug) std::fprintf(stderr, "[GstDebug] %s\n", debug);
            if (err) g_error_free(err);
            if (debug) g_free(debug);
            if (ctx->loop) g_main_loop_quit(ctx->loop);
            break;
        }
        case GST_MESSAGE_EOS:
            if (ctx->loop) g_main_loop_quit(ctx->loop);
            break;
        default:
            break;
    }
    return TRUE;
}

static void rtp_thread_main() {

    gst_init(nullptr, nullptr);

    // std::string depay = (std::string(RTP_ENCODING) == "H265") ? "rtph265depay" : "rtph264depay";
    // std::string parse = (std::string(RTP_ENCODING) == "H265") ? "h265parse" : "h264parse";
    // std::string pay   = (std::string(RTP_ENCODING) == "H265") ? "rtph265pay" : "rtph264pay";

    // Video Stream Pipeline
    std::string pipelineStr =
        "udpsrc port=" + std::to_string(IN_PORT) + " caps=\"" +
        "application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000" +
        "\" ! " +
        "rtph264depay ! h264parse ! " +
        "rtph264pay pt=96 config-interval=1 ! " +
        "multiudpsink clients=\"127.0.0.1:" + std::to_string(QGC_PORT) +
        ",127.0.0.1:" + std::to_string(EXT_PORT) +
        "\" sync=false async=false";

    std::cout << "Gstreamer Stream Pipeline Created----------------------" << std::endl;
    std::cout << "from 127.0.0.1:" << IN_PORT << std::endl;

    GstCtx ctx{};
    ctx.loop = g_main_loop_new(nullptr, FALSE);
    // std::cout << loop_ << std::endl;

    GError* err = nullptr;
    ctx.pipeline = gst_parse_launch(pipelineStr.c_str(), &err);
    // std::cout << pipelineStr_.c_str() << std::endl;
    // std::cout << pipeline << std::endl;
    if (!ctx.pipeline) {
        if (err) { std::cerr << "parse error: " << err->message << "\n"; g_error_free(err); }
        g_main_loop_unref(ctx.loop);
        return;
    }
    if (err) { std::cerr << "pipeline warning: " << err->message << "\n"; g_error_free(err); }

    GstBus* bus = gst_element_get_bus(ctx.pipeline);
    gst_bus_add_watch(bus, gst_bus_cb, &ctx);
    // std::cout << busWatchId_ << std::endl;
    gst_object_unref(bus);

    gst_element_set_state(ctx.pipeline, GST_STATE_PLAYING);
    
    std::thread stop([&ctx](){
        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (ctx.loop) g_main_loop_quit(ctx.loop);

    });
    
    g_main_loop_run(ctx.loop);

    gst_element_set_state(ctx.pipeline, GST_STATE_NULL);
    gst_object_unref(ctx.pipeline);
    g_main_loop_unref(ctx.loop);

    if (stop.joinable()) stop.join();
}
#endif

extern "C" bool relay_start() {
    if (g_running.exchange(true)) return true;

    g_stop.store(false);

    g_udpThread = std::thread(udp_thread_main);
#if defined(QGC_GST_STREAMING)
    g_rtpThread = std::thread(rtp_thread_main);
#endif

    return true;
}

extern "C" void relay_stop() {
    if (!g_running.load()) return;

    g_stop.store(true);
    // close(sock);
    // close(ssck);
    // std::cout << "dd----------------------3" << sock << "--" << ssck << std::endl;

    if (g_udpThread.joinable()) g_udpThread.join();
    std::cout << "Close UDP socket" << std::endl;
#if defined(QGC_GST_STREAMING)
    if (g_rtpThread.joinable()) g_rtpThread.join();
    std::cout << "Close RTP streaming" << std::endl;
#endif

    g_running.store(false);
    
    std::cout << "Quit stanby" << std::endl;
    return;
}
