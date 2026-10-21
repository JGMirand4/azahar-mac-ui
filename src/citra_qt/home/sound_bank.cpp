// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cmath>
#include <vector>
#include <QDir>
#include <QFile>
#include <QSoundEffect>
#include <QUrl>
#include "citra_qt/home/sound_bank.h"

namespace Home {

namespace {

constexpr int kSampleRate = 44100;

struct Note {
    double start; // segundos
    double length;
    double from_hz;
    double to_hz;
    double gain;
};

/// Sintetiza as notas (seno com envelope suave) em um arquivo WAV mono de 16 bits.
QByteArray Synthesize(const std::vector<Note>& notes) {
    double total = 0.0;
    for (const Note& n : notes) {
        total = std::max(total, n.start + n.length);
    }
    const int count = static_cast<int>((total + 0.05) * kSampleRate);
    std::vector<double> mix(count, 0.0);
    for (const Note& n : notes) {
        const int begin = static_cast<int>(n.start * kSampleRate);
        const int len = static_cast<int>(n.length * kSampleRate);
        double phase = 0.0;
        for (int i = 0; i < len && begin + i < count; ++i) {
            const double t = static_cast<double>(i) / len;
            const double hz = n.from_hz + (n.to_hz - n.from_hz) * t;
            phase += 2.0 * M_PI * hz / kSampleRate;
            // Ataque curto e queda exponencial: som "de vidro", sem estalos.
            const double attack = std::min(1.0, i / (0.004 * kSampleRate));
            const double env = attack * std::exp(-4.0 * t);
            mix[begin + i] += std::sin(phase) * env * n.gain;
        }
    }
    QByteArray pcm(count * 2, 0);
    for (int i = 0; i < count; ++i) {
        const auto v = static_cast<qint16>(std::clamp(mix[i], -1.0, 1.0) * 32000.0);
        pcm[i * 2] = static_cast<char>(v & 0xFF);
        pcm[i * 2 + 1] = static_cast<char>((v >> 8) & 0xFF);
    }

    QByteArray wav;
    const auto put32 = [&wav](quint32 v) {
        for (int i = 0; i < 4; ++i) {
            wav.append(static_cast<char>((v >> (8 * i)) & 0xFF));
        }
    };
    const auto put16 = [&wav](quint16 v) {
        wav.append(static_cast<char>(v & 0xFF));
        wav.append(static_cast<char>((v >> 8) & 0xFF));
    };
    wav.append("RIFF");
    put32(36 + pcm.size());
    wav.append("WAVEfmt ");
    put32(16);
    put16(1);
    put16(1);
    put32(kSampleRate);
    put32(kSampleRate * 2);
    put16(2);
    put16(16);
    wav.append("data");
    put32(pcm.size());
    wav.append(pcm);
    return wav;
}

} // namespace

SoundBank::SoundBank(const QString& dir_, QObject* parent) : QObject(parent), dir(dir_) {
    last_move.start();
}

SoundBank::~SoundBank() = default;

void SoundBank::EnsureLoaded() {
    if (loaded) {
        return;
    }
    loaded = true;
    QDir().mkpath(dir);

    struct Def {
        const char* name;
        std::vector<Note> notes;
        float volume;
    };
    const Def defs[] = {
        {"move", {{0.0, 0.05, 1500.0, 1300.0, 0.5}}, 0.22f},
        {"confirm", {{0.0, 0.09, 700.0, 700.0, 0.5}, {0.07, 0.16, 1050.0, 1050.0, 0.5}}, 0.35f},
        {"back", {{0.0, 0.09, 900.0, 900.0, 0.5}, {0.07, 0.14, 600.0, 600.0, 0.5}}, 0.3f},
        {"favorite",
         {{0.0, 0.08, 1100.0, 1100.0, 0.4}, {0.06, 0.08, 1400.0, 1400.0, 0.4},
          {0.12, 0.2, 1800.0, 1800.0, 0.4}},
         0.3f},
        {"launch", {{0.0, 0.32, 400.0, 1200.0, 0.45}, {0.18, 0.3, 800.0, 1600.0, 0.3}}, 0.35f},
        {"startup",
         {{0.0, 0.5, 523.0, 523.0, 0.3}, {0.12, 0.5, 659.0, 659.0, 0.3},
          {0.24, 0.6, 784.0, 784.0, 0.3}, {0.36, 0.7, 1047.0, 1047.0, 0.25}},
         0.3f},
    };
    static_assert(std::size(defs) == static_cast<size_t>(Sound::Count));
    for (size_t i = 0; i < std::size(defs); ++i) {
        // O sufixo da versao forca regerar os arquivos se os sons mudarem.
        const QString path =
            dir + QLatin1Char('/') + QString::fromLatin1(defs[i].name) + QStringLiteral("_v1.wav");
        if (!QFile::exists(path)) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly)) {
                continue;
            }
            file.write(Synthesize(defs[i].notes));
        }
        auto* effect = new QSoundEffect(this);
        effect->setSource(QUrl::fromLocalFile(path));
        effect->setVolume(defs[i].volume);
        effects[i] = effect;
    }
}

void SoundBank::Play(Sound sound) {
    if (!enabled) {
        return;
    }
    EnsureLoaded();
    if (sound == Sound::Move) {
        // Rolagem rapida nao pode virar uma metralhadora de tiques.
        if (last_move.elapsed() < 70) {
            return;
        }
        last_move.restart();
    }
    if (QSoundEffect* effect = effects[static_cast<size_t>(sound)]) {
        effect->play();
    }
}

} // namespace Home
