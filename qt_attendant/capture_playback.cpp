#include "capture_playback.hpp"
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <optional>

namespace attendant {
CapturePlayback readCapture(const QString &path)
{
    CapturePlayback result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = file.errorString();
        return result;
    }
    QTextStream input(&file);
    const QRegularExpression syntax(R"(^\(([0-9]+\.[0-9]+)\)\s+(\S+)\s+([0-9a-fA-F]{3}|[0-9a-fA-F]{8})#((?:[0-9a-fA-F]{2}){0,8}|R[0-8]?)$)");
    const QRegularExpression verbose(R"(^\s*\(([0-9]+\.[0-9]+)\)\s+(\S+)\s+([0-9a-fA-F]{3}|[0-9a-fA-F]{8})\s+\[([0-8])\]\s*(.*)$)");
    std::optional<double> start;
    QString serial;
    int owner = -1, requestedOwner = -1, requestedMode = -1;
    bool requestAcknowledged = false;
    QString interface, phase = "?", selection = "?";
    int slot = -1, x = 0, y = 0;
    while (!input.atEnd()) {
        const auto line = input.readLine();
        auto match = syntax.match(line);
        if (!match.hasMatch()) {
            const auto human = verbose.match(line);
            if (human.hasMatch()) {
                QString payload;
                const auto tail = human.captured(5);
                const int length = human.captured(4).toInt();
                if (tail.startsWith("remote request")) {
                    payload = "R" + QString::number(length);
                } else {
                    const auto parts = tail.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
                    for (int i = 0; i < length && i < parts.size(); ++i) payload += parts[i];
                    if (payload.size() != length * 2) payload = "INVALID";
                }
                match = syntax.match("(" + human.captured(1) + ") " + human.captured(2) +
                                     " " + human.captured(3) + "#" + payload);
            }
        }
        if (!match.hasMatch()) {
            result.error = QStringLiteral("Invalid candump record at line %1").arg(result.frames + 1);
            return result;
        }
        const double stamp = match.captured(1).toDouble();
        if (!start) {
            start = stamp;
            interface = match.captured(2);
        }
        const double elapsed = stamp - *start;
        if (elapsed < result.duration || match.captured(2) != interface) {
            result.error = QStringLiteral("Expected one chronological interface at line %1").arg(result.frames + 1);
            return result;
        }
        result.duration = elapsed;
        ++result.frames;
        const auto id = match.captured(3).toUpper();
        const auto data = match.captured(4).toUpper();
        if (id == "00C" && data.isEmpty()) {
            phase = "Start"; slot = -1; selection = "?"; x = y = 0;
            owner = requestedOwner = requestedMode = -1; serial.clear();
            requestAcknowledged = false;
        } else if (id == "000" && data.startsWith("R")) {
            phase = "Off"; slot = -1; selection = "?"; x = y = 0;
            owner = requestedOwner = requestedMode = -1;
            requestAcknowledged = false;
        } else if (id == "1FB00000" && data.size() == 16) {
            serial = data;
            phase = "Enumerated";
        } else if ((id == "060" || id == "061") && data.size() == 8 &&
                   (data.startsWith("00") || data.startsWith("40"))) {
            x = y = 0;
            owner = -1;
            requestAcknowledged = false;
            requestedOwner = -1;
            if (data.startsWith("00")) {
                requestedOwner = id == "060" ? 0 : 1;
                selection = data.mid(2, 2);
                requestedMode = selection.toInt(nullptr, 16) & 15;
                phase = "Request";
            } else {
                phase = "Suspend";
            }
        } else if (id == "063" && data.size() == 8 && requestedOwner >= 0) {
            const auto bytes = QByteArray::fromHex(data.toLatin1());
            if (static_cast<unsigned char>(bytes[1]) != requestedMode) continue;
            if (static_cast<unsigned char>(bytes[0]) == 0x10 + requestedOwner &&
                bytes[2] == 0 && bytes[3] == 2)
                requestAcknowledged = true;
            else if (static_cast<unsigned char>(bytes[0]) == 0x80 &&
                     bytes[2] == 0 && static_cast<unsigned char>(bytes[3]) == 0x80 &&
                     requestAcknowledged) {
                owner = requestedOwner;
                phase = "Confirmed";
                requestedOwner = -1;
            } else continue;
        } else if (id.size() == 8 && (id.toUInt(nullptr, 16) & 0xfffff0ffu) == 0x02000000u && data.size() == 4 && !data.startsWith('R')) {
            const int nextSlot = id.mid(5, 1).toInt(nullptr, 16);
            const auto bytes = QByteArray::fromHex(data.toLatin1());
            const int nextX = qint8(bytes[0]), nextY = qint8(bytes[1]);
            if (slot == nextSlot && nextX == x && nextY == y) continue;
            phase = "Joystick"; slot = nextSlot; x = nextX; y = nextY;
        } else {
            continue;
        }
        result.events.append(QVariantMap{{"time", elapsed}, {"phase", phase}, {"slot", slot},
                                         {"x", x}, {"y", y}, {"selection", selection},
                                         {"owner", owner}, {"serial", serial}});
    }
    if (result.events.isEmpty()) result.error = QStringLiteral("No supported playback events");
    return result;
}
}
