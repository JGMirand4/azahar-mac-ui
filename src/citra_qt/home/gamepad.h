// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QObject>
#include <QTimer>

namespace Home {

/// Estado instantaneo do controle (todos os controles conectados combinados).
struct PadState {
    bool connected = false;
    int layout = 0; // 0 = Xbox/generico, 1 = PlayStation
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false; // botao de baixo
    bool b = false; // botao da direita
    bool x = false; // botao da esquerda
    bool y = false; // botao de cima
    bool l = false;
    bool r = false;
    bool lt = false; // gatilhos (L2 / R2)
    bool rt = false;
    bool select = false; // botao Options/Share/View
    bool l3 = false;     // clique do analogico esquerdo
    bool start = false;
};

/// Le o estado do controle pelo sistema operacional. No macOS usa o framework GameController
/// (Xbox, PlayStation, Switch Pro e controles MFi); nas outras plataformas nao faz nada.
PadState ReadPadState();

/// Transforma o estado do controle em "botao apertado": detecta o momento do aperto e repete
/// as direcoes enquanto seguradas, para navegar pela tela inicial.
class GamepadNav : public QObject {
    Q_OBJECT

public:
    enum Button { Up, Down, Left, Right, A, B, X, Y, L, R, Start, L2, R2, Select, L3, ButtonCount };

    explicit GamepadNav(QObject* parent = nullptr);

    bool IsConnected() const {
        return connected;
    }
    int Layout() const {
        return layout;
    }

signals:
    void ButtonPressed(int button);
    void ConnectionChanged(bool connected);

private:
    void Poll();

    QTimer timer;
    bool connected = false;
    int layout = 0;
    bool held[ButtonCount] = {};
    qint64 next_repeat_ms[ButtonCount] = {};
};

} // namespace Home
