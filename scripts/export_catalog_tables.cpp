// Developer tool: export the already bundled catalog into relational source data.
#include "runtime_resources.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
using namespace rnet;
static QString text(const std::u16string&s){return QString::fromStdU16String(s);}
template<class C> QJsonArray numbers(const C &c){QJsonArray a;for(const auto&v:c)a.append(qint64(v));return a;}
int main(int argc,char**argv){QCoreApplication app(argc,argv);if(argc!=2)return 2;const auto c=bundled_catalog();QJsonObject root;root["info"]=QJsonArray{int(c.schema),qint64(c.parameter_offset),qint64(c.parameter_end),qint64(c.storage_map_offset),qint64(c.storage_map_end)};QJsonArray parameters,maps,editors,names;
for(const auto &[key,p]:c.parameters){QJsonArray strings,options;for(const auto&s:p.texts)strings.append(text(s));for(const auto&s:p.option_texts)options.append(text(s));parameters.append(QJsonObject{{"key",qint64(key)},{"id",qint64(p.id)},{"extra84",qint64(p.extra84)},{"flag24",p.flag24},{"signed_storage",p.signed_storage},{"flag70",p.flag70},{"flag71",p.flag71},{"extra8c",qint64(p.extra8c)},{"flagb0",p.flagb0},{"texts",strings},{"options",options},{"metadata",numbers(p.metadata)},{"serialized",numbers(p.serialized_metadata)},{"extension",numbers(p.extension)},{"text88",text(p.text88)},{"text90",text(p.text90)}});}
for(const auto &[key,m]:c.storage_maps){QJsonArray groups;for(const auto&g:m.groups){QJsonArray sections;for(unsigned i=0;i<7;++i){QJsonArray items;for(const auto&v:g.sections[i].items)items.append(QJsonArray{qint64(v.id),qint64(v.width)});sections.append(QJsonObject{{"fields",numbers(g.sections[i].fields)},{"offset",qint64(g.offsets[i])},{"items",items}});}groups.append(QJsonObject{{"instances",qint64(g.instances)},{"stride",qint64(g.stride)},{"last_section",qint64(g.last_section)},{"sections",sections}});}maps.append(QJsonObject{{"key",QString::number(key)},{"id",qint64(m.id)},{"version",qint64(m.version)},{"name",text(m.name)},{"groups",groups}});}
for(const auto &[key,e]:c.editors){QJsonArray labels;for(const auto&s:e.labels)labels.append(text(s));editors.append(QJsonObject{{"key",qint64(key)},{"id",qint64(e.id)},{"flags",qint64(e.flags)},{"kind",qint64(e.kind)},{"count",qint64(e.count)},{"extra",qint64(e.extra)},{"name",text(e.name)},{"mapped",e.mapped},{"labels",labels},{"values",numbers(e.values)}});}
for(const auto &[id,name]:c.file_names)names.append(QJsonArray{qint64(id),text(name)});root["parameters"]=parameters;root["maps"]=maps;root["editors"]=editors;root["names"]=names;QFile f(QString::fromLocal8Bit(argv[1]));if(!f.open(QIODevice::WriteOnly))return 1;return f.write(QJsonDocument(root).toJson(QJsonDocument::Compact))<0;}
