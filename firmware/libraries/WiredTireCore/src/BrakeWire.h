#pragma once
#include "BrakeTypes.h"
#include <cstddef>
namespace wired_brake {
constexpr size_t kBrakeWireBytes=256;
bool encodeBrakeRequest(const BrakeCommand&,uint32_t,uint8_t*,size_t);
bool decodeBrakeRequest(const uint8_t*,size_t,BrakeCommand&,uint32_t&);
bool encodeBrakeStatus(const BrakeTelemetry&,uint32_t,uint8_t*,size_t);
bool decodeBrakeStatus(const uint8_t*,size_t,uint32_t,BrakeTelemetry&);
struct BrakeDelivery{BrakeTelemetry state{};uint64_t receivedLocalUs=0;uint32_t requestElapsedMs=0;};
struct BrakeView{bool stateFresh=false;uint32_t stateAgeMs=kUnknownAge;std::array<bool,2>sensorFresh{{false,false}};std::array<uint32_t,2>sensorAgeMs{{kUnknownAge,kUnknownAge}};};
BrakeView updateBrakeView(const BrakeDelivery&,uint64_t now);
}
