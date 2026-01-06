/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleLocalPositionFactGroup.h"
#include "Vehicle.h"

#include <QtMath>

////////////////////////////////////////////////////////////add
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>

// int sock;
// struct sockaddr_in server_address;
// socklen_t server_sz;
// float lpos[4];

// void set_udp(const char* ip, int port)
// {
//     sock = socket(PF_INET, SOCK_DGRAM,0);
//     if (sock == -1)
//     {
//         perror("Cannot create socket.");
//         exit(EXIT_FAILURE);
//     }
//     else
//         printf("socket open\n");

//     memset(&server_address, 0, sizeof(server_address));
//     server_address.sin_family = AF_INET;
//     server_address.sin_addr.s_addr = inet_addr(ip);
//     server_address.sin_port = htons(port);
//     server_sz = sizeof(server_address);
// }
#include "UdpSender.h"
#include "UdpProto.h"


////////////////////////////////////////////////////////////


const char* VehicleLocalPositionFactGroup::_xFactName =     "x";
const char* VehicleLocalPositionFactGroup::_yFactName =     "y";
const char* VehicleLocalPositionFactGroup::_zFactName =     "z";
const char* VehicleLocalPositionFactGroup::_vxFactName =    "vx";
const char* VehicleLocalPositionFactGroup::_vyFactName =    "vy";
const char* VehicleLocalPositionFactGroup::_vzFactName =    "vz";

VehicleLocalPositionFactGroup::VehicleLocalPositionFactGroup(QObject* parent)
    : FactGroup     (1000, ":/json/Vehicle/LocalPositionFact.json", parent)
    , _xFact    (0, _xFactName,     FactMetaData::valueTypeDouble)
    , _yFact    (0, _yFactName,     FactMetaData::valueTypeDouble)
    , _zFact    (0, _zFactName,     FactMetaData::valueTypeDouble)
    , _vxFact   (0, _vxFactName,    FactMetaData::valueTypeDouble)
    , _vyFact   (0, _vyFactName,    FactMetaData::valueTypeDouble)
    , _vzFact   (0, _vzFactName,    FactMetaData::valueTypeDouble)
{
    _addFact(&_xFact,      _xFactName);
    _addFact(&_yFact,      _yFactName);
    _addFact(&_zFact,      _zFactName);
    _addFact(&_vxFact,     _vxFactName);
    _addFact(&_vyFact,     _vyFactName);
    _addFact(&_vzFact,     _vzFactName);

    // Start out as not available "--.--"
    _xFact.setRawValue(qQNaN());
    _yFact.setRawValue(qQNaN());
    _zFact.setRawValue(qQNaN());
    _vxFact.setRawValue(qQNaN());
    _vyFact.setRawValue(qQNaN());
    _vzFact.setRawValue(qQNaN());
    
    /////////////////////////////////////////// add
    // set_udp("127.0.0.1", 9190);
    
}

void VehicleLocalPositionFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    if (message.msgid != MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
        return;
    }

    mavlink_local_position_ned_t localPosition;
    mavlink_msg_local_position_ned_decode(&message, &localPosition);

    x()->setRawValue(localPosition.x);
    y()->setRawValue(localPosition.y);
    z()->setRawValue(localPosition.z);

    vx()->setRawValue(localPosition.vx);
    vy()->setRawValue(localPosition.vy);
    vz()->setRawValue(localPosition.vz);

    _setTelemetryAvailable(true);

    /////////////////////////////////////////////////////
    localpos p;
    //p.time = (uint64_t)localPosition.time_boot_ms;
    p.x = (float)localPosition.x;
    p.y = (float)localPosition.y;
    p.z = (float)localPosition.z;
    p.vx = (float)localPosition.vx;
    p.vy = (float)localPosition.vy;
    p.vz = (float)localPosition.vz;

    UdpSender::instance().sendtype(LOCAL_POS, &p, sizeof(p));
    
    // lpos[0] = 0;
    // lpos[1] = (float)localPosition.x;
    // lpos[2] = (float)localPosition.y;
    // lpos[3] = (float)localPosition.z;
    /////////////////////////////////////// add
    // sendto(sock, lpos, sizeof(lpos), 0, (const struct sockaddr *)&server_address, server_sz);    
}

