#include "rnet_clock.hpp"
#include <linux/can.h>
namespace rnet::qt {
RnetClockValue decodeRnetClockFrame(quint32 canId,const QByteArray &payload){
  if((canId&(CAN_EFF_FLAG|CAN_RTR_FLAG|CAN_ERR_FLAG))!=CAN_EFF_FLAG||
     ((canId&CAN_EFF_MASK)&0xfffff0ffU)!=rnetClockBroadcastId||payload.size()!=6)
    return {};
  auto byte=[&](int index){return static_cast<unsigned char>(payload.at(index));};
  const int second=byte(0),minute=byte(1),hour=byte(2);
  const int day=byte(3)&0x1f;
  RnetClockValue value{QDate(2000+byte(5),byte(4),day),QTime(hour,minute,second)};
  // The OBP can transmit a weekday that does not match the calendar date
  // while the operator is editing it; retain a valid date and time.
  if(!value.isValid())return {};
  return value;
}
QByteArray encodeRnetClockTime(const QDateTime &hostTime){
  const auto date=hostTime.date();const auto time=hostTime.time();
  if(!hostTime.isValid()||date.year()<2000||date.year()>2099)return {};
  QByteArray payload(6,'\0');
  payload[0]=char(time.second());payload[1]=char(time.minute());payload[2]=char(time.hour());
  payload[3]=char((date.dayOfWeek()<<5)|date.day());
  payload[4]=char(date.month());payload[5]=char(date.year()-2000);
  return payload;
}
}
