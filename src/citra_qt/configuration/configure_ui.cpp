// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <QColorDialog>
#include <QDirIterator>
#include "citra_qt/configuration/configure_ui.h"
#include "citra_qt/uisettings.h"
#include "ui_configure_ui.h"

ConfigureUi::ConfigureUi(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureUi>()) {
    ui->setupUi(this);
    InitializeLanguageComboBox();

    for (const auto& theme : UISettings::themes) {
        ui->theme_combobox->addItem(QString::fromUtf8(theme.first),
                                    QString::fromUtf8(theme.second));
    }

    SetConfiguration();

    connect(ui->home_accent_button, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(home_accent, this, tr("Accent color"));
        if (color.isValid()) {
            SetAccentButtonColor(color);
        }
    });
    connect(ui->home_bg_top_button, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            home_bg_top.isValid() ? home_bg_top : QColor(0xD9, 0xE9, 0xFA), this,
            tr("Background color (start)"));
        if (color.isValid()) {
            SetBackgroundButtonColor(ui->home_bg_top_button, home_bg_top, color);
        }
    });
    connect(ui->home_bg_bottom_button, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            home_bg_bottom.isValid() ? home_bg_bottom : QColor(0xF3, 0xEE, 0xE9), this,
            tr("Background color (end)"));
        if (color.isValid()) {
            SetBackgroundButtonColor(ui->home_bg_bottom_button, home_bg_bottom, color);
        }
    });
    connect(ui->home_bg_reset_button, &QPushButton::clicked, this, [this] {
        SetBackgroundButtonColor(ui->home_bg_top_button, home_bg_top, QColor());
        SetBackgroundButtonColor(ui->home_bg_bottom_button, home_bg_bottom, QColor());
    });
}

void ConfigureUi::SetBackgroundButtonColor(QPushButton* button, QColor& target,
                                           const QColor& color) {
    target = color;
    if (color.isValid()) {
        button->setStyleSheet(
            QStringLiteral("QPushButton{ background: %1; border: 1px solid #808080; "
                           "border-radius: 4px; }")
                .arg(color.name()));
        button->setText(QString());
    } else {
        button->setStyleSheet(QString());
        button->setText(tr("Auto"));
    }
}

void ConfigureUi::SetAccentButtonColor(const QColor& color) {
    home_accent = color;
    ui->home_accent_button->setStyleSheet(
        QStringLiteral("QPushButton{ background: %1; border: 1px solid #808080; "
                       "border-radius: 4px; }")
            .arg(color.name()));
}

ConfigureUi::~ConfigureUi() = default;

void ConfigureUi::InitializeLanguageComboBox() {
    ui->language_combobox->addItem(tr("<System>"), QString{});
    ui->language_combobox->addItem(QStringLiteral("English"), QStringLiteral("en"));
    QDirIterator it(QStringLiteral(":/languages"), QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString locale = it.next();
        locale.truncate(locale.lastIndexOf(QLatin1Char{'.'}));
        locale.remove(0, locale.lastIndexOf(QLatin1Char{'/'}) + 1);
        if (locale.startsWith(QStringLiteral("qtbase"))) {
            // The Qt Base QM translation files are lumped in with ours,
            // so don't show them in the language list!
            continue;
        }
        QString lang = QLocale::languageToString(QLocale(locale).language());
        const QString country = QLocale::territoryToString(QLocale(locale).territory());
        if (locale == QString::fromStdString("ca_ES_valencia")) {
            // QT returns "Catalan" for the "Valencian" dialect, so we have to change the
            // language name manually here.
            lang = QString::fromStdString("Valencian");
        }
        ui->language_combobox->addItem(QStringLiteral("%1 (%2)").arg(lang, country), locale);
    }

    // Unlike other configuration changes, interface language changes need to be reflected on the
    // interface immediately. This is done by passing a signal to the main window, and then
    // retranslating when passing back.
    connect(ui->language_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureUi::OnLanguageChanged);
}

void ConfigureUi::SetConfiguration() {
    ui->theme_combobox->setCurrentIndex(ui->theme_combobox->findData(UISettings::values.theme));
    ui->language_combobox->setCurrentIndex(
        // findData returns -1 if nothing found; Use <System> in this case (index 0).
        std::max(0, ui->language_combobox->findData(UISettings::values.language)));
    ui->icon_size_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_icon_size.GetValue()));
    ui->row_1_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_row_1.GetValue()));
    ui->row_2_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_row_2.GetValue()) + 1);
    ui->toggle_hide_no_icon->setChecked(UISettings::values.game_list_hide_no_icon.GetValue());
    ui->toggle_single_line_mode->setChecked(
        UISettings::values.game_list_single_line_mode.GetValue());
    ui->toggle_home_view->setChecked(UISettings::values.game_list_use_home_view.GetValue());
    SetAccentButtonColor(
        QColor(QString::fromStdString(UISettings::values.home_accent_color.GetValue())));
    const auto bg_color = [](const std::string& value) {
        return value.empty() ? QColor() : QColor(QString::fromStdString(value));
    };
    SetBackgroundButtonColor(ui->home_bg_top_button, home_bg_top,
                             bg_color(UISettings::values.home_bg_top.GetValue()));
    SetBackgroundButtonColor(ui->home_bg_bottom_button, home_bg_bottom,
                             bg_color(UISettings::values.home_bg_bottom.GetValue()));
    ui->show_advanced_frametime_info->setChecked(
        UISettings::values.show_advanced_frametime_info.GetValue());
}

void ConfigureUi::ApplyConfiguration() {
    UISettings::values.theme =
        ui->theme_combobox->itemData(ui->theme_combobox->currentIndex()).toString();
    UISettings::values.game_list_icon_size =
        static_cast<UISettings::GameListIconSize>(ui->icon_size_combobox->currentIndex());
    UISettings::values.game_list_row_1 =
        static_cast<UISettings::GameListText>(ui->row_1_text_combobox->currentIndex());
    UISettings::values.game_list_row_2 =
        static_cast<UISettings::GameListText>(ui->row_2_text_combobox->currentIndex() - 1);
    UISettings::values.game_list_hide_no_icon = ui->toggle_hide_no_icon->isChecked();
    UISettings::values.game_list_single_line_mode = ui->toggle_single_line_mode->isChecked();
    UISettings::values.game_list_use_home_view = ui->toggle_home_view->isChecked();
    UISettings::values.home_accent_color = home_accent.name().toStdString();
    UISettings::values.home_bg_top = home_bg_top.isValid() ? home_bg_top.name().toStdString() : "";
    UISettings::values.home_bg_bottom =
        home_bg_bottom.isValid() ? home_bg_bottom.name().toStdString() : "";
    UISettings::values.show_advanced_frametime_info = ui->show_advanced_frametime_info->isChecked();
}

void ConfigureUi::OnLanguageChanged(int index) {
    if (index == -1)
        return;

    emit LanguageChanged(ui->language_combobox->itemData(index).toString());
}

void ConfigureUi::RetranslateUI() {
    ui->retranslateUi(this);
}
