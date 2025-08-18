#pragma once
#include "INetProbe.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QHostInfo>

class HttpHeadProbe : public INetProbe {
    Q_OBJECT
public:
    explicit HttpHeadProbe(QObject* parent = nullptr);
    ~HttpHeadProbe() override;

    void start(const QString& host, quint16 port, int timeoutMs) override;
    void abort() override;

private:
    void finish_(const ProbeResult& r);

    QNetworkAccessManager nam_;
    QPointer<QNetworkReply> reply_ = nullptr;
    QTimer timeout_;
    QElapsedTimer elapsed_;
    QElapsedTimer dnsElapsed_;

    QString host_;
    quint16 port_ = 0;
    int timeoutMs_ = 3000;
    QString ip_;
    bool active_ = false;
};