#pragma once
#include "INetProbe.h"
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QHostInfo>
#include <QPointer>

class TcpConnectProbe : public INetProbe {
    Q_OBJECT
public:
    explicit TcpConnectProbe(QObject* parent = nullptr);
    ~TcpConnectProbe() override;

    void start(const QString& host, quint16 port, int timeoutMs) override;
    void abort() override;

private:
    void reset_();
    void finish_(const ProbeResult& r);

    QTcpSocket socket_;
    QTimer timeout_;
    QElapsedTimer elapsed_;
    QElapsedTimer dnsElapsed_;

    QString host_;
    quint16 port_ = 0;
    int timeoutMs_ = 3000;
    QString ip_;
    bool active_ = false;
};
