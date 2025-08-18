#pragma once
#include <QObject>
#include <QTimer>
#include <memory>
#include "StatsCalculator.h"
#include "INetProbe.h"

class MonitorController : public QObject {
    Q_OBJECT
public:
    enum class Mode { TcpConnect = 0, HttpHead = 1 };

    explicit MonitorController(QObject* parent = nullptr);

    void setMode(Mode m);
    void setTarget(QString host, quint16 port);
    void setIntervalSec(int sec);
    void setTimeoutMs(int ms);
    void setMaxSamples(int n);

    bool isRunning() const { return intervalTimer_.isActive(); }
    void start();
    void stop();
    void checkOnce();

    const StatsCalculator& stats() const { return stats_; }

    signals:
            void probeStarted();
    void probeProgressDns(qint64 dnsMs, const QString& ip);
    void probeFinished(const ProbeResult& result);
    void statsUpdated(qint64 minMs, qint64 avgMs, qint64 maxMs, int n);

private:
    void runProbe_();
    void onProbeFinished_(const ProbeResult& r);
    std::unique_ptr<INetProbe> makeProbe_() const;

    Mode mode_ = Mode::TcpConnect;
    QString host_ = QStringLiteral("red-byte.ru");
    quint16 port_ = 80;
    int timeoutMs_ = 3000;
    int intervalMs_ = 5000;

    QTimer intervalTimer_;
    bool probing_ = false;

    std::unique_ptr<INetProbe> probe_;
    StatsCalculator stats_;
};