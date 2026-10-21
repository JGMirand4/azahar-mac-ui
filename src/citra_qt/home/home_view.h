// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <vector>
#include <QLocale>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include "citra_qt/home/home_theme.h"

class QFrame;
class QLabel;
class QListView;
class QPushButton;
class QStandardItemModel;
class QTimer;
class QVBoxLayout;

namespace Home {

class BatteryWidget;
class HeroArt;
class LogoWidget;

/// Dados de um jogo exibidos na tela inicial. Preenchidos pela lista de jogos.
struct GameInfo {
    QString path;
    QString title;
    QString subtitle; // titulo longo ou nome do arquivo
    QPixmap icon;     // icone grande (48x48) do SMDH; nulo se nao houver
    QString region;
    QString size_text;
    QString compat_text;
    QString compat_key; // "0".."5" ou "99" (nao testado)
    QString play_time_text;
    qulonglong play_time_seconds = 0;
    qulonglong size_bytes = 0;
    qulonglong program_id = 0;
    qulonglong extdata_id = 0;
    quint32 media_type = 0;
    bool can_insert = false;
    bool favorite = false;
};

enum class SortMode {
    Name,
    PlayTime,
    Size,
};

/// Tela inicial inspirada no menu HOME do 3DS: barra superior, jogo em destaque,
/// grade de jogos e dock inferior. Nao depende do nucleo do emulador.
class HomeView : public QWidget {
    Q_OBJECT

public:
    explicit HomeView(QWidget* parent = nullptr);
    ~HomeView() override;

    /// Substitui todos os jogos exibidos.
    void SetGames(std::vector<GameInfo> games);
    void SetFavorite(qulonglong program_id, bool favorite);

    /// Filtra a grade. Retorna quantos jogos ficaram visiveis.
    int SetFilter(const QString& text);
    int GameCount() const;
    const GameInfo* GameAtRow(int row) const;
    const GameInfo* CurrentGame() const;

    /// Coloca o widget de busca (ja existente) dentro do painel da grade.
    void SetSearchWidget(QWidget* widget);
    /// Remove o widget de busca do painel (sem destrui-lo).
    void TakeSearchWidget();
    void SetSortMode(SortMode mode);

    /// Troca a cor de destaque e deriva o restante da paleta.
    void SetAccentColor(const QColor& accent);
    const Theme& GetTheme() const {
        return theme;
    }

    void FocusGrid();

    /// Idioma usado na data e na hora do topo.
    void SetLocale(const QLocale& locale);

signals:
    void GameActivated(const QString& path);
    void GameContextMenuRequested(const Home::GameInfo& info, const QPoint& global_pos);
    void FavoriteToggleRequested(qulonglong program_id);
    void SettingsRequested();
    void AddFolderRequested();
    void SearchRequested();
    void ClassicViewRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    friend class CardDelegate;

    void BuildUi();
    void RebuildStyleSheet();
    void ApplyLightPalette();
    void RefreshModel();
    void UpdateHero();
    void UpdateClock();
    void ApplySort();
    void ShowContextMenuForCurrent(const QPoint& global_pos);
    QString SortModeText() const;

    Theme theme;
    QLocale ui_locale = QLocale::system();
    std::vector<GameInfo> games;
    SortMode sort_mode = SortMode::Name;
    bool compact = false;
    QString filter_text;

    QStandardItemModel* model = nullptr;
    QListView* grid = nullptr;
    QVBoxLayout* grid_layout = nullptr;
    QWidget* search_widget = nullptr;

    LogoWidget* logo = nullptr;
    QLabel* date_label = nullptr;
    QLabel* time_label = nullptr;
    QFrame* battery_pill = nullptr;
    QLabel* battery_label = nullptr;
    BatteryWidget* battery = nullptr;
    QTimer* clock_timer = nullptr;

    QWidget* hero_panel = nullptr;
    HeroArt* hero_art = nullptr;
    QLabel* hero_compat = nullptr;
    QLabel* hero_region = nullptr;
    QLabel* hero_size = nullptr;
    QLabel* hero_title = nullptr;
    QLabel* hero_desc = nullptr;
    QLabel* hero_hours = nullptr;
    QPushButton* play_button = nullptr;
    QPushButton* favorite_button = nullptr;
    QPushButton* more_button = nullptr;

    QLabel* grid_title = nullptr;
    QLabel* grid_count = nullptr;
    QPushButton* sort_button = nullptr;
    QPushButton* settings_button = nullptr;
    QPushButton* dock_home = nullptr;
    QPushButton* dock_library = nullptr;
    QPushButton* dock_search = nullptr;
    QPushButton* dock_folders = nullptr;
};

} // namespace Home
