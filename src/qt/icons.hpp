#pragma once
#include "database.hpp"
#include <QIcon>
#include <QString>
namespace rnet::qt {
enum class Icon {
  None, App, Read, Write, Folder, Save, SaveAs, Undo, Edit, Search,
  Monitor, Identity, Usb, Export, Check, Key, Info, Catalog, Files,
  Text, Number, Toggle, Choice, Bits, Settings, Battery, Joystick,
  Motor, Seating, Light, Bluetooth, Profiles, Shield, Signal, Wheel,
  Stop, Branch, Tools, Mouse, Clock
};
QIcon uiIcon(Icon);
Icon actionIcon(unsigned command);
Icon groupIcon(const QString &path);
Icon parameterIcon(const ParameterDefinition&,const ParameterCatalog&);
}
