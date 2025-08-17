#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QTcpSocket>
#include <QSslSocket>
#include <QElapsedTimer>
#include <QDateTime>
#include <QPalette>
#include <QComboBox>
#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QPointer>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QVector>

static void setStatusBadge(QLabel* label, const QString& text, const QColor& color) {
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Simple Network Monitor (Qt)");
    window.resize(700, 460);

    auto *statusLabel = new QLabel("—");
    setStatusBadge(statusLabel, "Ожидание", QColor("#777"));

    auto *modeCombo = new QComboBox();
    modeCombo->addItem("TCP connect");
    modeCombo->addItem("HTTP HEAD");

    auto *hostEdit = new QLineEdit("red-byte.ru");
    hostEdit->setPlaceholderText("Хост, например: red-byte.ru");

    auto *portSpin = new QSpinBox();
    portSpin->setRange(1, 65535);
    portSpin->setValue(80);

    auto *intervalSpin = new QSpinBox();
    intervalSpin->setRange(1, 3600);
    intervalSpin->setValue(5);
    intervalSpin->setSuffix(" сек");

    auto *startStopBtn = new QPushButton("Старт");
    auto *checkOnceBtn = new QPushButton("Проверить сейчас");
    auto *saveLogBtn   = new QPushButton("Сохранить лог…");

    auto *latencyLabel = new QLabel("Задержка: —");
    auto *dnsLabel     = new QLabel("DNS: —");
    auto *httpLabel    = new QLabel("HTTP: —");
    auto *statsLabel   = new QLabel("Статистика: —");

    auto *log = new QPlainTextEdit();
    log->setReadOnly(true);
    log->setPlaceholderText("Лог проверок будет здесь…");

    auto *top = new QHBoxLayout();
    top->addWidget(new QLabel("Режим:"));
    top->addWidget(modeCombo);
    top->addSpacing(6);
    top->addWidget(new QLabel("Хост:"));
    top->addWidget(hostEdit, 2);
    top->addWidget(new QLabel("Порт:"));
    top->addWidget(portSpin);
    top->addSpacing(8);
    top->addWidget(new QLabel("Интервал:"));
    top->addWidget(intervalSpin);
    top->addSpacing(8);
    top->addWidget(startStopBtn);

    auto *mid = new QHBoxLayout();
    mid->addWidget(statusLabel, 0);
    mid->addSpacing(8);
    mid->addWidget(latencyLabel, 1);
    mid->addSpacing(8);
    mid->addWidget(dnsLabel, 1);
    mid->addSpacing(8);
    mid->addWidget(httpLabel, 1);
    mid->addSpacing(8);
    mid->addWidget(checkOnceBtn);

    auto *bottom = new QHBoxLayout();
    bottom->addWidget(statsLabel, 1);
    bottom->addStretch();
    bottom->addWidget(saveLogBtn);

    auto *layout = new QVBoxLayout(&window);
    layout->addLayout(top);
    layout->addLayout(mid);
    layout->addLayout(bottom);
    layout->addWidget(log, 1);

    auto *socket = new QTcpSocket(&window);
    auto *intervalTimer = new QTimer(&window);
    auto *attemptTimeout = new QTimer(&window);
    attemptTimeout->setSingleShot(true);
    const int singleAttemptMs = 3000;

    auto *elapsed = new QElapsedTimer();
    auto *dnsElapsed = new QElapsedTimer();

    auto *nam = new QNetworkAccessManager(&window);
    QPointer<QNetworkReply> currentReply = nullptr;

    QVector<qint64> samples;
    const int MaxSamples = 50;

    bool probing = false;

    auto nowStr = [] {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    };

    auto logLine = [&](const QString &line) {
        log->appendPlainText(QString("[%1] %2").arg(nowStr(), line));
    };

    auto updateStats = [&] {
        if (samples.isEmpty()) {
            statsLabel->setText("Статистика: —");
            return;
        }
        qint64 mn = samples[0], mx = samples[0], sum = 0;
        for (qint64 v : samples) { if (v < mn) mn = v; if (v > mx) mx = v; sum += v; }
        const qint64 avg = sum / samples.size();
        statsLabel->setText(QString("Статистика: min %1 мс / avg %2 мс / max %3 мс (n=%4)").arg(mn).arg(avg).arg(mx).arg(samples.size()));
    };

    auto recordSample = [&](qint64 ms) {
        if (ms < 0) return;
        if (samples.size() >= MaxSamples) samples.erase(samples.begin());
        samples.push_back(ms);
        updateStats();
    };

    auto resetInfoLabels = [&]{
        latencyLabel->setText("Задержка: …");
        dnsLabel->setText("DNS: …");
        httpLabel->setText("HTTP: —");
    };

    auto endProbeAs = [&](const QString& text, const QColor& color, qint64 msOrMinus1) {
        setStatusBadge(statusLabel, text, color);
        if (msOrMinus1 >= 0) {
            latencyLabel->setText(QString("Задержка: %1 мс").arg(msOrMinus1));
            recordSample(msOrMinus1);
        } else {
            latencyLabel->setText("Задержка: —");
        }
        probing = false;
        attemptTimeout->stop();
        socket->abort();
        if (currentReply) {
            QObject::disconnect(currentReply, nullptr, nullptr, nullptr);
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
        }
    };

    auto runCheckTCP = [&](const QString& host, quint16 port, const QList<QHostAddress>& addrs) {
        QHostAddress addr;
        for (const auto& a : addrs) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
        }
        if (addr.isNull() && !addrs.isEmpty()) addr = addrs.first();
        if (addr.isNull()) {
            endProbeAs("DNS FAIL", QColor("#ef6c00"), -1);
            logLine("DNS: адреса не найдены.");
            return;
        }
        dnsLabel->setText(QString("DNS: %1 мс (%2)").arg(dnsElapsed->elapsed()).arg(addr.toString()));
        socket->abort();
        socket->connectToHost(addr, port);
        logLine(QString("TCP %1:%2… (IP %3)").arg(host).arg(port).arg(addr.toString()));
    };

    auto runCheckHTTP = [&](const QString& host, quint16 port) {
        const bool useHttps = (port == 443);
        const QString scheme = useHttps ? "https" : "http";
        const QUrl url(QString("%1://%2:%3/").arg(scheme, host, QString::number(port)));
        QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
        req.setHeader(QNetworkRequest::UserAgentHeader, "SimpleQtNetMon/1.0");
        if (currentReply) {
            QObject::disconnect(currentReply, nullptr, nullptr, nullptr);
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
        }
        logLine(QString("HTTP HEAD %1").arg(url.toString()));
        currentReply = nam->sendCustomRequest(req, "HEAD");
        QObject::connect(currentReply, &QNetworkReply::finished, [&, reply = QPointer<QNetworkReply>(currentReply)]() {
            if (!reply) return;
            bool wasProbing = probing;
            qint64 ms = elapsed->elapsed();
            int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            httpLabel->setText(QString("HTTP: %1").arg(code ? QString::number(code) : "—"));
            if (!wasProbing) { reply->deleteLater(); if (currentReply == reply) currentReply = nullptr; return; }
            if (reply->error() == QNetworkReply::NoError && code >= 100) {
                endProbeAs(QString("UP (%1 мс)").arg(ms), QColor("#2e7d32"), ms);
                logLine(QString("HTTP OK %1, %2 мс").arg(code).arg(ms));
            } else {
                endProbeAs("DOWN", QColor("#c62828"), -1);
                logLine(QString("HTTP ERROR: %1").arg(reply->errorString()));
            }
            reply->deleteLater();
            if (currentReply == reply) currentReply = nullptr;
        });
    };

    auto runCheck = [&] {
        if (probing) return;
        const QString host = hostEdit->text().trimmed();
        const quint16 port = static_cast<quint16>(portSpin->value());
        if (host.isEmpty()) {
            logLine("Хост пуст. Укажите host и повторите.");
            return;
        }
        probing = true;
        elapsed->restart();
        dnsElapsed->restart();
        resetInfoLabels();
        setStatusBadge(statusLabel, "Проверка…", QColor("#777"));
        attemptTimeout->start(singleAttemptMs);
        QHostInfo::lookupHost(host, &window, [&, host, port](const QHostInfo& info) {
            if (!probing) return;
            if (info.error() != QHostInfo::NoError) {
                dnsLabel->setText("DNS: FAIL");
                endProbeAs("DNS FAIL", QColor("#ef6c00"), -1);
                logLine(QString("DNS ошибка: %1").arg(info.errorString()));
                return;
            }
            if (modeCombo->currentIndex() == 0) {
                runCheckTCP(host, port, info.addresses());
            } else {
                dnsLabel->setText(QString("DNS: %1 мс").arg(dnsElapsed->elapsed()));
                runCheckHTTP(host, port);
            }
        });
        logLine(QString("Проверка %1 %2:%3…").arg(modeCombo->currentIndex() == 0 ? "TCP" : "HTTP").arg(host).arg(port));
    };

    QObject::connect(socket, &QTcpSocket::connected, [&] {
        if (!probing) return;
        qint64 ms = elapsed->elapsed();
        endProbeAs(QString("UP (%1 мс)").arg(ms), QColor("#2e7d32"), ms);
        logLine(QString("TCP ДОСТУПЕН, задержка %1 мс").arg(ms));
    });

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QObject::connect(socket, &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError) {
        if (!probing) return;
        endProbeAs("DOWN", QColor("#c62828"), -1);
        logLine(QString("TCP НЕДОСТУПЕН: %1").arg(socket->errorString()));
    });
