// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <memory>
#include <QColor>
#include <QPushButton>
#include <QWidget>

namespace Ui {
class ConfigureUi;
}

class ConfigureUi : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureUi(QWidget* parent = nullptr);
    ~ConfigureUi() override;

    void InitializeLanguageComboBox();
    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

private slots:
    void OnLanguageChanged(int index);

signals:
    void LanguageChanged(const QString& locale);

private:
    void SetAccentButtonColor(const QColor& color);
    void SetBackgroundButtonColor(QPushButton* button, QColor& target, const QColor& color);

    QColor home_accent;
    QColor home_bg_top;    // invalida = automatica
    std::unique_ptr<Ui::ConfigureUi> ui;
};
