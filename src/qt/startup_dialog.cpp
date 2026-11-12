#include "startup_dialog.hpp"
#include "icons.hpp"
#include "app_settings.hpp"
#include <QtWidgets>
namespace rnet::qt {
StartupDialog::StartupDialog(QWidget *parent):QDialog(parent){
  setObjectName("startupNotice");
  setWindowTitle(QCoreApplication::translate("RNet","dfreernet %1 — Warning and disclaimer")
      .arg(QStringLiteral(DFREERNET_VERSION)));
  setWindowIcon(uiIcon(Icon::App));
  setWindowModality(Qt::ApplicationModal);
  setSizeGripEnabled(true);
  auto *layout=new QVBoxLayout(this);layout->setContentsMargins(24,22,24,20);layout->setSpacing(16);
  auto *header=new QHBoxLayout;
  auto *symbol=new QLabel;symbol->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(48,48));
  symbol->setAccessibleName(QCoreApplication::translate("RNet","Warning"));header->addWidget(symbol,0,Qt::AlignTop);
  auto *title=new QLabel(QCoreApplication::translate("RNet","Read before using dfreernet %1")
      .arg(QStringLiteral(DFREERNET_VERSION)));
  title->setObjectName("startupTitle");
  auto font=title->font();font.setPointSizeF(font.pointSizeF()*1.35);font.setBold(true);title->setFont(font);title->setWordWrap(true);
  header->addSpacing(12);header->addWidget(title,1);layout->addLayout(header);
  auto tr=[](const char *source){return QCoreApplication::translate("RNet",source).toHtmlEscaped();};
  auto *notice=new QTextBrowser;notice->setObjectName("startupNoticeText");
  notice->setFrameShape(QFrame::NoFrame);notice->setOpenLinks(false);notice->setOpenExternalLinks(false);
  notice->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
  notice->setAccessibleName(QCoreApplication::translate("RNet","Warning and disclaimer"));
  auto section=[](const QString &text){return "<h3>"+text+"</h3>";};
  auto para=[](const QString &text){return "<p>"+text+"</p>";};
  notice->setHtml(
      para(tr("Independent, experimental software. This is not an official controller-manufacturer programming tool."))+
      section(tr("Warning"))+
      para(tr("Incorrect settings can disable the wheelchair or cause unintended movement. Only change parameters you understand and are authorized to change."))+
      "<ul><li>"+tr("Keep the wheelchair stationary during controller access. Driving must not be needed until readback and release of programming access are complete.")+"</li><li>"+
      tr("Back up the configuration, check the selected Dongle access level, and verify written values by reading them back. Do not repeat an unverified write.")+"</li></ul>"+
      section(tr("Disclaimer"))+
      para(tr("This software is provided without any assurance of completeness, freedom from defects or suitability for a particular purpose. Use is at your own risk. You are responsible for checking and approving changes."))+
      para(tr("Liability is excluded only to the extent permitted by applicable law. Mandatory statutory liability remains unaffected."))+
      para(tr("This notice does not replace training, the manufacturer's instructions or the confirmation required before a controller operation.")));
  layout->addWidget(notice,1);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
  auto *proceed=buttons->button(QDialogButtonBox::Ok);proceed->setObjectName("startupContinue");
  proceed->setText(QCoreApplication::translate("RNet","I understand — Continue"));proceed->setMinimumHeight(38);
  auto *leave=buttons->button(QDialogButtonBox::Cancel);leave->setObjectName("startupExit");
  leave->setText(QCoreApplication::translate("RNet","Exit"));leave->setMinimumHeight(38);leave->setDefault(true);leave->setFocus();
  connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);
  connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
  layout->addWidget(buttons);resize(720,610);setMinimumSize(500,430);
}
void StartupDialog::showEvent(QShowEvent *event){
  QDialog::showEvent(event);
  // QDialogButtonBox may otherwise choose its first accept-role button on show.
  auto *leave=findChild<QPushButton*>("startupExit");
  leave->setDefault(true);leave->setFocus(Qt::OtherFocusReason);
}
void StartupDialog::placeOnSecondary(){
  auto *screen=preferredScreen(AppSettings::load().monitor);
  if(!screen)return;
  const auto area=screen->availableGeometry();
  resize(qMin(720,area.width()-32),qMin(610,area.height()-32));
  winId();windowHandle()->setScreen(screen);move(area.center()-rect().center());
}
}
