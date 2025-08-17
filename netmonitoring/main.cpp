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
#include <QElapsedTimer>
#include <QDateTime>
#include <QPalette>

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
    window.resize(520, 380);

    auto *statusLabel = new QLabel("—");
    setStatusBadge(statusLabel, "Ожидание", QColor("#777"));

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

    auto *latencyLabel = new QLabel("Задержка: —");
    auto *log = new QPlainTextEdit();
    log->setReadOnly(true);
    log->setPlaceholderText("Лог проверок будет здесь…");

    auto *top = new QHBoxLayout();
    top->addWidget(new QLabel("Хост:"));
    top->addWidget(hostEdit, /*stretch*/2);
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
    mid->addWidget(checkOnceBtn);

    auto *layout = new QVBoxLayout(&window);
    layout->addLayout(top);
    layout->addLayout(mid);
    layout->addWidget(log, /*stretch*/1);

    auto *socket = new QTcpSocket(&window);
    auto *intervalTimer = new QTimer(&window);
    auto *attemptTimeout = new QTimer(&window);
    attemptTimeout->setSingleShot(true);
    const int singleAttemptMs = 3000;
    auto *elapsed = new QElapsedTimer();

    bool probing = false;

    auto nowStr = [] {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    };

    auto logLine = [&](const QString &line) {
        log->appendPlainText(QString("[%1] %2").arg(nowStr(), line));
    };

    auto endProbeAs = [&](const QString& text, const QColor& color, qint64 msOrMinus1) {
        setStatusBadge(statusLabel, text, color);
        if (msOrMinus1 >= 0) {
            latencyLabel->setText(QString("Задержка: %1 мс").arg(msOrMinus1));
        } else {
            latencyLabel->setText("Задержка: —");
        }
        probing = false;
        attemptTimeout->stop();
        socket->abort();
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
        latencyLabel->setText("Задержка: …");
        setStatusBadge(statusLabel, "Проверка…", QColor("#777"));

        socket->abort();
        socket->connectToHost(host, port);
        attemptTimeout->start(singleAttemptMs);
        logLine(QString("Проверка TCP %1:%2…").arg(host).arg(port));
    };

    QObject::connect(socket, &QTcpSocket::connected, [&] {
        if (!probing) return;
        qint64 ms = elapsed->elapsed();
        endProbeAs(QString("UP (%1 мс)").arg(ms), QColor("#2e7d32"), ms);
        logLine(QString("ДОСТУПЕН, задержка %1 мс").arg(ms));
    });

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QObject::connect(socket, &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError) {
        if (!probing) return;
        qint64 ms = elapsed->elapsed();
        endProbeAs("DOWN", QColor("#c62828"), -1);
        logLine(QString("НЕДОСТУПЕН: %1 (через %2 мс)")
                .arg(socket->errorString(), QString::number(ms)));
    });
#else
    QObject::connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), [&]{
        if (!probing) return;
        qint64 ms = elapsed->elapsed();
        endProbeAs("DOWN", QColor("#c62828"), -1);
        logLine(QString("НЕДОСТУПЕН: %1 (через %2 мс)")
                        .arg(socket->errorString(), QString::number(ms)));
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

    window.show();
    return app.exec();
}
