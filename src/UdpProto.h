#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct UdpHeader {
    uint16_t type;
    uint16_t length;
    uint64_t t_us;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct localpos {/*uint32_t time;*/ float x, y, z, vx, vy, vz;};
struct att {/*uint32_t time;*/ float roll, pitch, yaw, rollrate, pitchrate, yawrate;};
struct gimbalinfo {/*uint32_t time;*/ float roll, pitch, yaw, abyaw;};//, rollrate, pitchrate, yawrate;};
struct globalpos {/*uint32_t time;*/ double lat, lon, alt, rel_alt; float vx, vy, vz;};
#pragma pack(pop)

enum MsgType : uint16_t {
    LOCAL_POS = 1,
    ATTITUDE = 2,
    GIMBAL_INFO = 3,
    GLOBAL_POS = 4,
};

#pragma pack(push, 1)
struct Payload {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;

    float roll;
    float pitch;
    float yaw;
    float rollrate;
    float pitchrate;
    float yawrate;

    float gimbal_roll;
    float gimbal_pitch;
    float gimbal_yaw;
    float gimbal_abyaw;

    double lat;
    double lon;
    double alt;
    double rel_alt;
    float gvx;
    float gvy;
    float gvz;
};
#pragma pack(pop)
