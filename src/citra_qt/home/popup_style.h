// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QMenu>
#include <QObject>
#include <QWidget>

namespace Home {

/// Deixa menus e listas suspensas com cantos realmente arredondados: o QSS so arredonda a borda
/// se a janela do popup for translucida e sem moldura.
class PopupRounder : public QObject {
public:
    explicit PopupRounder(QObject* parent = nullptr) : QObject(parent) {}

    void SetEnabled(bool value) {
        enabled = value;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (enabled && event->type() == QEvent::Polish && watched->isWidgetType()) {
            auto* widget = static_cast<QWidget*>(watched);
            const bool is_menu = qobject_cast<QMenu*>(widget) != nullptr;
            const bool is_combo_popup =
                qobject_cast<QFrame*>(widget) != nullptr &&
                widget->inherits("QComboBoxPrivateContainer");
            if ((is_menu || is_combo_popup) && widget->isWindow()) {
                widget->setWindowFlag(Qt::FramelessWindowHint, true);
                widget->setWindowFlag(Qt::NoDropShadowWindowHint, true);
                widget->setAttribute(Qt::WA_TranslucentBackground, true);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    bool enabled = false;
};

} // namespace Home