#else
    QObject::connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), [&]{
        if (!probing) return;
        endProbeAs("DOWN", QColor("#c62828"), -1);
        logLine(QString("TCP НЕДОСТУПЕН: %1").arg(socket->errorString()));
    });
#endif

    QObject::connect(attemptTimeout, &QTimer::timeout, [&] {
        if (!probing) return;
        endProbeAs("TIMEOUT", QColor("#ef6c00"), -1);
        logLine(QString("ТАЙМАУТ (%1 мс)").arg(singleAttemptMs));
    });

    QObject::connect(checkOnceBtn, &QPushButton::clicked, runCheck);

    QObject::connect(startStopBtn, &QPushButton::clicked, [&] {
        if (intervalTimer->isActive()) {
            intervalTimer->stop();
            startStopBtn->setText("Старт");
            logLine("Автомониторинг остановлен.");
        } else {
            intervalTimer->start(intervalSpin->value() * 1000);
            startStopBtn->setText("Стоп");
            logLine(QString("Автомониторинг запущен (каждые %1 сек).").arg(intervalSpin->value()));
            runCheck();
        }
    });

    QObject::connect(intervalSpin, qOverload<int>(&QSpinBox::valueChanged), [&](int s) {
        if (intervalTimer->isActive()) {
            intervalTimer->start(s * 1000);
            logLine(QString("Интервал изменён на %1 сек.").arg(s));
        }
    });

    QObject::connect(intervalTimer, &QTimer::timeout, runCheck);

    QObject::connect(saveLogBtn, &QPushButton::clicked, [&]{
        const QString path = QFileDialog::getSaveFileName(&window, "Сохранить лог", "netlog.txt", "Текстовые файлы (*.txt);;Все файлы (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logLine(QString("Не удалось сохранить лог: %1").arg(f.errorString()));
            return;
        }
        QTextStream ts(&f);
        ts << log->toPlainText();
        f.close();
        logLine(QString("Лог сохранён: %1").arg(path));
    });

    window.show();
    return app.exec();
}
