#include "runtime_resources.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QFile>
#include <QUuid>
#include <QVariant>
#include <algorithm>
#include <functional>
#include <zlib.h>
namespace rnet {
namespace {
Bytes inflate(const Bytes &packed){
  if(packed.size()<5)throw std::runtime_error("Truncated bundled resource");
  std::uint32_t length=0;for(unsigned i=0;i<4;++i)length|=std::uint32_t(packed[i])<<(8*i);
  if(!length||length>64*1024*1024)throw std::runtime_error("Invalid bundled resource length");
  Bytes out(length);uLongf actual=length;
  if(::uncompress(out.data(),&actual,packed.data()+4,packed.size()-4)!=Z_OK||actual!=length)throw std::runtime_error("Bundled resource checksum or length differs");
  return out;
}
void ensureCoreApplication(){
  if(!QCoreApplication::instance()){
    static int argc=1;static char name[]="dfreernet-resources";static char *argv[]={name,nullptr};static QCoreApplication application(argc,argv);
  }
}
QString databasePath(){
  // Private read-only temporary copy: no original files and no writable master DB.
  static QTemporaryFile file;
  static const QString path=[&]{
    const auto data=inflate(assets::database());
    if(data.size()<16||std::string(reinterpret_cast<const char*>(data.data()),15)!="SQLite format 3")throw std::runtime_error("Invalid bundled SQLite header");
    if(!file.open()||file.write(reinterpret_cast<const char*>(data.data()),qint64(data.size()))!=qint64(data.size())||!file.flush())throw std::runtime_error("Cannot prepare bundled SQLite database");
    auto path=file.fileName();file.close();return path;
  }();return path;
}
class Database {
  QString name_;
public:
  QSqlDatabase db;
  explicit Database(const QString &path={},bool readonly=true){
    ensureCoreApplication();name_=QUuid::createUuid().toString();db=QSqlDatabase::addDatabase("QSQLITE",name_);if(readonly)db.setConnectOptions("QSQLITE_OPEN_READONLY");db.setDatabaseName(path.isEmpty()?databasePath():path);
    if(!db.open())throw std::runtime_error(("Cannot open bundled SQLite database: "+db.lastError().text()).toStdString());
    auto check=query("PRAGMA user_version");if(!check.next()||check.value(0).toInt()!=1)throw std::runtime_error("Unsupported bundled database version");
  }
  ~Database(){db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(name_);}
  QSqlQuery query(const char *sql){QSqlQuery q(db);if(!q.exec(QString::fromUtf8(sql)))throw std::runtime_error(("Catalog query failed: "+q.lastError().text()).toStdString());return q;}
};
Bytes blob(const char *name){Database d;QSqlQuery q(d.db);q.prepare("SELECT data FROM asset_blobs WHERE name=?");q.addBindValue(QString::fromLatin1(name));if(!q.exec()||!q.next())throw std::runtime_error("Bundled database asset is missing");auto data=q.value(0).toByteArray();return Bytes(data.begin(),data.end());}
std::u16string text(const QVariant &v){return v.toString().toStdU16String();}
ParameterCatalog loadCatalog(const QString &path={}){
  Database d(path);ParameterCatalog c;
  auto info=d.query("SELECT schema,parameter_offset,parameter_end,storage_map_offset,storage_map_end FROM catalog_info WHERE format_version=1");
  if(!info.next())throw std::runtime_error("Catalog metadata missing");
  c.schema=info.value(0).toUInt();
  c.parameter_offset=info.value(1).toULongLong();
  c.parameter_end=info.value(2).toULongLong();
  c.storage_map_offset=info.value(3).toULongLong();
  c.storage_map_end=info.value(4).toULongLong();
  auto p=d.query("SELECT key,id,extra84,flag24,signed_storage,flag70,flag71,extra8c,flagb0 FROM parameters ORDER BY key");
  while(p.next()){ParameterDefinition v;v.key=p.value(0).toUInt();v.id=p.value(1).toUInt();v.extra84=p.value(2).toUInt();v.flag24=p.value(3).toBool();v.signed_storage=p.value(4).toBool();v.flag70=p.value(5).toBool();v.flag71=p.value(6).toBool();v.extra8c=p.value(7).toUInt();v.flagb0=p.value(8).toBool();c.parameters.emplace(v.key,std::move(v));}
  auto t=d.query("SELECT parameter_key,kind,ordinal,text FROM parameter_texts");
  while(t.next()){auto &v=c.parameters.at(t.value(0).toUInt());auto index=t.value(2).toUInt();auto s=text(t.value(3));switch(t.value(1).toInt()){case 0:v.texts.at(index)=s;break;case 1:v.option_texts.at(index)=s;break;case 2:if(index)throw std::runtime_error("Invalid group text index");v.text88=s;break;case 3:if(index)throw std::runtime_error("Invalid detail text index");v.text90=s;break;default:throw std::runtime_error("Invalid parameter text kind");}}
  auto f=d.query("SELECT parameter_key,kind,ordinal,value FROM parameter_fields");
  while(f.next()){auto &v=c.parameters.at(f.value(0).toUInt());auto index=f.value(2).toUInt();auto n=f.value(3).toUInt();switch(f.value(1).toInt()){case 0:v.metadata.at(index)=n;break;case 1:v.serialized_metadata.at(index)=n;break;case 2:v.extension.at(index)=n;break;default:throw std::runtime_error("Invalid parameter field kind");}}
  auto m=d.query("SELECT map_key,id,version,name FROM storage_maps");while(m.next()){StorageMap v;v.id=m.value(1).toUInt();v.version=m.value(2).toUInt();v.name=text(m.value(3));c.storage_maps.emplace(m.value(0).toULongLong(),std::move(v));}
  auto g=d.query("SELECT map_key,group_index,instances,stride,last_section FROM storage_groups");while(g.next()){auto &v=c.storage_maps.at(g.value(0).toULongLong()).groups.at(g.value(1).toUInt());v.instances=g.value(2).toUInt();v.stride=g.value(3).toUInt();v.last_section=g.value(4).toUInt();}
  auto s=d.query("SELECT map_key,group_index,section_index,field0,field1,field2,field3,offset FROM storage_sections");while(s.next()){auto &group=c.storage_maps.at(s.value(0).toULongLong()).groups.at(s.value(1).toUInt());auto index=s.value(2).toUInt();for(unsigned i=0;i<4;++i)group.sections.at(index).fields[i]=s.value(i+3).toUInt();group.offsets.at(index)=s.value(7).toUInt();}
  auto items=d.query("SELECT map_key,group_index,section_index,ordinal,id,width FROM storage_items ORDER BY map_key,group_index,section_index,ordinal");while(items.next()){auto &v=c.storage_maps.at(items.value(0).toULongLong()).groups.at(items.value(1).toUInt()).sections.at(items.value(2).toUInt()).items;if(items.value(3).toUInt()!=v.size())throw std::runtime_error("Non-contiguous storage items");v.push_back({items.value(4).toUInt(),items.value(5).toUInt()});}
  auto e=d.query("SELECT key,id,flags,kind,count,extra,name,mapped FROM editors");while(e.next()){PredefinedEditor v;v.id=e.value(1).toUInt();v.flags=e.value(2).toUInt();v.kind=e.value(3).toUInt();v.count=e.value(4).toUInt();v.extra=e.value(5).toUInt();v.name=text(e.value(6));v.mapped=e.value(7).toBool();c.editors.emplace(e.value(0).toUInt(),std::move(v));}
  auto options=d.query("SELECT editor_key,ordinal,label,value FROM editor_options");while(options.next()){auto &v=c.editors.at(options.value(0).toUInt());auto index=options.value(1).toUInt();v.labels.at(index)=text(options.value(2));v.values.at(index)=options.value(3).toUInt();}
  auto names=d.query("SELECT id,name FROM file_names");while(names.next())c.file_names.emplace(names.value(0).toUInt(),text(names.value(1)));
  if(c.schema!=33||c.parameters.empty()||c.storage_maps.empty()||c.editors.empty())throw std::runtime_error("Bundled catalog is incomplete");
  return c;
}
}
namespace assets {
Bytes catalog(){return blob("catalog");}Bytes document(){return blob("document");}Bytes seed(){return blob("seed");}Bytes menu(){return blob("menu_source");}Bytes texts(){return blob("texts_source");}
}
const Bytes &bundled_catalog_bytes(){static const auto data=inflate(assets::catalog());return data;}
ParameterCatalog bundled_catalog(){static const auto catalog=loadCatalog();return catalog;}
ParameterCatalog read_catalog_database(const std::filesystem::path &path){return loadCatalog(QString::fromStdString(path.string()));}

const Bytes &bundled_default_document(){static const auto data=inflate(assets::document());return data;}
std::vector<MenuItem> bundled_menus(){
  Database d;auto q=d.query("SELECT node,parent,ordinal,command,flags,text FROM menus ORDER BY ordinal,node");
  std::map<unsigned,MenuItem> nodes;std::map<unsigned,std::vector<std::pair<unsigned,unsigned>>> children;
  while(q.next()){auto id=q.value(0).toUInt();MenuItem item;item.id=q.value(3).toUInt();item.flags=q.value(4).toUInt();item.text=text(q.value(5));nodes.emplace(id,std::move(item));children[q.value(1).isNull()?0:q.value(1).toUInt()].push_back({q.value(2).toUInt(),id});}
  std::function<std::vector<MenuItem>(unsigned,unsigned)> build=[&](unsigned parent,unsigned depth){if(depth>16)throw std::runtime_error("Invalid menu recursion");auto &list=children[parent];std::sort(list.begin(),list.end());std::vector<MenuItem> result;for(const auto &[ordinal,id]:list){(void)ordinal;auto item=nodes.at(id);item.children=build(id,depth+1);result.push_back(std::move(item));}return result;};return build(0,0);
}
std::vector<TextResource> bundled_text_resources(){Database d;auto q=d.query("SELECT id,text FROM resource_texts ORDER BY ordinal");std::vector<TextResource> result;while(q.next())result.push_back({q.value(0).toUInt(),text(q.value(1))});return result;}
void write_catalog_database(const Bytes &plain,const std::filesystem::path &path){
  const auto catalog=ParameterCatalog::read(plain); // Validate before creating output.
  ensureCoreApplication();const auto target=QString::fromStdString(path.string());
  if(QFile::exists(target))throw std::runtime_error("Refuse overwriting an existing catalog database");
  if(!QFile::copy(databasePath(),target))throw std::runtime_error("Cannot create catalog database");
  try {
    Database connection(target,false);auto &db=connection.db;
    if(!db.transaction())throw std::runtime_error("Cannot begin catalog import transaction");
    auto exec=[&](const char *sql,const QVariantList &values){QSqlQuery q(db);if(!q.prepare(QString::fromUtf8(sql)))throw std::runtime_error(q.lastError().text().toStdString());for(const auto &v:values)q.addBindValue(v);if(!q.exec())throw std::runtime_error(q.lastError().text().toStdString());};
    for(const auto *table:{"parameter_texts","parameter_fields","parameters","storage_items","storage_sections","storage_groups","storage_maps","editor_options","editors","file_names","catalog_info"}){
      QSqlQuery q(db);if(!q.exec("DELETE FROM "+QString::fromLatin1(table)))throw std::runtime_error(q.lastError().text().toStdString());
    }
    exec("INSERT INTO catalog_info VALUES(1,?,?,?,?,?)",{catalog.schema,qulonglong(catalog.parameter_offset),qulonglong(catalog.parameter_end),qulonglong(catalog.storage_map_offset),qulonglong(catalog.storage_map_end)});
    auto str=[](const std::u16string &s){return QString::fromStdU16String(s);};
    for(const auto &[key,p]:catalog.parameters){
      exec("INSERT INTO parameters VALUES(?,?,?,?,?,?,?,?,?)",{key,p.id,p.extra84,p.flag24,p.signed_storage,p.flag70,p.flag71,p.extra8c,p.flagb0});
      for(unsigned i=0;i<p.texts.size();++i)exec("INSERT INTO parameter_texts VALUES(?,?,?,?)",{key,0,i,str(p.texts[i])});
      for(unsigned i=0;i<p.option_texts.size();++i)exec("INSERT INTO parameter_texts VALUES(?,?,?,?)",{key,1,i,str(p.option_texts[i])});
      exec("INSERT INTO parameter_texts VALUES(?,?,?,?)",{key,2,0,str(p.text88)});exec("INSERT INTO parameter_texts VALUES(?,?,?,?)",{key,3,0,str(p.text90)});
      for(unsigned i=0;i<13;++i){exec("INSERT INTO parameter_fields VALUES(?,?,?,?)",{key,0,i,p.metadata[i]});exec("INSERT INTO parameter_fields VALUES(?,?,?,?)",{key,1,i,p.serialized_metadata[i]});}
      for(unsigned i=0;i<4;++i)exec("INSERT INTO parameter_fields VALUES(?,?,?,?)",{key,2,i,p.extension[i]});
    }
    for(const auto &[key,m]:catalog.storage_maps){const auto mapKey=QString::number(key);
      exec("INSERT INTO storage_maps VALUES(?,?,?,?)",{mapKey,m.id,m.version,str(m.name)});
      for(unsigned gi=0;gi<2;++gi){const auto &g=m.groups[gi];exec("INSERT INTO storage_groups VALUES(?,?,?,?,?)",{mapKey,gi,g.instances,g.stride,g.last_section});
        for(unsigned si=0;si<7;++si){const auto &section=g.sections[si];exec("INSERT INTO storage_sections VALUES(?,?,?,?,?,?,?,?)",{mapKey,gi,si,section.fields[0],section.fields[1],section.fields[2],section.fields[3],g.offsets[si]});
          for(unsigned i=0;i<section.items.size();++i){const auto &item=section.items[i];exec("INSERT INTO storage_items VALUES(?,?,?,?,?,?)",{mapKey,gi,si,i,item.id,item.width});}
        }
      }
    }
    for(const auto &[key,e]:catalog.editors){exec("INSERT INTO editors VALUES(?,?,?,?,?,?,?,?)",{key,e.id,e.flags,e.kind,e.count,e.extra,str(e.name),e.mapped});for(unsigned i=0;i<33;++i)exec("INSERT INTO editor_options VALUES(?,?,?,?)",{key,i,str(e.labels[i]),e.values[i]});}
    for(const auto &[id,name]:catalog.file_names)exec("INSERT INTO file_names VALUES(?,?)",{id,str(name)});
    uLongf length=compressBound(plain.size());Bytes packed(length+4);for(unsigned i=0;i<4;++i)packed[i]=std::uint8_t(plain.size()>>(i*8));if(compress2(packed.data()+4,&length,plain.data(),plain.size(),9)!=Z_OK)throw std::runtime_error("Cannot compress imported catalog");packed.resize(length+4);
    exec("UPDATE asset_blobs SET data=? WHERE name='catalog'",{QByteArray(reinterpret_cast<const char*>(packed.data()),int(packed.size()))});
    exec("INSERT OR REPLACE INTO provenance VALUES('catalog_sha256',?)",{QString::fromLatin1(QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(plain.data()),int(plain.size())),QCryptographicHash::Sha256).toHex())});
    exec("INSERT OR REPLACE INTO provenance VALUES('origin','User-imported RND catalog; controller data unchanged')",{});
    if(!db.commit())throw std::runtime_error("Cannot commit catalog import");
    auto integrity=connection.query("PRAGMA integrity_check");if(!integrity.next()||integrity.value(0).toString()!="ok")throw std::runtime_error("Imported catalog database failed integrity check");
  }catch(...){QFile::remove(target);throw;}
  try{const auto imported=read_catalog_database(path);if(imported.parameters.size()!=catalog.parameters.size()||imported.storage_maps.size()!=catalog.storage_maps.size())throw std::runtime_error("Imported catalog verification failed");}catch(...){QFile::remove(target);throw;}
}

}
