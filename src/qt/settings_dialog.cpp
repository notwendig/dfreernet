#include "settings_dialog.hpp"
#include "icons.hpp"
#include "i18n.hpp"
#include "access.hpp"
#include <QtWidgets>
namespace rnet::qt {
void SettingsDialog::bind(QObject *object,const char *property,const char *source){texts_.push_back({object,property,source});object->setProperty(property,QCoreApplication::translate("RNet",source));}
QFormLayout *SettingsDialog::page(const char *title){
  auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
  auto *heading=new QLabel;auto font=heading->font();font.setBold(true);font.setPointSizeF(font.pointSizeF()*1.25);heading->setFont(font);bind(heading,"text",title);layout->addWidget(heading);
  auto *form=new QFormLayout;form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);form->setVerticalSpacing(16);layout->addLayout(form);layout->addStretch();pages_->addWidget(page);return form;
}
QLineEdit *SettingsDialog::directory(QFormLayout *form,const char *label,const char *name,const QString &value){
  auto *container=new QWidget;auto *layout=new QHBoxLayout(container);layout->setContentsMargins(0,0,0,0);
  auto *line=new QLineEdit(value);line->setObjectName(name);layout->addWidget(line,1);
  auto *browse=new QPushButton;bind(browse,"text",QT_TRANSLATE_NOOP("RNet","Browse…"));browse->setIcon(uiIcon(Icon::Folder));layout->addWidget(browse);
  auto *caption=new QLabel;bind(caption,"text",label);caption->setBuddy(line);form->addRow(caption,container);
  connect(browse,&QPushButton::clicked,this,[this,line]{auto name=QFileDialog::getExistingDirectory(this,QCoreApplication::translate("RNet","Choose directory"),line->text());if(!name.isEmpty())line->setText(name);});return line;
}
SettingsDialog::SettingsDialog(const AppSettings &settings,QWidget *parent):QDialog(parent),settings_(settings){
  setObjectName("settingsDialog");bind(this,"windowTitle",QT_TRANSLATE_NOOP("RNet","Settings"));setWindowIcon(uiIcon(Icon::Settings));resize(880,650);setMinimumSize(740,540);
  auto *layout=new QVBoxLayout(this);auto *body=new QHBoxLayout;tabs_=new QListWidget;tabs_->setObjectName("settingsTabs");tabs_->setFixedWidth(205);tabs_->setIconSize(QSize(24,24));tabs_->setWordWrap(true);
  for(auto icon:{Icon::Settings,Icon::Usb,Icon::Folder,Icon::Profiles}){auto *item=new QListWidgetItem(uiIcon(icon),"",tabs_);item->setSizeHint(QSize(185,54));}
  pages_=new QStackedWidget;pages_->setObjectName("settingsPages");body->addWidget(tabs_);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(pages_);body->addWidget(scroll,1);layout->addLayout(body,1);
  auto row=[&](QFormLayout *form,const char *text,QWidget *control){auto *label=new QLabel;bind(label,"text",text);label->setBuddy(control);form->addRow(label,control);};
  auto spin=[&](const char *name,int low,int high,int value){auto *s=new QSpinBox;s->setObjectName(name);s->setRange(low,high);s->setValue(value);return s;};
  auto *general=page(QT_TRANSLATE_NOOP("RNet","General"));language_=new QComboBox;language_->setObjectName("settingsLanguage");language_->addItem("English (US)","en_US");language_->addItem("Deutsch","de_DE");language_->setCurrentIndex(language_->findData(settings.language));row(general,QT_TRANSLATE_NOOP("RNet","Language"),language_);
  access_=new QComboBox;access_->setObjectName("settingsAccess");for(int i=0;i<2;++i)access_->addItem("",i);access_->setCurrentIndex(qMin(settings.accessLevel,1));row(general,QT_TRANSLATE_NOOP("RNet","Default Dongle access level"),access_);
  showAll_=new QCheckBox;showAll_->setObjectName("settingsShowAllParameters");showAll_->setChecked(settings.showAllParameters);
  bind(showAll_,"text",QT_TRANSLATE_NOOP("RNet","Show all parameters up to the selected Dongle level"));general->addRow(showAll_);
  auto *accessNote=new QLabel;bind(accessNote,"text",QT_TRANSLATE_NOOP("RNet","Unread parameters are gray and read-only. OEM and Factory are selected in the Dongle menu with a warning for the current session."));accessNote->setWordWrap(true);general->addRow(accessNote);
  undo_=spin("settingsUndoLimit",1,1000,settings.undoLimit);row(general,QT_TRANSLATE_NOOP("RNet","Undo history (steps)"),undo_);
  keepAwake_=new QCheckBox;keepAwake_->setObjectName("settingsKeepAwake");keepAwake_->setChecked(settings.keepAwake);
  bind(keepAwake_,"text",QT_TRANSLATE_NOOP("RNet","Prevent Linux sleep while the app is open"));general->addRow(keepAwake_);
  auto *can=page(QT_TRANSLATE_NOOP("RNet","CAN"));interface_=new QComboBox;interface_->setObjectName("settingsCanInterface");interface_->setEditable(true);
  for(const auto &name:QDir("/sys/class/net").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){QFile file("/sys/class/net/"+name+"/type");if(file.open(QIODevice::ReadOnly)&&file.readAll().trimmed()=="280")interface_->addItem(name);}
  interface_->setCurrentText(settings.canInterface);row(can,QT_TRANSLATE_NOOP("RNet","CAN interface"),interface_);
  duration_=spin("settingsMonitorSeconds",1,120,settings.monitorSeconds);row(can,QT_TRANSLATE_NOOP("RNet","Monitor duration (seconds)"),duration_);
  heartbeat_=new QCheckBox;heartbeat_->setObjectName("settingsHeartbeat");heartbeat_->setChecked(settings.heartbeatEnabled);bind(heartbeat_,"text",QT_TRANSLATE_NOOP("RNet","Show receive-only CAN heartbeat"));can->addRow(heartbeat_);
  heartbeatTimeout_=spin("settingsHeartbeatTimeout",500,10000,settings.heartbeatTimeoutMs);row(can,QT_TRANSLATE_NOOP("RNet","Heartbeat timeout (milliseconds)"),heartbeatTimeout_);
  liveSignal_=new QCheckBox;liveSignal_->setObjectName("settingsLiveSignal");liveSignal_->setChecked(settings.liveSignalEnabled);bind(liveSignal_,"text",QT_TRANSLATE_NOOP("RNet","Live signal: periodic hazard lights"));can->addRow(liveSignal_);
  liveInterval_ = spin("settingsLiveInterval", 2, AppSettings::maxLiveSignalIntervalSeconds, settings.liveSignalIntervalSeconds);
  row(can, QT_TRANSLATE_NOOP("RNet", "Live-signal interval (seconds)"), liveInterval_);
  liveDuration_ = spin("settingsLiveDuration", 1, AppSettings::maxLiveSignalDurationSeconds, settings.liveSignalDurationSeconds);
  row(can, QT_TRANSLATE_NOOP("RNet", "Blink duration (seconds)"), liveDuration_);
  auto updateSignal=[this]{liveInterval_->setEnabled(liveSignal_->isChecked());liveDuration_->setEnabled(liveSignal_->isChecked());};
  connect(liveSignal_,&QCheckBox::toggled,this,updateSignal);updateSignal();
  auto *liveNote=new QLabel;bind(liveNote,"text",QT_TRANSLATE_NOOP("RNet","Starts when CAN heartbeat is available. Pauses during controller access. Stops when disabled or the app closes."));liveNote->setWordWrap(true);can->addRow(liveNote);
  auto *note=new QLabel;bind(note,"text",QT_TRANSLATE_NOOP("RNet","The selected interface is used directly. Configure and enable it in Linux before controller access. Enabling or disabling the live signal controls the hazard-light cycle; controller parameters are unchanged."));note->setWordWrap(true);can->addRow(note);
  auto *folders=page(QT_TRANSLATE_NOOP("RNet","Files and folders"));working_=directory(folders,QT_TRANSLATE_NOOP("RNet","Working directory"),"settingsWorkingDirectory",settings.workingDirectory);backup_=directory(folders,QT_TRANSLATE_NOOP("RNet","Controller backups"),"settingsBackupDirectory",settings.backupDirectory);exports_=directory(folders,QT_TRANSLATE_NOOP("RNet","Exports"),"settingsExportDirectory",settings.exportDirectory);logs_=directory(folders,QT_TRANSLATE_NOOP("RNet","Log directory (restart)"),"settingsLogDirectory",settings.logDirectory);
  auto *appearance=page(QT_TRANSLATE_NOOP("RNet","Appearance"));monitor_=new QComboBox;monitor_->setObjectName("settingsMonitor");monitor_->addItem("","secondary");monitor_->addItem("","primary");
  for(auto *screen:QGuiApplication::screens())monitor_->addItem(screen->name(),screen->name());
  if(monitor_->findData(settings.monitor)<0)monitor_->addItem(settings.monitor,settings.monitor);
  monitor_->setCurrentIndex(monitor_->findData(settings.monitor));row(appearance,QT_TRANSLATE_NOOP("RNet","Preferred monitor"),monitor_);
  maximized_=new QCheckBox;maximized_->setChecked(settings.maximized);bind(maximized_,"text",QT_TRANSLATE_NOOP("RNet","Start maximized"));appearance->addRow(maximized_);
  details_=new QCheckBox;details_->setChecked(settings.showDetails);bind(details_,"text",QT_TRANSLATE_NOOP("RNet","Show parameter details"));appearance->addRow(details_);
  toolbarIconsOnly_ = new QCheckBox;
  toolbarIconsOnly_->setObjectName("settingsToolbarIconsOnly");
  toolbarIconsOnly_->setChecked(settings.toolbarIconsOnly);
  bind(toolbarIconsOnly_, "text", QT_TRANSLATE_NOOP("RNet", "Toolbar: icons only"));
  appearance->addRow(toolbarIconsOnly_);
  toolbarIcons_=spin("settingsToolbarIcons",16,32,settings.toolbarIconSize);row(appearance,QT_TRANSLATE_NOOP("RNet","Toolbar icons (pixels)"),toolbarIcons_);
  itemIcons_=spin("settingsItemIcons",16,28,settings.itemIconSize);row(appearance,QT_TRANSLATE_NOOP("RNet","Tree and parameter icons (pixels)"),itemIcons_);
  font_=spin("settingsFontSize",0,30,settings.fontSize);bind(font_,"specialValueText",QT_TRANSLATE_NOOP("RNet","System default"));row(appearance,QT_TRANSLATE_NOOP("RNet","Font size (points)"),font_);
  error_=new QLabel;error_->setObjectName("settingsError");error_->setWordWrap(true);error_->setStyleSheet("color: red");layout->addWidget(error_);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Apply);buttons->setObjectName("settingsButtons");layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::accepted,this,[this]{if(apply())accept();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,[this]{apply();});
  connect(tabs_,&QListWidget::currentRowChanged,pages_,&QStackedWidget::setCurrentIndex);tabs_->setCurrentRow(0);retranslate();
}
void SettingsDialog::retranslate(){
  for(const auto &binding:texts_)if(binding.object)binding.object->setProperty(binding.property.constData(),QCoreApplication::translate("RNet",binding.source.constData()));
  const char *labels[]{QT_TRANSLATE_NOOP("RNet","General"),QT_TRANSLATE_NOOP("RNet","CAN"),QT_TRANSLATE_NOOP("RNet","Files and folders"),QT_TRANSLATE_NOOP("RNet","Appearance")};for(int i=0;i<4;++i){tabs_->item(i)->setText(QCoreApplication::translate("RNet",labels[i]));tabs_->item(i)->setSizeHint(QSize(185,qMax(54,fontMetrics().height()*2+8)));}
  for(int i=0;i<access_->count();++i)access_->setItemText(i,QCoreApplication::translate("RNet",access_level_name(AccessLevel(i))));
  monitor_->setItemText(0,QCoreApplication::translate("RNet","Secondary monitor (preferred)"));monitor_->setItemText(1,QCoreApplication::translate("RNet","Primary monitor"));
}
void SettingsDialog::changeEvent(QEvent *event){QDialog::changeEvent(event);if((event->type()==QEvent::LanguageChange||event->type()==QEvent::FontChange)&&pages_)retranslate();}
bool SettingsDialog::apply(){
  try{
    auto s=settings_;s.language=language_->currentData().toString();s.accessLevel=access_->currentData().toInt();s.undoLimit=undo_->value();
    s.canInterface=interface_->currentText();s.monitorSeconds=duration_->value();s.heartbeatEnabled=heartbeat_->isChecked();s.heartbeatTimeoutMs=heartbeatTimeout_->value();
    s.keepAwake=keepAwake_->isChecked();s.showAllParameters=showAll_->isChecked();
    s.liveSignalEnabled = liveSignal_->isChecked();
    s.liveSignalIntervalSeconds = liveInterval_->value();
    s.liveSignalDurationSeconds = liveDuration_->value();
    s.toolbarIconsOnly = toolbarIconsOnly_->isChecked();
    s.workingDirectory=working_->text();s.backupDirectory=backup_->text();s.exportDirectory=exports_->text();s.logDirectory=logs_->text();
    s.monitor=monitor_->currentData().toString();s.maximized=maximized_->isChecked();s.showDetails=details_->isChecked();s.toolbarIconSize=toolbarIcons_->value();s.itemIconSize=itemIcons_->value();s.fontSize=font_->value();
    s=s.validated();s.save();settings_=s;if(applied)applied(s);languages().setLanguage(s.language,true);error_->clear();return true;
  }catch(const std::exception &error){error_->setText(localizedError(error.what()));return false;}
}
}
