// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <deque>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

namespace Home {

/// Baixa as capas dos jogos (GameTDB) e guarda em cache na pasta do usuario.
/// O identificador usado e o codigo de 4 letras do jogo (ex.: "AREE"), lido do cabecalho NCCH.
class CoverFetcher : public QObject {
    Q_OBJECT

public:
    explicit CoverFetcher(const QString& cache_dir, QObject* parent = nullptr);

    /// Le o codigo de 4 letras do jogo direto do arquivo (.3ds/.cci/.cxi/.app/.cia).
    /// Retorna vazio se nao for possivel (formato desconhecido, arquivo criptografado...).
    static QString ReadGameId(const QString& path);

    /// Caminho do arquivo em cache; vazio se a capa ainda nao foi baixada.
    QString CachedCover(const QString& id) const;

    /// Coloca o jogo na fila de download (ignora se ja esta em cache ou se falhou ha pouco).
    void Request(const QString& id);

signals:
    /// Uma capa nova ficou disponivel em cache.
    void CoverReady(const QString& id);

private:
    void StartNext();
    void Finish(bool success);
    static QStringList RegionsFor(const QString& id);

    QString cache_dir;
    QNetworkAccessManager* network = nullptr;
    QNetworkReply* reply = nullptr;
    std::deque<QString> queue;
    QString current_id;
    QStringList current_regions;
    int current_region = 0;
};

} // namespace Home
