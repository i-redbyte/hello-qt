#include "NetworkMonitorWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QComboBox>
#include "utils/StatusBadge.h"

static QString nowStr() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

NetworkMonitorWidget::NetworkMonitorWidget(QWidget* parent)
        : QWidget(parent) {
    setupUi_();
    wireSignals_();
}

void NetworkMonitorWidget::setupUi_() {
    statusLabel_ = new QLabel("—", this);
    setStatusBadge(statusLabel_, tr("Ожидание"), QColor("#777"));

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem(tr("TCP connect"));
    modeCombo_->addItem(tr("HTTP HEAD"));

    hostEdit_ = new QLineEdit(QStringLiteral("red-byte.ru"), this);
    hostEdit_->setPlaceholderText(tr("Хост, например: red-byte.ru"));

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(80);

    intervalSpin_ = new QSpinBox(this);
    intervalSpin_->setRange(1, 3600);
    intervalSpin_->setValue(5);
    intervalSpin_->setSuffix(tr(" сек"));

    startStopBtn_ = new QPushButton(tr("Старт"), this);
    checkOnceBtn_ = new QPushButton(tr("Проверить сейчас"), this);
    saveLogBtn_   = new QPushButton(tr("Сохранить лог…"), this);

    latencyLabel_ = new QLabel(tr("Задержка: —"), this);
    dnsLabel_     = new QLabel(tr("DNS: —"), this);
    httpLabel_    = new QLabel(tr("HTTP: —"), this);
    statsLabel_   = new QLabel(tr("Статистика: —"), this);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setPlaceholderText(tr("Лог проверок будет здесь…"));

    auto *top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Режим:"), this));
    top->addWidget(modeCombo_);
    top->addSpacing(6);
    top->addWidget(new QLabel(tr("Хост:"), this));
    top->addWidget(hostEdit_, 2);
    top->addWidget(new QLabel(tr("Порт:"), this));
    top->addWidget(portSpin_);
    top->addSpacing(8);
    top->addWidget(new QLabel(tr("Интервал:"), this));
    top->addWidget(intervalSpin_);
    top->addSpacing(8);
    top->addWidget(startStopBtn_);

    auto *mid = new QHBoxLayout();
    mid->addWidget(statusLabel_, 0);
    mid->addSpacing(8);
    mid->addWidget(latencyLabel_, 1);
    mid->addSpacing(8);
    mid->addWidget(dnsLabel_, 1);
    mid->addSpacing(8);
    mid->addWidget(httpLabel_, 1);
    mid->addSpacing(8);
    mid->addWidget(checkOnceBtn_);

    auto *bottom = new QHBoxLayout();
    bottom->addWidget(statsLabel_, 1);
    bottom->addStretch();
    bottom->addWidget(saveLogBtn_);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(mid);
    layout->addLayout(bottom);
    layout->addWidget(log_, 1);
}

