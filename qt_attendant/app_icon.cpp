#include "app_icon.hpp"
#include <QPainter>

namespace attendant {
QIcon applicationIcon() {
    QIcon icon;
    for(int size:{16,24,32,48,64,128,256}) {
        QPixmap image(size,size);image.fill(Qt::transparent);
        QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
        p.scale(size/64.0,size/64.0);
        p.setPen(Qt::NoPen);p.setBrush(QColor("#245eaa"));p.drawEllipse(2,2,60,60);
        p.setPen(QPen(Qt::white,5,Qt::SolidLine,Qt::RoundCap));
        p.drawLine(17,47,47,47);p.drawLine(32,44,40,25);
        p.setPen(Qt::NoPen);p.setBrush(Qt::white);p.drawEllipse(32,14,17,17);
        p.end();icon.addPixmap(image);
    }
    return icon;
}
}
