#include "HttpHeadProbe.h"
#include <QUrl>
#include <QNetworkRequest>

HttpHeadProbe::HttpHeadProbe(QObject* parent)
        : INetProbe(parent) {
    timeout_.setSingleShot(true);
    QObject::connect(&timeout_, &QTimer::timeout, this, [this]{
        if (!active_) return;
        if (reply_) {
            QObject::disconnect(reply_, nullptr, this, nullptr);
            reply_->abort();
            reply_->deleteLater();
            reply_ = nullptr;
        }
        ProbeResult r;
        r.status = ProbeResult::Status::Timeout;
        r.message = tr("Timeout");
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });
}

HttpHeadProbe::~HttpHeadProbe() {
    abort();
}

void HttpHeadProbe::start(const QString& host, quint16 port, int timeoutMs) {
    if (active_) return;
    host_ = host;
    port_ = port;
    timeoutMs_ = timeoutMs;

    active_ = true;
    ip_.clear();
    elapsed_.restart();
    dnsElapsed_.restart();

    timeout_.start(timeoutMs_);

    QPointer<HttpHeadProbe> self(this);
    QHostInfo::lookupHost(host_, this, [this, self](const QHostInfo& info){
        if (!self || !active_) return;
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            ProbeResult r;
            r.status = ProbeResult::Status::DnsFail;
            r.message = info.errorString();
            r.dnsMs = -1;
            finish_(r);
            return;
        }
        QHostAddress addr;
        for (const auto& a : info.addresses()) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
        }
        if (addr.isNull()) addr = info.addresses().first();
        ip_ = addr.toString();
        emit progressDnsResolved(dnsElapsed_.elapsed(), ip_);

        const bool useHttps = (port_ == 443);
        const QString scheme = useHttps ? "https" : "http";
        const QUrl url(QString("%1://%2:%3/").arg(scheme, host_, QString::number(port_)));

        QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
        req.setHeader(QNetworkRequest::UserAgentHeader, "SimpleQtNetMon/1.0");

        if (reply_) {
            QObject::disconnect(reply_, nullptr, this, nullptr);
            reply_->abort();
            reply_->deleteLater();
            reply_ = nullptr;
        }

        reply_ = nam_.sendCustomRequest(req, "HEAD");

        QObject::connect(reply_, &QNetworkReply::finished, this, [this]{
            if (!active_) { if (reply_) { reply_->deleteLater(); reply_ = nullptr; } return; }
            ProbeResult r;
            r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
            r.ip = ip_;

            const int code = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            r.httpCode = code > 0 ? std::optional<int>(code) : std::nullopt;

            if (reply_->error() == QNetworkReply::NoError && code >= 100) {
                r.status = ProbeResult::Status::Up;
                r.latencyMs = elapsed_.elapsed();
                r.message = QString();
            } else {
                r.status = ProbeResult::Status::Down;
                r.message = reply_->errorString();
            }

            reply_->deleteLater();
            reply_ = nullptr;
            finish_(r);
        });
    });
}

void HttpHeadProbe::abort() {
    if (!active_) return;
    active_ = false;
    timeout_.stop();
    if (reply_) { reply_->abort(); reply_->deleteLater(); reply_ = nullptr; }
}

void HttpHeadProbe::finish_(const ProbeResult& r) {
    active_ = false;
    timeout_.stop();
    if (reply_) { reply_->abort(); reply_->deleteLater(); reply_ = nullptr; }
    emit finished(r);
}