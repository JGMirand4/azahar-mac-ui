// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <QDateTime>
#include "citra_qt/home/gamepad.h"

namespace Home {

#ifndef __APPLE__
PadState ReadPadState() {
    return {}; // sem suporte a controle na tela inicial nesta plataforma
}
#endif

GamepadNav::GamepadNav(QObject* parent) : QObject(parent) {
    timer.setInterval(16);
    connect(&timer, &QTimer::timeout, this, [this] { Poll(); });
    timer.start();
}

void GamepadNav::Poll() {
    const PadState state = ReadPadState();
    if (state.connected != connected) {
        connected = state.connected;
        emit ConnectionChanged(connected);
    }
    layout = state.layout;

    const bool now[ButtonCount] = {state.up, state.down, state.left, state.right, state.a, state.b,
                                   state.x,  state.y,    state.l,    state.r,     state.start, state.lt,
                                   state.rt,      state.select, state.l3};
    const qint64 time = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < ButtonCount; ++i) {
        const bool direction = i <= Right;
        if (now[i] && !held[i]) {
            held[i] = true;
            next_repeat_ms[i] = time + 380; // pausa antes de comecar a repetir
            emit ButtonPressed(i);
        } else if (now[i] && held[i] && direction && time >= next_repeat_ms[i]) {
            next_repeat_ms[i] = time + 110;
            emit ButtonPressed(i);
        } else if (!now[i]) {
            held[i] = false;
        }
    }
}

} // namespace Home
