#include "value_editor.hpp"
#include "i18n.hpp"
#include <QtWidgets>
namespace rnet::qt {
namespace {
class RawIntegerValidator final : public QValidator {
  qint64 min_,max_,step_;
public:
  RawIntegerValidator(qint64 min,qint64 max,qint64 step,QObject *parent)
      :QValidator(parent),min_(min),max_(max),step_(step){}
  State validate(QString &text,int &) const override {
    if(text.isEmpty()||text=="-")return Intermediate;
    int start=text.startsWith('-')?1:0;
    for(int i=start;i<text.size();++i)if(text[i]<'0'||text[i]>'9')return Invalid;
    bool ok=false;auto value=text.toLongLong(&ok);
    return ok&&value>=min_&&value<=max_&&(!step_||(value-min_)%step_==0)?Acceptable:Intermediate;
  }
};
}
ParameterValueEditor::ParameterValueEditor(const ParameterDefinition &p,const DocumentValue &v,
    const ParameterCatalog &catalog,QWidget *parent)
    :QWidget(parent),catalog_(catalog),definition_(p),value_(v){
  setObjectName("parameterValueEditor");
  auto *layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
  const auto choices=parameter_choices(p,v,catalog);
  if(!choices.empty()){
    combo_=new QComboBox(this);combo_->setObjectName("choiceValueEditor");combo_->setEditable(false);
    for(const auto &choice:choices){
      auto label=QString::fromStdU16String(choice.label);
      if(choice.label==u"Yes")label=QCoreApplication::translate("RNet","Yes");
      if(choice.label==u"No")label=QCoreApplication::translate("RNet","No");
      combo_->addItem(label+" ("+QString::number(choice.value)+")",QVariant::fromValue(qlonglong(choice.value)));
    }
    combo_->setPlaceholderText(QCoreApplication::translate("RNet","Unlisted raw value: %1").arg(v.number));
    combo_->setCurrentIndex(combo_->findData(QVariant::fromValue(qlonglong(v.number))));
    layout->addWidget(combo_);setFocusProxy(combo_);
    connect(combo_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
  }else{
    line_=new QLineEdit(this);layout->addWidget(line_);setFocusProxy(line_);
    line_->setObjectName(v.numeric?"numberValueEditor":"textValueEditor");
    if(v.numeric){
      minimum_=p.signed_storage?qint32(p.metadata[4]):qint64(p.metadata[4]);
      maximum_=p.signed_storage?qint32(p.metadata[5]):qint64(p.metadata[5]);
      step_=p.metadata[6]?p.metadata[6]:1;
      line_->setValidator(new RawIntegerValidator(minimum_,maximum_,p.metadata[6],line_));
      line_->setText(QString::number(v.number));
      for(int direction:{1,-1}){
        auto *button=new QToolButton(this);button->setArrowType(direction>0?Qt::UpArrow:Qt::DownArrow);
        button->setObjectName(direction>0?"stepUp":"stepDown");
        button->setAccessibleName(QCoreApplication::translate("RNet",direction>0?"Increase value":"Decrease value"));
        button->setFocusPolicy(Qt::NoFocus);button->setAutoRepeat(true);layout->addWidget(button);
        connect(button,&QToolButton::clicked,this,[this,direction]{step(direction);});
      }
    }else{
      line_->setMaxLength(int(v.width));line_->setText(QString::fromStdU16String(v.text));
    }
    line_->selectAll();connect(line_,&QLineEdit::textChanged,this,[this]{refresh();});
  }
  refresh();
}
QString ParameterValueEditor::rawValue() const {
  return combo_?(combo_->currentIndex()<0?QString():QString::number(combo_->currentData().toLongLong())):line_->text();
}
bool ParameterValueEditor::valid(QString *error) const {
  try{encode_parameter_value(definition_,value_,rawValue().toStdU16String(),catalog_);if(error)error->clear();return true;}
  catch(const std::exception &e){if(error)*error=localizedError(e.what());return false;}
}
void ParameterValueEditor::refresh(){
  QString error;bool ok=valid(&error);setToolTip(error);
  if(line_){auto palette=line_->palette();palette.setColor(QPalette::Base,ok?QApplication::palette().color(QPalette::Base):QColor("#FFE0E0"));line_->setPalette(palette);}
  if(validityChanged)validityChanged();
}
void ParameterValueEditor::step(int direction){
  bool ok=false;auto value=line_->text().toLongLong(&ok);
  if(!ok)value=minimum_;
  value=qBound(minimum_,value,maximum_);
  auto offset=value-minimum_;auto next=offset/step_;
  if(direction>0)++next;else if(offset%step_==0)--next;
  next=qBound(qint64(0),next,(maximum_-minimum_)/step_);
  line_->setText(QString::number(minimum_+next*step_));
}
}