void NetworkMonitorWidget::wireSignals_() {
    QObject::connect(modeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx){
        controller_.setMode(idx == 0 ? MonitorController::Mode::TcpConnect
                                     : MonitorController::Mode::HttpHead);
    });

    QObject::connect(hostEdit_, &QLineEdit::textEdited, this, [this](const QString& t){
        controller_.setTarget(t.trimmed(), static_cast<quint16>(portSpin_->value()));
    });

    QObject::connect(portSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int p){
        controller_.setTarget(hostEdit_->text().trimmed(), static_cast<quint16>(p));
    });

    QObject::connect(intervalSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int s){
        controller_.setIntervalSec(s);
        appendLog_(tr("Интервал изменён на %1 сек.").arg(s));
    });

    QObject::connect(checkOnceBtn_, &QPushButton::clicked, this, [this]{ controller_.checkOnce(); });

    QObject::connect(startStopBtn_, &QPushButton::clicked, this, [this]{
        if (controller_.isRunning()) {
            controller_.stop();
            startStopBtn_->setText(tr("Старт"));
            appendLog_(tr("Автомониторинг остановлен."));
        } else {
            controller_.setTarget(hostEdit_->text().trimmed(), static_cast<quint16>(portSpin_->value()));
            controller_.setIntervalSec(intervalSpin_->value());
            controller_.start();
            startStopBtn_->setText(tr("Стоп"));
            appendLog_(tr("Автомониторинг запущен (каждые %1 сек).")
                               .arg(intervalSpin_->value()));
        }
    });

    QObject::connect(saveLogBtn_, &QPushButton::clicked, this, [this]{
        const QString path = QFileDialog::getSaveFileName(this, tr("Сохранить лог"),
                                                          QStringLiteral("netlog.txt"),
                                                          tr("Текстовые файлы (*.txt);;Все файлы (*)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            appendLog_(tr("Не удалось сохранить лог: %1").arg(f.errorString()));
            return;
        }
        QTextStream ts(&f);
        ts << log_->toPlainText();
        f.close();
        appendLog_(tr("Лог сохранён: %1").arg(path));
    });

    QObject::connect(&controller_, &MonitorController::probeStarted, this, [this]{
        setStatusBadge(statusLabel_, tr("Проверка…"), QColor("#777"));
        latencyLabel_->setText(tr("Задержка: …"));
        dnsLabel_->setText(tr("DNS: …"));
        httpLabel_->setText(tr("HTTP: —"));

        const QString host = hostEdit_->text().trimmed();
        const quint16 port = static_cast<quint16>(portSpin_->value());
        appendLog_(tr("Проверка %1 %2:%3…")
                           .arg(modeCombo_->currentIndex() == 0 ? "TCP" : "HTTP")
                           .arg(host)
                           .arg(port));
    });

    QObject::connect(&controller_, &MonitorController::probeProgressDns, this,
                     [this](qint64 ms, const QString& ip){
                         dnsLabel_->setText(tr("DNS: %1 мс (%2)").arg(ms).arg(ip));
                     });

    QObject::connect(&controller_, &MonitorController::statsUpdated, this,
                     [this](qint64 mn, qint64 avg, qint64 mx, int n){
                         if (n <= 0) {
                             statsLabel_->setText(tr("Статистика: —"));
                         } else {
                             statsLabel_->setText(tr("Статистика: min %1 мс / avg %2 мс / max %3 мс (n=%4)")
                                                          .arg(mn).arg(avg).arg(mx).arg(n));
                         }
                     });

    QObject::connect(&controller_, &MonitorController::probeFinished, this,
                     [this](const ProbeResult& r){
                         switch (r.status) {
                             case ProbeResult::Status::Up:
                                 setStatusBadge(statusLabel_, tr("UP (%1 мс)").arg(r.latencyMs), QColor("#2e7d32"));
                                 latencyLabel_->setText(tr("Задержка: %1 мс").arg(r.latencyMs));
                                 if (r.httpCode) {
                                     httpLabel_->setText(tr("HTTP: %1").arg(*r.httpCode));
                                     appendLog_(tr("HTTP OK %1, %2 мс").arg(*r.httpCode).arg(r.latencyMs));
                                 } else {
                                     appendLog_(tr("TCP ДОСТУПЕН, задержка %1 мс").arg(r.latencyMs));
                                 }
                                 break;
                             case ProbeResult::Status::Down:
                                 setStatusBadge(statusLabel_, tr("DOWN"), QColor("#c62828"));
                                 latencyLabel_->setText(tr("Задержка: —"));
                                 if (r.httpCode) httpLabel_->setText(tr("HTTP: %1").arg(*r.httpCode));
                                 appendLog_(tr("НЕДОСТУПЕН: %1").arg(r.message));
                                 break;
                             case ProbeResult::Status::DnsFail:
                                 setStatusBadge(statusLabel_, tr("DNS FAIL"), QColor("#ef6c00"));
                                 latencyLabel_->setText(tr("Задержка: —"));
                                 dnsLabel_->setText(tr("DNS: FAIL"));
                                 appendLog_(tr("DNS ошибка: %1").arg(r.message));
                                 break;
                             case ProbeResult::Status::Timeout:
                                 setStatusBadge(statusLabel_, tr("TIMEOUT"), QColor("#ef6c00"));
                                 latencyLabel_->setText(tr("Задержка: —"));
                                 appendLog_(tr("ТАЙМАУТ (%1 мс)").arg(3000));
                                 break;
                             case ProbeResult::Status::Error:
                                 setStatusBadge(statusLabel_, tr("ERROR"), QColor("#c62828"));
                                 appendLog_(r.message.isEmpty() ? tr("Неизвестная ошибка") : r.message);
                                 break;
                         }
                     });

    controller_.setMode(MonitorController::Mode::TcpConnect);
    controller_.setTarget(hostEdit_->text().trimmed(), static_cast<quint16>(portSpin_->value()));
    controller_.setIntervalSec(intervalSpin_->value());
}

void NetworkMonitorWidget::appendLog_(const QString& line) {
    log_->appendPlainText(QString("[%1] %2").arg(nowStr(), line));
}