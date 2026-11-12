#pragma once
#include <QString>
class QScreen;
namespace rnet::qt {
struct AppSettings {
  QString language="en_US",canInterface="can0",monitor="secondary";
  QString workingDirectory,backupDirectory,exportDirectory,logDirectory;
  static constexpr int maxLiveSignalIntervalSeconds = 86400;
  static constexpr int maxLiveSignalDurationSeconds = 3600;
  int liveSignalIntervalSeconds = 300;
  int liveSignalDurationSeconds = 60;
  int accessLevel=1,monitorSeconds=10,undoLimit=100,toolbarIconSize=24,itemIconSize=18,fontSize=0,heartbeatTimeoutMs=1500;
  bool toolbarIconsOnly = false;
  bool showAllParameters=true,liveSignalEnabled=false,profileTableView=false,moduleGroupView=false;
  bool maximized=true,showDetails=true,heartbeatEnabled=true,keepAwake=true;
  static AppSettings defaults();
  static AppSettings load();
  void save() const;
  AppSettings validated() const;
  QString newBackupDirectory(const QString &prefix) const;
};
QScreen *preferredScreen(const QString &preference);
void applyAppFont(int points);
}
