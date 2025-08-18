#pragma once
#include <QWidget>
#include "core/MonitorController.h"

class QLabel; class QLineEdit; class QSpinBox; class QComboBox;
class QPlainTextEdit; class QPushButton;

class NetworkMonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit NetworkMonitorWidget(QWidget* parent = nullptr);

private:
    void setupUi_();
    void wireSignals_();
    void appendLog_(const QString& line);

    // UI widgets
    QLabel* statusLabel_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QSpinBox* intervalSpin_ = nullptr;
    QPushButton* startStopBtn_ = nullptr;
    QPushButton* checkOnceBtn_ = nullptr;
    QPushButton* saveLogBtn_ = nullptr;
    QLabel* latencyLabel_ = nullptr;
    QLabel* dnsLabel_ = nullptr;
    QLabel* httpLabel_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QPlainTextEdit* log_ = nullptr;

    // Core controller
    MonitorController controller_;
};