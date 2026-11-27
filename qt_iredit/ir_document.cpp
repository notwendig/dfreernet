#include "ir_document.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>

namespace iredit {
namespace {
[[noreturn]] void fail(const char *message) {
    throw std::runtime_error(QCoreApplication::translate("RNet", message).toStdString());
}
constexpr int maxFile = 4 * 1024 * 1024;
constexpr int maxNodes = 32768;
constexpr auto assembly = "IRConfigurator, Version=2.3.2.0, Culture=neutral, PublicKeyToken=null";
const QString itemClass = QStringLiteral("IRConfigurator.Data.ProntoMenuItem");
const QString menuClass = QStringLiteral("IRConfigurator.Data.MenuFile`1[[") + itemClass + ", " + assembly + "]]";
const QString itemListClass = QStringLiteral("System.Collections.Generic.List`1[[") + itemClass + ", " + assembly + "]]";
const QString byteListClass = QStringLiteral("System.Collections.Generic.List`1[[System.Byte, mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089]]");

// A bounded NRBF data reader. It never loads .NET types or executes deserialization hooks.
struct Node;
using Value = std::shared_ptr<Node>;
struct Node {
    enum Kind { Null, Number, Ref, String, Object, Array, Bytes } kind = Null;
    int number = 0;
    QString text;
    QHash<QString, Value> fields;
    QVector<Value> values;
    QByteArray bytes;
};
Value node(Node::Kind kind) { auto n = std::make_shared<Node>(); n->kind = kind; return n; }
struct TypeInfo { quint8 type = 0, primitive = 0; QString name; int library = 0; };
struct Meta { QString name; QStringList names; QVector<TypeInfo> types; };
class Reader {
    QByteArray data_;
    int pos_ = 0, depth_ = 0, records_ = 0;
    QHash<int, Value> objects_;
    QHash<int, Meta> meta_;
    QHash<int, QString> libraries_;
    quint8 byte() { if (pos_ >= data_.size()) fail("Truncated IR file."); return quint8(data_[pos_++]); }
    int integer() {
        if (data_.size() - pos_ < 4) fail("Truncated IR file.");
        auto n = qFromLittleEndian<qint32>(data_.constData() + pos_); pos_ += 4; return n;
    }
    int length(int n) { if (n < 0 || n > maxNodes) fail("IR file exceeds supported limits."); return n; }
    QString string() {
        quint32 n = 0; int shift = 0;
        for (;;) { auto b = byte(); if (shift >= 28 && (b & 0xf8)) fail("Invalid IR string length."); n |= quint32(b & 127) << shift; if (!(b & 128)) break; shift += 7; }
        if (n > 65536 || n > quint32(data_.size() - pos_)) fail("Invalid IR string length.");
        QByteArray bytes = data_.mid(pos_, n); pos_ += int(n);
        auto result = QString::fromUtf8(bytes);
        if (result.toUtf8() != bytes) fail("Invalid UTF-8 in IR file.");
        return result;
    }
    TypeInfo typeInfo(quint8 kind) {
        TypeInfo t; t.type = kind;
        switch (kind) {
        case 0: case 7: t.primitive = byte(); break;
        case 3: t.name = string(); break;
        case 4: t.name = string(); t.library = integer(); break;
        case 1: case 2: case 5: case 6: break;
        default: fail("Unsupported IR field type.");
        }
        return t;
    }
    Value primitive(quint8 type) {
        auto n = node(Node::Number);
        if (type == 2 || type == 1) n->number = byte();
        else if (type == 8) n->number = integer();
        else fail("Unsupported IR primitive type.");
        return n;
    }
    void add(int id, const Value &v) {
        if (id <= 0 || objects_.contains(id) || objects_.size() >= maxNodes) fail("Invalid IR object identifier.");
        objects_.insert(id, v);
    }
    QVector<Value> arrayValues(int count) {
        QVector<Value> result; result.reserve(count);
        while (result.size() < count) {
            if (pos_ >= data_.size()) fail("Truncated IR file.");
            auto tag = quint8(data_[pos_]);
            if (tag == 13 || tag == 14) {
                byte(); int n = tag == 13 ? byte() : integer();
                if (n <= 0 || n > count - result.size()) fail("Invalid null array in IR file.");
                for (int i = 0; i < n; ++i) result.append(node(Node::Null));
            } else result.append(record());
        }
        return result;
    }
    Value object(int id, const Meta &m) {
        auto n = node(Node::Object); n->text = m.name; add(id, n);
        for (int i = 0; i < m.names.size(); ++i)
            n->fields.insert(m.names[i], m.types[i].type == 0 ? primitive(m.types[i].primitive) : record());
        return n;
    }
    Value record() {
        if (++records_ > maxNodes || ++depth_ > 128) fail("IR file exceeds supported limits.");
        auto result = recordBody(); --depth_; return result;
    }
    Value recordBody() {
        int tag = byte();
        if (tag == 12) {
            int id = integer(); auto name = string();
            if (id <= 0 || libraries_.contains(id)) fail("Invalid IR library identifier.");
            libraries_.insert(id, name); return record();
        }
        if (tag == 10) return node(Node::Null);
        if (tag == 9) { auto n = node(Node::Ref); n->number = integer(); return n; }
        if (tag == 6) { int id = integer(); auto n = node(Node::String); n->text = string(); add(id, n); return n; }
        if (tag == 8) return primitive(byte());
        if (tag == 1) {
            int id = integer(), mid = integer();
            if (!meta_.contains(mid)) fail("Missing IR class metadata.");
            return object(id, meta_.value(mid));
        }
        if (tag == 4 || tag == 5) {
            int id = integer(); Meta m; m.name = string(); int count = length(integer());
            if (count > 32) fail("Unsupported IR class metadata.");
            for (int i = 0; i < count; ++i) m.names.append(string());
            QVector<quint8> kinds; for (int i = 0; i < count; ++i) kinds.append(byte());
            for (auto kind : kinds) m.types.append(typeInfo(kind));
            if (tag == 5 && !libraries_.contains(integer())) fail("Missing IR library metadata.");
            if (meta_.contains(id)) fail("Duplicate IR class metadata.");
            if (!(m.name.startsWith("IRConfigurator.Data.MenuFile`1[[IRConfigurator.Data.ProntoMenuItem,") || m.name == itemClass || m.name.startsWith("System.Collections.Generic.List`1[[")))
                fail("Unsupported class in IR file.");
            meta_.insert(id, m); return object(id, m);
        }
        if (tag == 15) {
            int id = integer(), count = integer(); auto type = byte();
            if (count < 0 || count > 65536) fail("IR file exceeds supported limits.");
            if (type != 2 || count > data_.size() - pos_) fail("Unsupported IR byte array.");
            auto n = node(Node::Bytes); n->bytes = data_.mid(pos_, count); pos_ += count; add(id, n); return n;
        }
        if (tag == 7 || tag == 16 || tag == 17) {
            int id = integer(), count = 0;
            if (tag == 7) {
                if (byte() != 0 || integer() != 1) fail("Unsupported multidimensional IR array.");
                count = length(integer()); auto t = typeInfo(byte());
                if (t.type != 4 && t.type != 2 && t.type != 1) fail("Unsupported IR array element type.");
            } else count = length(integer());
            auto n = node(Node::Array); add(id, n); n->values = arrayValues(count); return n;
        }
        fail("Unsupported record in IR file.");
    }
public:
    explicit Reader(const QByteArray &data) : data_(data) {}
    Value root() {
        if (data_.size() > maxFile) fail("IR file exceeds supported limits.");
        if (byte() != 0) fail("Not an RnIR3 file.");
        int id = integer(); integer(); if (integer() != 1 || integer() != 0) fail("Unsupported IR stream version.");
        while (pos_ < data_.size() && quint8(data_[pos_]) != 11) record();
        if (byte() != 11 || pos_ != data_.size() || !objects_.contains(id)) fail("Incomplete IR file.");
        return objects_.value(id);
    }
    Value resolve(Value v) const {
        for (int i = 0; v && v->kind == Node::Ref; ++i) {
            if (i > 128 || !objects_.contains(v->number)) fail("Missing IR object reference.");
            v = objects_.value(v->number);
        }
        if (!v) fail("Missing IR value.");
        return v;
    }
    Value field(Value v, const QString &name) const {
        v = resolve(v); if (v->kind != Node::Object || !v->fields.contains(name)) fail("Missing IR menu field.");
        return resolve(v->fields.value(name));
    }
    int number(Value v) const { v = resolve(v); if (v->kind != Node::Number) fail("Invalid IR numeric field."); return v->number; }
    QVector<Value> list(Value v) const {
        v = resolve(v);
        if (v->kind != Node::Object || !v->text.startsWith("System.Collections.Generic.List`1[[IRConfigurator.Data.ProntoMenuItem,") || v->fields.size() != 3 || !v->fields.contains("_version")) fail("Invalid IR menu list.");
        int size = number(field(v, "_size")); auto a = field(v, "_items");
        if (a->kind != Node::Array || size < 0 || size > a->values.size()) fail("Invalid IR menu list.");
        return a->values.mid(0, size);
    }
    QByteArray bytes(Value v) const {
        v = resolve(v); if (v->kind == Node::Null) return {};
        if (v->kind == Node::Bytes) return v->bytes;
        if (v->kind != Node::Object || !v->text.startsWith("System.Collections.Generic.List`1[[System.Byte,") || v->fields.size() != 3 || !v->fields.contains("_version")) fail("Invalid IR code list.");
        int size = number(field(v, "_size")); auto a = field(v, "_items");
        if (a->kind != Node::Bytes || size < 0 || size > a->bytes.size()) fail("Invalid IR code list.");
        return a->bytes.left(size);
    }
};

class Writer {
    QByteArray data_;
    int next_ = 3;
    QVector<std::function<void()>> pending_;
    void byte(int b) { data_.append(char(b)); }
    void integer(int n) { auto v = qToLittleEndian<qint32>(n); data_.append(reinterpret_cast<const char *>(&v), 4); }
    void string(const QString &s) { auto utf8 = s.toUtf8(); quint32 n = utf8.size(); while (n >= 128) { byte((n & 127) | 128); n >>= 7; } byte(n); data_.append(utf8); }
    void reference(std::function<void(int)> write) {
        int id = next_++; byte(9); integer(id);
        pending_.append([write=std::move(write), id] { write(id); });
    }
    void typeInfo(const TypeInfo &t) {
        if (t.type == 0 || t.type == 7) byte(t.primitive);
        else if (t.type == 3) string(t.name);
        else if (t.type == 4) { string(t.name); integer(2); }
    }
    void klass(int id, const QString &name, const QStringList &names, const QVector<TypeInfo> &types, bool system = false) {
        byte(system ? 4 : 5); integer(id); string(name); integer(names.size());
        for (const auto &n : names) string(n);
        for (const auto &t : types) byte(t.type);
        for (const auto &t : types) typeInfo(t);
        if (!system) integer(2);
    }
    void byteList(int id, const QByteArray &b) {
        klass(id, byteListClass, {"_items", "_size", "_version"}, {{7, 2, {}, 0}, {0, 8, {}, 0}, {0, 8, {}, 0}}, true);
        reference([this,b](int arrayId) { byte(15); integer(arrayId); integer(b.size()); byte(2); data_.append(b); });
        integer(b.size()); integer(0);
    }
    void item(int id, const Item &i) {
        klass(id, itemClass, {"Flags", "Parent", "TextIndex", "Name", "IRCode0", "IRCode1"},
              {{0, 2, {}, 0}, {0, 2, {}, 0}, {0, 2, {}, 0}, {1, 0, {}, 0}, {3, 0, byteListClass, 0}, {3, 0, byteListClass, 0}});
        byte(i.flags); byte(i.parent); byte(i.textIndex);
        byte(6); integer(next_++); string(i.name);
        reference([this,b=i.codes[0]](int listId) { byteList(listId,b); });
        reference([this,b=i.codes[1]](int listId) { byteList(listId,b); });
    }
public:
    QByteArray write(const Document &d) {
        byte(0); integer(1); integer(-1); integer(1); integer(0);
        byte(12); integer(2); string(assembly);
        klass(1, menuClass, {"Version", "MenuList", "ShortcutIndex"}, {{0, 2, {}, 0}, {3, 0, itemListClass, 0}, {3, 0, byteListClass, 0}});
        byte(4);
        // Use deferred object references, matching BinaryFormatter's dependency
        // ordering. MenuFile's deserialization constructor accesses its lists.
        reference([this,items=d.items](int listId) {
            klass(listId, itemListClass, {"_items", "_size", "_version"}, {{4, 0, itemClass + "[]", 2}, {0, 8, {}, 0}, {0, 8, {}, 0}}, true);
            reference([this,items](int arrayId) {
                byte(7); integer(arrayId); byte(0); integer(1); integer(items.size()); byte(4); string(itemClass); integer(2);
                for (const auto &i : items) reference([this,i](int itemId) { item(itemId,i); });
            });
            integer(items.size()); integer(0);
        });
        reference([this,b=d.shortcuts](int listId) { byteList(listId,b); });
        for (int i=0; i<pending_.size(); ++i) { auto write = pending_[i]; write(); }
        byte(11); return data_;
    }
};
}

Pronto Pronto::parse(const QString &text) {
    auto tokens = text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (tokens.size() < 4 || tokens.size() > 131076) fail("Enter a complete Pronto Hex code.");
    Pronto p;
    for (const auto &t : tokens) {
        if (!QRegularExpression("^[0-9a-fA-F]{4}$").match(t).hasMatch()) fail("Each Pronto word must contain four hexadecimal digits.");
        p.words.append(t.toUShort(nullptr, 16));
    }
    if (p.words[0] != 0) fail("Only modulated raw Pronto codes beginning with 0000 are supported.");
    if (!p.words[1]) fail("Pronto carrier frequency must not be zero.");
    if (tokens.size() != 4 + 2 * (int(p.words[2]) + int(p.words[3])) || p.pairs() < 2) fail("Pronto pulse count does not match its header.");
    for (int i = 4; i < p.words.size(); ++i) if (!p.words[i]) fail("Pronto pulse durations must not be zero.");
    return p;
}
QString Pronto::text() const {
    QStringList out; for (auto w : words) out.append(QString("%1").arg(w, 4, 16, QChar('0')).toUpper());
    return out.join(' ');
}
double Pronto::frequency() const { return 1000000.0 / (words.at(1) * 0.241246); }
int Pronto::pairs() const { return words.size() < 4 ? 0 : int(words[2]) + int(words[3]); }
QByteArray Pronto::toRnet() const {
    parse(text());
    if (words[2] && words[3]) fail("Separate Pronto introduction and repeat sequences cannot be converted to R-net yet.");
    int middle = pairs() - 2;
    if (middle > 255) fail("This code has too many pulse pairs for R-net format 2.");
    int divisor = std::lround(words[1] * 0.241246 / 0.25);
    if (divisor < 1 || divisor > 65535) fail("Carrier frequency is outside the supported R-net range.");
    QByteArray out; out.append(char(2));
    auto appendWord = [&](quint16 v) { auto le = qToLittleEndian(v); out.append(reinterpret_cast<const char *>(&le), 2); };
    appendWord(divisor); out.append(char(middle));
    appendWord(words[4]); appendWord(words[5]); appendWord(words[words.size()-2]); appendWord(words.last());
    for (int i = 6; i < words.size()-2; ++i) {
        if (words[i] > 255) fail("An internal pulse exceeds the R-net format 2 range.");
        out.append(char(words[i]));
    }
    return out;
}
QString Pronto::fromRnet(const QByteArray &code) {
    if (code.size() < 12 || quint8(code[0]) != 2 || code.size() != 12 + 2 * quint8(code[3])) return {};
    auto read = [&](int i) { return qFromLittleEndian<quint16>(code.constData()+i); };
    if (!read(1)) return {};
    int divisor = std::lround(read(1) * 0.25 / 0.241246);
    if (divisor < 1 || divisor > 65535) return {};
    Pronto p; p.words = {0, quint16(divisor), quint16(2 + quint8(code[3])), 0, read(4), read(6)};
    for (int i = 12; i < code.size(); ++i) p.words.append(quint8(code[i]));
    p.words.append(read(8)); p.words.append(read(10));
    try { if (parse(p.text()).toRnet() != code) return {}; } catch (...) { return {}; }
    return p.text();
}
void Document::validate() const {
    if (items.size() > 255 || shortcuts.size() != 8) fail("Invalid IR menu size or shortcut table.");
    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        if (item.name.trimmed().isEmpty() || item.name.size() > 256) fail("Enter a non-empty name of at most 256 characters.");
        if (item.parent > i || (item.parent && !(items[item.parent-1].folder() || items[item.parent-1].macro()))) fail("Invalid parent reference in IR menu.");
        if (item.codes[0].size() > 65536 || item.codes[1].size() > 65536) fail("IR code exceeds the supported size.");
    }
    for (char c : shortcuts) if (quint8(c) != 255 && quint8(c) >= items.size()) fail("Invalid shortcut reference in IR menu.");
}
void Document::setCode(int index, int slot, const QByteArray &code) {
    if (index < 0 || index >= items.size() || slot < 0 || slot > 1 || items[index].folder() || items[index].macro()) fail("Select an IR command first.");
    items[index].codes[slot] = code;
    if (items[index].codes[0].isEmpty() && items[index].codes[1].isEmpty()) items[index].flags &= ~quint8(0x28);
    else items[index].flags = (items[index].flags & ~quint8(0x08)) | 0x20;
}
int Document::addFolder(const QString &name, int parent) {
    if (items.size() >= 255 || parent >= items.size() || parent < -1 || (parent >= 0 && !items[parent].folder())) fail("Cannot add an IR device here.");
    Item i; i.name = name; i.flags = 0x40; i.parent = quint8(parent+1); items.append(i); return items.size()-1;
}
int Document::addCommand(const QString &name, int parent) {
    if (items.size() >= 255 || parent < 0 || parent >= items.size() || !items[parent].folder()) fail("Select a device or folder first.");
    Item i; i.name = name; i.parent = quint8(parent+1); items.append(i); return items.size()-1;
}
void Document::remove(int index) {
    validate();
    if (index < 0 || index >= items.size()) fail("Select an IR menu entry first.");
    QVector<bool> removed(items.size(), false); QVector<int> map(items.size(), -1);
    for (int i = 0; i < items.size(); ++i) removed[i] = i == index || (items[i].parent && removed[items[i].parent-1]);
    QVector<Item> remaining;
    for (int i = 0; i < items.size(); ++i) if (!removed[i]) { map[i] = remaining.size(); remaining.append(items[i]); }
    for (auto &i : remaining) if (i.parent) i.parent = quint8(map[i.parent-1]+1);
    for (char &c : shortcuts) if (quint8(c) != 255) c = map[quint8(c)] < 0 ? char(255) : char(map[quint8(c)]);
    items = remaining; validate();
}
QByteArray Document::rnir3() const { validate(); return Writer().write(*this); }
Document Document::fromRnir3(const QByteArray &data) {
    Reader r(data); auto root = r.root();
    if (root->kind != Node::Object || !root->text.startsWith("IRConfigurator.Data.MenuFile`1[[IRConfigurator.Data.ProntoMenuItem,")) fail("Not a supported Pronto RnIR3 menu.");
    if (r.number(r.field(root, "Version")) != 4) fail("Only RnIR3 version 4 menus are supported.");
    if (root->fields.size() != 3) fail("Unsupported extra IR menu fields.");
    Document d; d.shortcuts = r.bytes(r.field(root, "ShortcutIndex"));
    for (auto value : r.list(r.field(root, "MenuList"))) {
        value = r.resolve(value);
        if (value->kind != Node::Object || value->text != itemClass || value->fields.size() != 6) fail("Unsupported IR menu item.");
        auto byteField = [&](const char *field) { int n = r.number(r.field(value, field)); if (n < 0 || n > 255) fail("Invalid IR numeric field."); return quint8(n); };
        Item i; i.flags = byteField("Flags"); i.parent = byteField("Parent"); i.textIndex = byteField("TextIndex");
        auto name = r.field(value, "Name"); if (name->kind != Node::String) fail("Invalid IR item name."); i.name = name->text;
        i.codes[0] = r.bytes(r.field(value, "IRCode0")); i.codes[1] = r.bytes(r.field(value, "IRCode1")); d.items.append(i);
    }
    d.validate(); return d;
}
QByteArray Document::json() const {
    validate(); QJsonArray entries;
    for (const auto &i : items) entries.append(QJsonObject{{"name", i.name}, {"flags", i.flags}, {"parent", i.parent}, {"textIndex", i.textIndex}, {"code0", QString::fromLatin1(i.codes[0].toHex())}, {"code1", QString::fromLatin1(i.codes[1].toHex())}});
    return QJsonDocument(QJsonObject{{"format", "qt_iredit"}, {"version", 1}, {"shortcuts", QString::fromLatin1(shortcuts.toHex())}, {"items", entries}}).toJson();
}
Document Document::fromJson(const QByteArray &data) {
    QJsonParseError error; auto json = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) fail("Invalid IR project JSON.");
    auto root = json.object(); if (root["format"] != "qt_iredit" || root["version"].toInt(-1) != 1 || !root["items"].isArray()) fail("Unsupported IR project format.");
    auto bytes = [](const QJsonValue &v) { auto s = v.toString(); if (!v.isString() || s.size()%2 || !QRegularExpression("^[0-9a-fA-F]*$").match(s).hasMatch()) fail("Invalid hexadecimal code in IR project."); return QByteArray::fromHex(s.toLatin1()); };
    Document d; d.shortcuts = bytes(root["shortcuts"]);
    for (const auto &v : root["items"].toArray()) {
        if (!v.isObject()) fail("Invalid IR project entry.");
        auto o = v.toObject(); Item i;
        i.name = o["name"].toString();
        auto number = [&](const char *key) { int n = o[key].toInt(-1); if (n < 0 || n > 255 || o[key].toDouble(-1) != n) fail("Invalid IR project entry."); return quint8(n); };
        i.flags = number("flags"); i.parent = number("parent"); i.textIndex = number("textIndex"); i.codes[0] = bytes(o["code0"]); i.codes[1] = bytes(o["code1"]); d.items.append(i);
    }
    d.validate(); return d;
}
Document Document::load(const QString &file) {
    QFile f(file); if (!f.open(QIODevice::ReadOnly)) throw std::runtime_error(f.errorString().toStdString());
    if (f.size() > maxFile) fail("IR file exceeds supported limits.");
    auto data = f.readAll(); return data.trimmed().startsWith('{') ? fromJson(data) : fromRnir3(data);
}
void Document::save(const QString &file) const {
    auto bytes = QFileInfo(file).suffix().compare("RnIR3", Qt::CaseInsensitive) == 0 ? rnir3() : json();
    if (bytes.size() > maxFile) fail("IR file exceeds supported limits.");
    QSaveFile out(file); if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit()) throw std::runtime_error(out.errorString().toStdString());
}
}
