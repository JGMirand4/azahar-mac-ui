// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <vector>
#include <QLocale>
#include <QColor>
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
class QVariantAnimation;
class QVBoxLayout;

namespace Home {

class CarouselWidget;
class ListHeader;
class GamepadNav;
class Curtain;
class HintBar;
class SoundBank;

class BatteryWidget;
class HeroArt;

/// Dados de um jogo exibidos na tela inicial. Preenchidos pela lista de jogos.
struct GameInfo {
    QString path;
    QString title;
    QString subtitle; // titulo longo ou nome do arquivo
    QPixmap icon;     // icone grande (48x48) do SMDH; nulo se nao houver
    QString region;
    QString product_id; // codigo de 4 letras do jogo (ex.: BNDP); vazio se desconhecido
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
    bool custom_cover = false; // capa escolhida pelo usuario (preenche o banner)
    qint64 last_played = 0;    // segundos desde 1970; 0 = nunca jogado pelo Hanami
};

enum class SortMode {
    Recent,
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
    /// Alterna entre a grade de capas e a lista detalhada (continua dentro da tela inicial).
    /// 0 = carrossel (estilo HOME), 1 = grade, 2 = lista detalhada.
    void SetViewMode(int mode);
    /// Pasta dos sons gerados e se eles estao ligados.
    void SetSoundOptions(const QString& dir, bool enabled);
    /// Icones dos botoes do controle: 0 = automatico, 1 = Xbox, 2 = PlayStation.
    void SetPadStyle(int style);
    /// Estilo do icone dos jogos: 0 = normal, 1 = cartucho de 3DS.
    void SetIconStyle(int style);
    /// Redesenha o emblema da marca com o icone atual do app (claro ou escuro).
    void RefreshBrand();
    /// Reflete no botao do topo se o modo console esta ativo.
    void SetConsoleMode(bool enabled);
    /// Um botao do controle foi apertado (valores de GamepadNav::Button).
    void HandlePadButton(int button);
    /// Preferencias de exibicao (salvas pelo GameList entre execucoes).
    int GetViewMode() const {
        return view_mode;
    }
    int GetSortMode() const {
        return static_cast<int>(sort_mode);
    }
    int GetQuickFilter() const {
        return quick_filter;
    }
    void SetPreferences(int mode, int sort, int quick);

    /// Troca a cor de destaque e deriva o restante da paleta.
    void SetAccentColor(const QColor& accent);
    /// Cores do degrade de fundo; cor invalida = automatica a partir do destaque.
    void SetBackgroundColor(const QColor& color);
    const Theme& GetTheme() const {
        return theme;
    }

    void FocusGrid();
    void SetLibraryMode(bool enabled);

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
    void PreferencesChanged();
    void ConsoleModeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class CardDelegate;
    friend class CarouselWidget;
    friend class ListHeader;
    friend class Curtain;

    void BuildUi();
    void RebuildStyleSheet();
    void ApplyLightPalette();
    void RefreshModel();
    void UpdateHero();
    void UpdateClock();
    void ApplySort();
    void UpdateGridMetrics();
    void UpdateHeroSize();
    void ApplyViewMode();
    void UpdateHints();
    void FocusActiveView();
    bool HandleShortcut(QKeyEvent* event);
    /// Toca a animacao de abertura do jogo e so entao emite GameActivated.
    void RequestLaunch(const QString& path);
    void PlaySound(int sound);
    void SetQuickFilter(int mode);
    void UpdateQuickCounts();
    void PickRandomGame();
    void ShowContextMenuForCurrent(const QPoint& global_pos);
    QString SortModeText() const;

    Theme theme;
    QColor accent_color{0x0F, 0x6F, 0xD0};
    QColor bg_top_color;
    QLocale ui_locale = QLocale::system();
    std::vector<GameInfo> games;
    SortMode sort_mode = SortMode::Recent;
    bool compact = false;
    bool library_mode = false;
    bool list_mode = false; // derivado: view_mode == 2
    int view_mode = 0;
    int visible_count = 0;
    bool library_switched = false;
    CarouselWidget* carousel = nullptr;
    Curtain* curtain = nullptr;
    SoundBank* sounds = nullptr;
    QPushButton* console_button = nullptr;
    bool launching = false;
    GamepadNav* gamepad = nullptr;
    bool pad_hints = false;  // dicas mostram os botoes do controle em vez das teclas
    bool injecting = false;  // teclas sinteticas vindas do controle
    int pad_layout = 0;
    bool sound_mute = true; // ligado enquanto a tela monta, para nao tocar sons sem querer
    HintBar* hints = nullptr;
    QPushButton* mode_buttons[3] = {nullptr, nullptr, nullptr};
    int quick_filter = 0; // 0 = todos, 1 = favoritos, 2 = ja jogados
    QString filter_text;

    QStandardItemModel* model = nullptr;
    QListView* grid = nullptr;
    QVBoxLayout* grid_layout = nullptr;
    QWidget* search_widget = nullptr;

    QPushButton* quick_buttons[3] = {nullptr, nullptr, nullptr};
    QLabel* date_label = nullptr;
    QLabel* time_label = nullptr;
    QFrame* battery_pill = nullptr;
    QLabel* battery_label = nullptr;
    BatteryWidget* battery = nullptr;
    QTimer* clock_timer = nullptr;

    QWidget* hero_panel = nullptr;
    HeroArt* hero_art = nullptr;
    QLabel* hero_eyebrow = nullptr;
    QLabel* hero_compat = nullptr;
    QLabel* hero_region = nullptr;
    QLabel* hero_size = nullptr;
    QLabel* hero_title = nullptr;
    QLabel* hero_desc = nullptr;
    QLabel* hero_hours = nullptr;
    QPushButton* play_button = nullptr;
    QPushButton* favorite_button = nullptr;
    QPushButton* more_button = nullptr;
    QPushButton* surprise_button = nullptr;
    QLabel* console_emblem = nullptr;
    QLabel* brand_badge = nullptr;
    QTimer* hero_timer = nullptr; // agrupa as atualizacoes do destaque ao navegar rapido
    QFrame* coin_pill = nullptr;
    QLabel* coin_label = nullptr;
    ListHeader* list_header = nullptr;
    QWidget* grid_panel_widget = nullptr;
    QVariantAnimation* tint_anim = nullptr;
    QColor tint_now;
    bool UsePlaystationIcons() const;
    void UpdatePlayIcon();
    int pad_style = 0; // 0 = automatico, 1 = Xbox, 2 = PlayStation
    void UpdateTint();
    void ApplyTint();
    void CycleSort();

    QLabel* grid_title = nullptr;
    QWidget* empty_state = nullptr;
    QLabel* empty_label = nullptr;
    QLabel* empty_icon = nullptr;
    QPushButton* empty_add_button = nullptr;
    void UpdateEmptyState(int visible);
    QPushButton* sort_button = nullptr;
    QPushButton* settings_button = nullptr;
    QPushButton* dock_home = nullptr;
    QPushButton* dock_library = nullptr;
    QPushButton* dock_search = nullptr;
    QPushButton* dock_folders = nullptr;
};

} // namespace Home
