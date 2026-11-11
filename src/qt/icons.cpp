#include "icons.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QHash>
namespace rnet::qt {
namespace {
void draw(QPainter &p,Icon icon){
  const QColor blue("#346A9C"),teal("#16877E"),amber("#C17D21"),slate("#566B80");
  p.setPen(QPen(blue,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
  auto line=[&](qreal x,qreal y,qreal a,qreal b){p.drawLine(QPointF(x,y),QPointF(a,b));};
  auto rect=[&](qreal x,qreal y,qreal w,qreal h){p.drawRoundedRect(QRectF(x,y,w,h),1.5,1.5);};
  auto circle=[&](qreal x,qreal y,qreal r){p.drawEllipse(QPointF(x,y),r,r);};
  auto chip=[&]{rect(5,5,14,14);for(int v:{8,12,16}){line(v,2,v,5);line(v,19,v,22);line(2,v,5,v);line(19,v,22,v);}rect(9,9,6,6);};
  auto file=[&]{QPainterPath path;path.moveTo(5,2.5);path.lineTo(14,2.5);path.lineTo(19,7.5);path.lineTo(19,21.5);path.lineTo(5,21.5);path.closeSubpath();p.drawPath(path);line(14,3,14,8);line(14,8,19,8);};
  auto pencil=[&]{QPainterPath path;path.moveTo(5,15);path.lineTo(16,4);path.lineTo(20,8);path.lineTo(9,19);path.lineTo(4,20);path.closeSubpath();p.drawPath(path);line(14,6,18,10);line(5,15,9,19);};
  auto disk=[&]{rect(3,3,18,18);rect(7,3,10,6);rect(7,14,10,7);line(14,5,14,7);};
  auto arrow=[&](bool up){p.setPen(QPen(up?amber:teal,2.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));line(16,up?20:12,16,up?11:21);line(12.5,up?14.5:17.5,16,up?11:21);line(19.5,up?14.5:17.5,16,up?11:21);};
  switch(icon){
  case Icon::App:chip();break;
  case Icon::Read:case Icon::Write:rect(3,3,15,10);line(7,16,7,13);line(4,16,10,16);arrow(icon==Icon::Write);break;
  case Icon::Folder:{QPainterPath path;path.moveTo(3,7);path.lineTo(3,4);path.lineTo(10,4);path.lineTo(12,7);path.lineTo(21,7);path.lineTo(19,20);path.lineTo(3,20);path.closeSubpath();p.fillPath(path,QColor("#ECF3FA"));p.drawPath(path);line(3,10,20,10);break;}
  case Icon::Save:disk();break;
  case Icon::SaveAs:disk();p.setBrush(QColor("#FFFFFF"));p.setPen(Qt::NoPen);p.drawRect(QRectF(13,12,10,11));p.setBrush(Qt::NoBrush);p.setPen(QPen(teal,2,Qt::SolidLine,Qt::RoundCap));line(15,20,21,14);line(14,22,15,19);line(14,22,17,21);break;
  case Icon::Undo:{QPainterPath path;path.moveTo(4,9);path.cubicTo(19,3,24,18,11,20);p.drawPath(path);line(4,9,4,3);line(4,9,10,10);break;}
  case Icon::Edit:pencil();break;
  case Icon::Search:circle(10,10,6.5);line(15,15,21,21);break;
  case Icon::Monitor:rect(2,4,20,16);p.setPen(QPen(teal,1.8));{QPolygonF points{QPointF(4,13),QPointF(7,13),QPointF(9,8),QPointF(12,17),QPointF(15,11),QPointF(17,13),QPointF(20,13)};p.drawPolyline(points);}break;
  case Icon::Identity:rect(2,5,20,15);circle(7.5,10,2);p.drawArc(QRectF(4,13,7,5),0,180*16);line(14,9,19,9);line(14,13,19,13);line(14,16,18,16);break;
  case Icon::Usb:line(12,21,12,4);line(9,7,12,3);line(15,7,12,3);line(12,15,6,12);line(6,12,6,9);circle(6,7,1.5);line(12,18,18,14);line(18,14,18,11);p.drawRect(QRectF(16.5,8,3,3));break;
  case Icon::Export:file();p.setPen(QPen(teal,2));line(11,15,22,15);line(18,11,22,15);line(18,19,22,15);break;
  case Icon::Check:file();p.setPen(QPen(teal,2));line(8,15,11,18);line(11,18,17,11);break;
  case Icon::Key:circle(8,9,4);line(11,12,20,21);line(16,17,19,14);line(18,19,21,16);break;
  case Icon::Info:circle(12,12,9);line(12,11,12,17);circle(12,7,0.5);break;
  case Icon::Catalog:rect(3,3,18,18);line(8,3,8,21);for(int y:{8,12,16})line(11,y,18,y);break;
  case Icon::Files:rect(3,3,13,15);p.setBrush(QColor("#FFFFFF"));rect(8,7,13,15);p.setBrush(Qt::NoBrush);line(11,12,18,12);line(11,16,18,16);break;
  case Icon::Text:{QFont f("sans-serif");f.setPixelSize(17);f.setBold(true);p.setFont(f);p.drawText(QRectF(1,1,22,22),Qt::AlignCenter,"Tt");break;}
  case Icon::Number:p.setPen(QPen(teal,1.9));line(9,4,7,20);line(16,4,14,20);line(4,9,21,9);line(3,15,20,15);break;
  case Icon::Toggle:rect(2,5,20,14);line(12,6,12,18);{QFont f("sans-serif");f.setPixelSize(10);f.setBold(true);p.setFont(f);p.drawText(QRectF(3,5,8,14),Qt::AlignCenter,"0");p.drawText(QRectF(13,5,8,14),Qt::AlignCenter,"1");}break;
  case Icon::Choice:rect(2,4,20,16);line(6,9,13,9);line(6,14,12,14);line(16,11,18,14);line(18,14,20,11);break;
  case Icon::Bits:for(int x:{4,10,16})for(int y:{5,13})p.drawRect(QRectF(x,y,4,5));break;
  case Icon::Settings:for(int y:{6,12,18})line(3,y,21,y);p.setBrush(QColor("#FFFFFF"));circle(8,6,2);circle(16,12,2);circle(10,18,2);break;
  case Icon::Battery:p.setPen(QPen(teal,1.7));rect(2,6,18,12);line(22,10,22,14);line(6,10,6,14);line(10,10,10,14);line(14,10,14,14);break;
  case Icon::Joystick:rect(3,16,18,5);line(12,16,10,8);circle(9,5,3);line(6,13,8,11);line(18,13,16,11);break;
  case Icon::Motor:circle(12,12,7);circle(12,12,2);line(2,12,5,12);line(19,12,22,12);line(8,20,16,20);line(8,12,10,9);line(16,12,14,15);break;
  case Icon::Seating:line(5,3,7,15);line(7,15,18,15);line(18,15,21,20);line(7,15,4,20);line(8,11,17,11);line(17,11,17,15);break;
  case Icon::Light:p.setPen(QPen(amber,1.7));p.drawArc(QRectF(7,5,10,11),0,180*16);line(7,10,9,17);line(17,10,15,17);line(9,17,15,17);line(10,21,14,21);line(12,1,12,3);line(3,5,5,7);line(19,7,21,5);break;
  case Icon::Bluetooth:line(11,2,11,22);line(11,2,18,7);line(18,7,6,17);line(11,22,18,17);line(18,17,6,7);break;
  case Icon::Profiles:circle(9,7,3);p.drawArc(QRectF(3,13,13,10),0,180*16);line(17,5,21,5);line(18,10,22,10);line(18,15,22,15);break;
  case Icon::Shield:{QPainterPath path;path.moveTo(12,2);path.lineTo(21,6);path.cubicTo(21,15,18,19,12,22);path.cubicTo(6,19,3,15,3,6);path.closeSubpath();p.drawPath(path);line(8,11,11,14);line(11,14,16,9);break;}
  case Icon::Signal:circle(12,12,2);p.drawArc(QRectF(5,5,14,14),-60*16,120*16);p.drawArc(QRectF(1,1,22,22),-55*16,110*16);p.drawArc(QRectF(5,5,14,14),120*16,120*16);break;
  case Icon::Wheel:circle(12,12,9);circle(12,12,2);for(auto point:{QPointF(12,3),QPointF(21,12),QPointF(12,21),QPointF(3,12)})p.drawLine(QPointF(12,12),point);break;
  case Icon::Stop:circle(12,12,9);p.setPen(QPen(slate,2.5));p.drawRect(QRectF(8,8,8,8));break;
  case Icon::Branch:line(6,3,6,19);line(6,8,15,8);line(6,18,15,18);rect(15,5,6,6);rect(15,15,6,6);break;
  case Icon::Tools:circle(16,7,4);line(13,10,4,19);line(4,19,6,21);line(6,21,15,12);p.setPen(QPen(QColor("#FFFFFF"),3));line(17,3,19,5);break;
  case Icon::Mouse:rect(6,2,12,20);line(6,9,18,9);line(12,3,12,8);break;
  case Icon::Clock:circle(12,12,9);line(12,5,12,12);line(12,12,17,15);break;
  case Icon::None:break;
  }
}
}
QIcon uiIcon(Icon icon){
  if(icon==Icon::None)return {};
  static QHash<int,QIcon> cache;auto key=int(icon);
  if(cache.contains(key))return cache.value(key);
  QIcon result;
  for(int size:{16,20,24,32,48,64,96}){
    QPixmap pixmap(size,size);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);painter.scale(size/24.,size/24.);draw(painter,icon);painter.end();result.addPixmap(pixmap);
  }
  cache.insert(key,result);return result;
}
Icon actionIcon(unsigned id){
  switch(id){
  case 40030:case 32803:return Icon::Read;
  case 32804:return Icon::Write;
  case 57601:return Icon::Folder;
  case 57603:return Icon::Save;
  case 57604:return Icon::SaveAs;
  case 57643:return Icon::Undo;
  case 57636:return Icon::Search;
  case 40011:return Icon::Edit;
  case 40004:return Icon::Export;
  case 40002:return Icon::Check;
  case 40031:return Icon::Identity;
  case 40025:return Icon::Monitor;
  case 40003:return Icon::Usb;
  case 40009:return Icon::Settings;
  case 40010:return Icon::Files;
  case 40001:return Icon::Text;
  case 33201:return Icon::Signal;
  case 32965:return Icon::Monitor;
  case 32876:return Icon::Files;
  case 32979:case 33051:return Icon::Clock;
  case 33182:return Icon::Wheel;
  case 40008:return Icon::Catalog;
  case 57664:return Icon::Info;
  default:return Icon::None;
  }
}
Icon groupIcon(const QString &path){
  const auto name=path.toLower();
  if(name.contains("joystick")||name.contains("thumbwheel"))return Icon::Joystick;
  if(name.startsWith("battery"))return Icon::Battery;
  if(name.startsWith("motor")||name.startsWith("servo"))return Icon::Motor;
  if(name.startsWith("seating"))return Icon::Seating;
  if(name.startsWith("lights"))return Icon::Light;
  if(name.startsWith("mouse"))return Icon::Mouse;
  if(name.startsWith("idevice")||name.contains("bluetooth"))return Icon::Bluetooth;
  if(name.startsWith("profile"))return Icon::Profiles;
  if(name.startsWith("speed"))return Icon::Wheel;
  if(name.startsWith("inhibits")||name.startsWith("stability"))return Icon::Shield;
  if(name.startsWith("encoder")||name.contains("gyro"))return Icon::Signal;
  if(name.startsWith("factory")||name.startsWith("oem"))return Icon::Key;
  if(name.startsWith("engineering")||name.startsWith("debug"))return Icon::Tools;
  if(name.startsWith("file"))return Icon::Files;
  if(name.startsWith("configuration")||name.startsWith("input output"))return Icon::App;
  if(name.startsWith("omni")||name.startsWith("mini joystick")||name.startsWith("compact joystick"))return Icon::Joystick;
  return Icon::Settings;
}
Icon parameterIcon(const ParameterDefinition &p,const ParameterCatalog &catalog){
  if(p.extension[0]==17)return Icon::Text;
  if(p.extension[0]==1)return Icon::Toggle;
  auto editor=catalog.editors.find(p.extension[0]);
  if(editor!=catalog.editors.end()){
    if(editor->second.kind==4)return Icon::Choice;
    if(editor->second.kind==3||editor->second.kind==9)return Icon::Bits;
  }
  if(p.extension[0]==13)return Icon::Number;
  return Icon::Settings;
}
}
