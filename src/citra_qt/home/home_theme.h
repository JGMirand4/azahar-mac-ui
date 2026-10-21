// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <algorithm>
#include <QColor>

namespace Home {

/// Paleta da tela inicial. Para trocar as cores da interface basta mudar a cor de destaque:
/// Theme::FromAccent(QColor("#E8604C")) deriva fundo, brilhos e sombras a partir dela.
struct Theme {
    QColor accent{0x0F, 0x6F, 0xD0};
    QColor accent_light{0x2B, 0x9B, 0xF0};
    QColor text{0x14, 0x20, 0x2E};
    QColor text_body{0x3E, 0x4A, 0x59};
    QColor text_muted{0x4F, 0x5B, 0x6B};
    QColor ink{0x2B, 0x36, 0x44};
    QColor bg{0xE3, 0xEE, 0xFA};     // fundo solido
    QColor on_bg{0x14, 0x20, 0x2E};  // texto desenhado direto sobre o fundo
    QColor on_bg_muted{0x4F, 0x5B, 0x6B};
    QColor shadow{0x1F, 0x4A, 0x78};
    QColor battery_ok{0x1F, 0xA4, 0x63};
    QColor favorite{0xF5, 0xB3, 0x01};

    static Theme FromAccent(const QColor& accent) {
        Theme t;
        float h = 0.0f;
        float s = 0.0f;
        float l = 0.0f;
        float a = 0.0f;
        accent.getHslF(&h, &s, &l, &a);
        if (h < 0.0f) {
            h = 0.58f; // cor sem matiz (cinza): usa um azul neutro
        }
        t.accent = accent;
        t.accent_light = QColor::fromHslF(h, std::min(1.0f, s), std::min(0.9f, l + 0.12f));
        t.bg = QColor::fromHslF(h, 0.60f, 0.92f);
        t.AdaptToBackground();
        t.shadow = QColor::fromHslF(h, 0.50f, 0.30f);
        return t;
    }

    /// Como FromAccent, mas com a cor de fundo (solida) escolhida pelo usuario.
    /// Cor invalida mantem o valor automatico derivado da cor de destaque.
    static Theme FromColors(const QColor& accent, const QColor& background) {
        Theme t = FromAccent(accent);
        if (background.isValid()) {
            t.bg = background;
            t.AdaptToBackground();
        }
        return t;
    }

    /// Texto sobre o fundo: escuro em fundos claros e claro em fundos escuros, para nunca
    /// ficar com a mesma cor do fundo.
    void AdaptToBackground() {
        const double luma = 0.299 * bg.redF() + 0.587 * bg.greenF() + 0.114 * bg.blueF();
        if (luma < 0.55) {
            on_bg = QColor(0xF5, 0xF8, 0xFC);
            on_bg_muted = QColor(0xC9, 0xD3, 0xDF);
        } else {
            on_bg = QColor(0x14, 0x20, 0x2E);
            on_bg_muted = QColor(0x4F, 0x5B, 0x6B);
        }
    }

    static QColor Mix(const QColor& a, const QColor& b, float amount) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                                a.greenF() + (b.greenF() - a.greenF()) * amount,
                                a.blueF() + (b.blueF() - a.blueF()) * amount);
    }

    /// Versao mais viva e clara da cor, usada nas manchas de luz do fundo.
    static QColor Tint(const QColor& c, float lightness) {
        float h = 0.0f;
        float s = 0.0f;
        float l = 0.0f;
        float a = 0.0f;
        c.getHslF(&h, &s, &l, &a);
        if (h < 0.0f) {
            return QColor::fromHslF(0.0f, 0.0f, std::max(l, lightness));
        }
        return QColor::fromHslF(h, std::min(1.0f, std::max(s, 0.6f)), std::max(l, lightness));
    }
};

} // namespace Home
