// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include "citra_qt/home/cover_fetcher.h"

namespace Home {

namespace {

constexpr qint64 kFailureTtlSeconds = 14 * 24 * 3600; // nao tenta de novo por 14 dias

quint32 ReadU32(const QByteArray& data, int offset) {
    if (offset < 0 || offset + 4 > data.size()) {
        return 0;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(data.constData()) + offset;
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

quint64 AlignUp(quint64 value, quint64 alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

/// Le o codigo do produto ("CTR-P-AREE") do cabecalho NCCH que comeca em `offset`.
QString ReadNcchGameId(QFile& file, qint64 offset) {
    if (!file.seek(offset)) {
        return {};
    }
    const QByteArray header = file.read(0x200);
    if (header.size() < 0x160 || header.mid(0x100, 4) != "NCCH") {
        return {};
    }
    const QByteArray code = header.mid(0x150, 16);
    const QList<QByteArray> parts = QByteArray(code.constData()).split('-'); // corta no NUL
    if (parts.size() < 3 || parts[0] != "CTR" || parts[2].size() < 4) {
        return {};
    }
    const QString id = QString::fromLatin1(parts[2].left(4));
    static const QRegularExpression valid(QStringLiteral("^[A-Z0-9]{4}$"));
    return valid.match(id).hasMatch() ? id : QString();
}

} // namespace

CoverFetcher::CoverFetcher(const QString& cache_dir_, QObject* parent)
    : QObject(parent), cache_dir(cache_dir_) {
    network = new QNetworkAccessManager(this);
    QDir().mkpath(cache_dir);
}

QString CoverFetcher::ReadGameId(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray head = file.read(0x200);
    if (head.size() < 0x200) {
        return {};
    }
    if (head.mid(0x100, 4) == "NCSD") {
        // Primeira particao (programa): deslocamento em unidades de 0x200 bytes.
        const quint32 units = ReadU32(head, 0x120);
        return units == 0 ? QString() : ReadNcchGameId(file, static_cast<qint64>(units) * 0x200);
    }
    if (head.mid(0x100, 4) == "NCCH") {
        return ReadNcchGameId(file, 0);
    }
    if (ReadU32(head, 0) == 0x2020) {
        // CIA: cabecalho, certificados, ticket, TMD e meta, cada um alinhado em 64 bytes.
        const quint64 content = AlignUp(0x2020, 64) + AlignUp(ReadU32(head, 8), 64) +
                                AlignUp(ReadU32(head, 0xC), 64) + AlignUp(ReadU32(head, 0x10), 64) +
                                AlignUp(ReadU32(head, 0x14), 64);
        return ReadNcchGameId(file, static_cast<qint64>(content));
    }
    return {};
}

QStringList CoverFetcher::RegionsFor(const QString& id) {
    // A 4a letra do codigo indica a regiao; na Europa a capa pode estar em varios idiomas.
    switch (id.at(3).toLatin1()) {
    case 'E':
        return {QStringLiteral("US"), QStringLiteral("EN")};
    case 'P':
        return {QStringLiteral("EN"), QStringLiteral("DE"), QStringLiteral("FR"),
                QStringLiteral("ES"), QStringLiteral("IT"), QStringLiteral("NL")};
    case 'J':
        return {QStringLiteral("JA")};
    case 'K':
        return {QStringLiteral("KO")};
    case 'C':
        return {QStringLiteral("ZH")};
    case 'D':
        return {QStringLiteral("DE"), QStringLiteral("EN")};
    case 'F':
        return {QStringLiteral("FR"), QStringLiteral("EN")};
    case 'S':
        return {QStringLiteral("ES"), QStringLiteral("EN")};
    case 'I':
        return {QStringLiteral("IT"), QStringLiteral("EN")};
    case 'H':
        return {QStringLiteral("NL"), QStringLiteral("EN")};
    default:
        return {QStringLiteral("US"), QStringLiteral("EN"), QStringLiteral("JA")};
    }
}

QString CoverFetcher::CachedCover(const QString& id) const {
    const QString path = cache_dir + QLatin1Char('/') + id + QStringLiteral(".jpg");
    return QFileInfo::exists(path) ? path : QString();
}

void CoverFetcher::Request(const QString& id) {
    if (id.size() != 4 || !CachedCover(id).isEmpty() || id == current_id ||
        std::find(queue.begin(), queue.end(), id) != queue.end()) {
        return;
    }
    const QFileInfo failed(cache_dir + QLatin1Char('/') + id + QStringLiteral(".none"));
    if (failed.exists() &&
        failed.lastModified().secsTo(QDateTime::currentDateTime()) < kFailureTtlSeconds) {
        return;
    }
    queue.push_back(id);
    if (reply == nullptr) {
        StartNext();
    }
}

void CoverFetcher::StartNext() {
    if (queue.empty()) {
        current_id.clear();
        return;
    }
    current_id = queue.front();
    queue.pop_front();
    current_regions = RegionsFor(current_id);
    current_region = 0;

    const auto fetch = [this](auto&& self) -> void {
        if (current_region >= current_regions.size()) {
            Finish(false);
            return;
        }
        const QUrl url(QStringLiteral("https://art.gametdb.com/3ds/cover/%1/%2.jpg")
                           .arg(current_regions[current_region], current_id));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Azahar-Home/1.0"));
        request.setTransferTimeout(12000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        reply = network->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, self] {
            QNetworkReply* finished = reply;
            reply = nullptr;
            finished->deleteLater();
            const QByteArray data = finished->error() == QNetworkReply::NoError
                                        ? finished->readAll()
                                        : QByteArray();
            QImage image;
            if (!data.isEmpty() && image.loadFromData(data) && image.width() >= 32) {
                QFile out(cache_dir + QLatin1Char('/') + current_id + QStringLiteral(".jpg"));
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(data);
                    out.close();
                    Finish(true);
                    return;
                }
            }
            ++current_region;
            self(self);
        });
    };
    fetch(fetch);
}

void CoverFetcher::Finish(bool success) {
    const QString id = current_id;
    if (!success) {
        // Marca a falha para nao ficar tentando a cada abertura do app.
        QFile marker(cache_dir + QLatin1Char('/') + id + QStringLiteral(".none"));
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write("not found");
        }
    }
    current_id.clear();
    if (success) {
        emit CoverReady(id);
    }
    StartNext();
}

} // namespace Home
