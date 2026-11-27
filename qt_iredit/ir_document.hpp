#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>

namespace iredit {
struct Pronto {
    QVector<quint16> words;
    QString text() const;
    double frequency() const;
    int pairs() const;
    static Pronto parse(const QString &text);
    QByteArray toRnet() const;
    static QString fromRnet(const QByteArray &code);
};
struct Item {
    QString name;
    quint8 flags = 0;
    quint8 parent = 0;
    quint8 textIndex = 255;
    QByteArray codes[2];
    bool folder() const { return flags & 0x40; }
    bool macro() const { return flags & 0x10; }
};
struct Document {
    QVector<Item> items;
    QByteArray shortcuts = QByteArray(8, char(255));
    void validate() const;
    void setCode(int item, int slot, const QByteArray &code);
    int addFolder(const QString &name, int parent = -1);
    int addCommand(const QString &name, int parent);
    void remove(int item);
    QByteArray rnir3() const;
    static Document fromRnir3(const QByteArray &data);
    QByteArray json() const;
    static Document fromJson(const QByteArray &data);
    static Document load(const QString &file);
    void save(const QString &file) const;
};
}
