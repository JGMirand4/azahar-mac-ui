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
    QColor bg_top{0xD9, 0xE9, 0xFA};
    QColor bg_mid{0xEB, 0xF1, 0xF8};
    QColor bg_bottom{0xF3, 0xEE, 0xE9};
    QColor blob_a{0x9C, 0xCB, 0xF5};
    QColor blob_b{0xF6, 0xD2, 0xB8};
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
        t.bg_top = QColor::fromHslF(h, 0.70f, 0.92f);
        t.bg_mid = QColor::fromHslF(h, 0.45f, 0.955f);
        t.blob_a = QColor::fromHslF(h, 0.85f, 0.80f);
        t.shadow = QColor::fromHslF(h, 0.50f, 0.30f);
        return t;
    }
};

} // namespace Home
