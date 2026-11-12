#include "app_settings.hpp"
#include "i18n.hpp"
#include <QtWidgets>
namespace rnet::qt {
AppSettings AppSettings::defaults(){
  AppSettings s;auto documents=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if(documents.isEmpty())documents=QDir::homePath();
  s.workingDirectory=QDir(documents).filePath("R-net");
  s.backupDirectory=QDir::homePath()+"/rnet-backups";
  s.exportDirectory=s.workingDirectory+"/exports";
  s.logDirectory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/logs";return s;
}
AppSettings AppSettings::load(){
  auto s=defaults();QSettings q;
  auto str=[&](const char *key,QString &value){value=q.value(key,value).toString();};
  auto num=[&](const char *key,int &value,int low,int high){bool ok=false;auto n=q.value(key,value).toInt(&ok);if(ok&&n>=low&&n<=high)value=n;};
  str("ui/language",s.language);if(s.language!="en_US"&&s.language!="de_DE")s.language="en_US";
  str("can/interface",s.canInterface);str("ui/monitor",s.monitor);
  str("paths/working",s.workingDirectory);str("paths/backups",s.backupDirectory);str("paths/exports",s.exportDirectory);
  str("paths/logs",s.logDirectory);
  num("dongle/accessLevel",s.accessLevel,0,1);num("can/monitorSeconds",s.monitorSeconds,1,120);
  num("editor/undoLimit",s.undoLimit,1,1000);num("ui/toolbarIconSize",s.toolbarIconSize,16,32);
  num("ui/itemIconSize",s.itemIconSize,16,28);num("ui/fontSize",s.fontSize,0,30);num("can/heartbeatTimeoutMs",s.heartbeatTimeoutMs,500,10000);
  s.maximized=q.value("ui/maximized",true).toBool();s.showDetails=q.value("ui/showDetails",true).toBool();
  s.keepAwake=q.value("ui/keepAwake",true).toBool();
  s.toolbarIconsOnly = q.value("ui/toolbarIconsOnly", s.toolbarIconsOnly).toBool();
  s.showAllParameters=q.value("ui/showAllParameters",true).toBool();
  s.profileTableView=q.value("ui/profileTableView",false).toBool();
  s.moduleGroupView=q.value("ui/moduleGroupView",false).toBool();
  if(s.moduleGroupView)s.profileTableView=false;
  s.liveSignalEnabled=false; // Never resume an uncertain toggle state after restart.
  // Convert saved minute settings before applying any newer second values.
  int intervalMinutes = s.liveSignalIntervalSeconds / 60;
  int durationMinutes = s.liveSignalDurationSeconds / 60;
  num("can/liveSignalIntervalMinutes", intervalMinutes, 2, maxLiveSignalIntervalSeconds / 60);
  num("can/liveSignalDurationMinutes", durationMinutes, 1, maxLiveSignalDurationSeconds / 60);
  s.liveSignalIntervalSeconds = intervalMinutes * 60;
  s.liveSignalDurationSeconds = durationMinutes * 60;
  num("can/liveSignalIntervalSeconds", s.liveSignalIntervalSeconds, 2, maxLiveSignalIntervalSeconds);
  num("can/liveSignalDurationSeconds", s.liveSignalDurationSeconds, 1, maxLiveSignalDurationSeconds);
  if (s.liveSignalDurationSeconds >= s.liveSignalIntervalSeconds)
    s.liveSignalDurationSeconds = 1;
  s.heartbeatEnabled=q.value("can/heartbeatEnabled",true).toBool();return s;
}
void AppSettings::save()const{
  QSettings q;
  q.setValue("ui/language",language);q.setValue("can/interface",canInterface);q.setValue("ui/monitor",monitor);
  q.setValue("paths/working",workingDirectory);q.setValue("paths/backups",backupDirectory);q.setValue("paths/exports",exportDirectory);
  q.setValue("paths/logs",logDirectory);
  q.setValue("ui/showAllParameters",showAllParameters);
  q.setValue("ui/profileTableView",profileTableView);
  q.setValue("ui/moduleGroupView",moduleGroupView);
  q.setValue("can/liveSignalEnabled", false); // Session-only light experiment.
  q.setValue("can/liveSignalIntervalSeconds", liveSignalIntervalSeconds);
  q.setValue("can/liveSignalDurationSeconds", liveSignalDurationSeconds);
  q.setValue("ui/toolbarIconsOnly", toolbarIconsOnly);
  q.setValue("dongle/accessLevel",qMin(accessLevel,1));q.setValue("can/monitorSeconds",monitorSeconds);q.setValue("editor/undoLimit",undoLimit);
  q.setValue("ui/toolbarIconSize",toolbarIconSize);q.setValue("ui/itemIconSize",itemIconSize);q.setValue("ui/fontSize",fontSize);
  q.setValue("ui/keepAwake",keepAwake);q.setValue("ui/maximized",maximized);q.setValue("ui/showDetails",showDetails);q.setValue("can/heartbeatEnabled",heartbeatEnabled);
  q.setValue("can/heartbeatTimeoutMs",heartbeatTimeoutMs);q.sync();
  if(q.status()!=QSettings::NoError)throw UiError(QCoreApplication::translate("RNet","Could not save application settings"));
}
AppSettings AppSettings::validated()const{
  auto s=*this;
  if (s.liveSignalIntervalSeconds < 2 || s.liveSignalIntervalSeconds > maxLiveSignalIntervalSeconds ||
      s.liveSignalDurationSeconds < 1 || s.liveSignalDurationSeconds > maxLiveSignalDurationSeconds ||
      s.liveSignalDurationSeconds >= s.liveSignalIntervalSeconds)
    throw UiError(QCoreApplication::translate("RNet", "Blink duration must be shorter than the interval (interval 2–86400 s, duration 1–3600 s)"));
  if(s.language!="en_US"&&s.language!="de_DE")throw UiError(QCoreApplication::translate("RNet","Unsupported language"));
  s.canInterface=s.canInterface.trimmed();
  if(!QRegularExpression("^[A-Za-z0-9_.-]{1,15}$").match(s.canInterface).hasMatch())
    throw UiError(QCoreApplication::translate("RNet","Enter a valid CAN interface name (up to 15 characters)"));
  for(auto *directory:{&s.workingDirectory,&s.backupDirectory,&s.exportDirectory,&s.logDirectory}){
    *directory=directory->trimmed();if(directory->startsWith("~/"))*directory=QDir::homePath()+directory->mid(1);
    if(directory->isEmpty()||!QDir::isAbsolutePath(*directory))throw UiError(QCoreApplication::translate("RNet","Use absolute directory paths"));
    *directory=QDir::cleanPath(*directory);
    if(QFileInfo::exists(*directory)&&!QFileInfo(*directory).isDir())throw UiError(QCoreApplication::translate("RNet","A configured directory is a file"));
  }
  if(s.accessLevel<0||s.accessLevel>3||s.monitorSeconds<1||s.monitorSeconds>120||s.undoLimit<1||s.undoLimit>1000||
     s.toolbarIconSize<16||s.toolbarIconSize>32||s.itemIconSize<16||s.itemIconSize>28||s.fontSize<0||s.fontSize>30||
     s.heartbeatTimeoutMs<500||s.heartbeatTimeoutMs>10000)throw UiError(QCoreApplication::translate("RNet","Invalid settings value"));
  return s;
}
QString AppSettings::newBackupDirectory(const QString &prefix)const{
  if(backupDirectory.isEmpty()||!QDir::isAbsolutePath(backupDirectory))throw UiError(QCoreApplication::translate("RNet","Use absolute directory paths"));
  if(!QDir().mkpath(backupDirectory))throw UiError(QCoreApplication::translate("RNet","Cannot create the configured backup directory; check Settings"));
  return QDir(backupDirectory).filePath(prefix+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}
QScreen *preferredScreen(const QString &preference){
  auto *primary=QGuiApplication::primaryScreen();
  if(preference=="primary")return primary;
  for(auto *screen:QGuiApplication::screens())if(screen->name()==preference)return screen;
  for(auto *screen:QGuiApplication::screens())if(screen!=primary)return screen;
  return primary;
}
void applyAppFont(int points){
  static const QFont original=QApplication::font();auto font=original;if(points>0)font.setPointSize(points);QApplication::setFont(font);
}
}
