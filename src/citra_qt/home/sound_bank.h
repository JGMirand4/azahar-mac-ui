// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <array>
#include <memory>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QSoundEffect;

namespace Home {

/// Sons curtos de navegacao (tique, confirmar, voltar...). Sao sintetizados no codigo e gravados
/// em arquivos .wav na pasta de cache na primeira vez, entao nao ha arquivos de audio no projeto.
class SoundBank : public QObject {
    Q_OBJECT

public:
    enum class Sound { Move, Confirm, Back, Favorite, Launch, Startup, Count };

    explicit SoundBank(const QString& dir, QObject* parent = nullptr);
    ~SoundBank() override;

    void SetEnabled(bool value) {
        enabled = value;
    }
    void Play(Sound sound);

private:
    void EnsureLoaded();

    QString dir;
    bool enabled = true;
    bool loaded = false;
    std::array<QSoundEffect*, static_cast<size_t>(Sound::Count)> effects{};
    QElapsedTimer last_move;
};

} // namespace Home
