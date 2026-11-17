#include "rnet_distance.hpp"
#include <linux/can.h>
namespace rnet::qt {
RnetDistanceValue decodeRnetDistanceFrame(quint32 canId,const QByteArray &payload){
  if((canId&(CAN_EFF_FLAG|CAN_RTR_FLAG|CAN_ERR_FLAG))!=CAN_EFF_FLAG||
     (canId&CAN_EFF_MASK)!=rnetDistanceBroadcastId||payload.size()!=8)return {};
  auto word=[&](int offset){
    quint32 value=0;
    for(int i=0;i<4;++i)value|=quint32(quint8(payload.at(offset+i)))<<(i*8);
    return value;
  };
  const auto total=word(0),trip=word(4);
  if(total==0xffffffffU||trip==0xffffffffU)return {};
  return {total,trip,true};
}
}
