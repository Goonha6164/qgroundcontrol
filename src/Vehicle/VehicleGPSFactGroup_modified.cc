/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleGPSFactGroup.h"
#include "Vehicle.h"
#include "QGCGeo.h"

////////////////////////////////////////////////////////////add

#include "UdpSender.h"
#include "UdpProto.h"

// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>

// int sock2;
// struct sockaddr_in server_address2;
// socklen_t server_sz2;
// float lpos2[3];

// void set_udp2(const char* ip, int port)
// {
//     sock2 = socket(PF_INET, SOCK_DGRAM,0);
//     if (sock2 == -1)
//     {
//         perror("Cannot create socket.");
//         exit(EXIT_FAILURE);
//     }
//     else
//         printf("socket open\n");

//     memset(&server_address2, 0, sizeof(server_address2));
//     server_address2.sin_family = AF_INET;
//     server_address2.sin_addr.s_addr = inet_addr(ip);
//     server_address2.sin_port = htons(port);
//     server_sz2 = sizeof(server_address2);
// }
////////////////////////////////////////////////////////////


const char* VehicleGPSFactGroup::_latFactName =                 "lat";
const char* VehicleGPSFactGroup::_lonFactName =                 "lon";
const char* VehicleGPSFactGroup::_mgrsFactName =                "mgrs";
const char* VehicleGPSFactGroup::_hdopFactName =                "hdop";
const char* VehicleGPSFactGroup::_vdopFactName =                "vdop";
const char* VehicleGPSFactGroup::_courseOverGroundFactName =    "courseOverGround";
const char* VehicleGPSFactGroup::_countFactName =               "count";
const char* VehicleGPSFactGroup::_lockFactName =                "lock";

VehicleGPSFactGroup::VehicleGPSFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/GPSFact.json", parent)
    , _latFact              (0, _latFactName,               FactMetaData::valueTypeDouble)
    , _lonFact              (0, _lonFactName,               FactMetaData::valueTypeDouble)
    , _mgrsFact             (0, _mgrsFactName,              FactMetaData::valueTypeString)
    , _hdopFact             (0, _hdopFactName,              FactMetaData::valueTypeDouble)
    , _vdopFact             (0, _vdopFactName,              FactMetaData::valueTypeDouble)
    , _courseOverGroundFact (0, _courseOverGroundFactName,  FactMetaData::valueTypeDouble)
    , _countFact            (0, _countFactName,             FactMetaData::valueTypeInt32)
    , _lockFact             (0, _lockFactName,              FactMetaData::valueTypeInt32)
{
    _addFact(&_latFact,                 _latFactName);
    _addFact(&_lonFact,                 _lonFactName);
    _addFact(&_mgrsFact,                _mgrsFactName);
    _addFact(&_hdopFact,                _hdopFactName);
    _addFact(&_vdopFact,                _vdopFactName);
    _addFact(&_courseOverGroundFact,    _courseOverGroundFactName);
    _addFact(&_lockFact,                _lockFactName);
    _addFact(&_countFact,               _countFactName);

    _latFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _lonFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _mgrsFact.setRawValue("");
    _hdopFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _vdopFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _courseOverGroundFact.setRawValue(std::numeric_limits<float>::quiet_NaN());

    /////////////////////////////////////////// add
    // set_udp2("127.0.0.1", 9190);
}

void VehicleGPSFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_GPS_RAW_INT:
        _handleGpsRawInt(message);
        break;
    case MAVLINK_MSG_ID_HIGH_LATENCY:
        _handleHighLatency(message);
        break;
    case MAVLINK_MSG_ID_HIGH_LATENCY2:
        _handleHighLatency2(message);
        break;
    default:
        break;
    }
}

void VehicleGPSFactGroup::_handleGpsRawInt(mavlink_message_t& message)
{
    mavlink_gps_raw_int_t gpsRawInt;
    mavlink_msg_gps_raw_int_decode(&message, &gpsRawInt);

    lat()->setRawValue              (gpsRawInt.lat * 1e-7);
    lon()->setRawValue              (gpsRawInt.lon * 1e-7);
    mgrs()->setRawValue             (convertGeoToMGRS(QGeoCoordinate(gpsRawInt.lat * 1e-7, gpsRawInt.lon * 1e-7)));
    count()->setRawValue            (gpsRawInt.satellites_visible == 255 ? 0 : gpsRawInt.satellites_visible);
    hdop()->setRawValue             (gpsRawInt.eph == UINT16_MAX ? qQNaN() : gpsRawInt.eph / 100.0);
    vdop()->setRawValue             (gpsRawInt.epv == UINT16_MAX ? qQNaN() : gpsRawInt.epv / 100.0);
    courseOverGround()->setRawValue (gpsRawInt.cog == UINT16_MAX ? qQNaN() : gpsRawInt.cog / 100.0);
    lock()->setRawValue             (gpsRawInt.fix_type);


    ///////////////////////////////////////////////////////////
    globalpos p;
    p.time = (uint64_t)gpsRawInt.time_usec;
    p.x = (float)gpsRawInt.lat * 1e-7;
    p.y = (float)gpsRawInt.lon * 1e-7;
    p.z = (float)gpsRawInt.satellites_visible;

    UdpSender::instance().sendtype(GLOBAL_POS, &p, sizeof(p));
    
    // lpos2[1] = gpsRawInt.lat * 1e-7;
    // lpos2[2] = gpsRawInt.lon * 1e-7;
    // lpos2[0] = 1;
    // /////////////////////////////////////// add
    // sendto(sock2, lpos2, sizeof(lpos2), 0, (const struct sockaddr *)&server_address2, server_sz2);   
}

void VehicleGPSFactGroup::_handleHighLatency(mavlink_message_t& message)
{
    mavlink_high_latency_t highLatency;
    mavlink_msg_high_latency_decode(&message, &highLatency);

    struct {
        const double latitude;
        const double longitude;
        const double altitude;
    } coordinate {
        highLatency.latitude  / (double)1E7,
                highLatency.longitude  / (double)1E7,
                static_cast<double>(highLatency.altitude_amsl)
    };

    lat()->setRawValue  (coordinate.latitude);
    lon()->setRawValue  (coordinate.longitude);
    mgrs()->setRawValue (convertGeoToMGRS(QGeoCoordinate(coordinate.latitude, coordinate.longitude)));
    count()->setRawValue(0);
}

void VehicleGPSFactGroup::_handleHighLatency2(mavlink_message_t& message)
{
    mavlink_high_latency2_t highLatency2;
    mavlink_msg_high_latency2_decode(&message, &highLatency2);

    lat()->setRawValue  (highLatency2.latitude * 1e-7);
    lon()->setRawValue  (highLatency2.longitude * 1e-7);
    mgrs()->setRawValue (convertGeoToMGRS(QGeoCoordinate(highLatency2.latitude * 1e-7, highLatency2.longitude * 1e-7)));
    count()->setRawValue(0);
    hdop()->setRawValue (highLatency2.eph == UINT8_MAX ? qQNaN() : highLatency2.eph / 10.0);
    vdop()->setRawValue (highLatency2.epv == UINT8_MAX ? qQNaN() : highLatency2.epv / 10.0);
}
