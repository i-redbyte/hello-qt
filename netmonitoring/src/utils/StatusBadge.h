#pragma once

#include <QLabel>
#include <QColor>

inline void setStatusBadge(QLabel *label, const QString &text, const QColor &color) {
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
    label->setMargin(8);
    label->setStyleSheet(QString(
            "QLabel {"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "  font-weight: 600;"
            "  background: %1;"
            "  color: white;"
            "}")
                                 .arg(color.name()));
}