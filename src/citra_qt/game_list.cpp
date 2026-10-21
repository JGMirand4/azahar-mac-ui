// Copyright 2015-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include <algorithm>
#include <QActionGroup>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QImage>
#include <QFileDialog>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QThreadPool>
#include <QToolButton>
#include <QTreeView>
#include <fmt/format.h>
#include "citra_qt/citra_qt.h"
#include "citra_qt/compatibility_list.h"
#include "citra_qt/game_list.h"
#include "citra_qt/game_list_p.h"
#include "citra_qt/game_list_worker.h"
#include "citra_qt/home/home_view.h"
#include "citra_qt/uisettings.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "qcursor.h"

GameListSearchField::KeyReleaseEater::KeyReleaseEater(GameList* gamelist, QObject* parent)
    : QObject(parent), gamelist{gamelist} {}

// EventFilter in order to process systemkeys while editing the searchfield
bool GameListSearchField::KeyReleaseEater::eventFilter(QObject* obj, QEvent* event) {
    // If it isn't a KeyRelease event then continue with standard event processing
    if (event->type() != QEvent::KeyRelease)
        return QObject::eventFilter(obj, event);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    QString edit_filter_text = gamelist->search_field->edit_filter->text().toLower();

    // If the searchfield's text hasn't changed special function keys get checked
    // If no function key changes the searchfield's text the filter doesn't need to get reloaded
    if (edit_filter_text == edit_filter_text_old) {
        switch (keyEvent->key()) {
        // Escape: Resets the searchfield
        case Qt::Key_Escape: {
            if (edit_filter_text_old.isEmpty()) {
                return QObject::eventFilter(obj, event);
            } else {
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
            }
            break;
        }
        // Return and Enter
        // If the enter key gets pressed first checks how many and which entry is visible
        // If there is only one result launch this game
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if (gamelist->search_field->visible == 1) {
                const QString file_path = gamelist->GetLastFilterResultItem();

                // To avoid loading error dialog loops while confirming them using enter
                // Also users usually want to run a different game after closing one
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
                emit gamelist->GameChosen(file_path);
            } else {
                return QObject::eventFilter(obj, event);
            }
            break;
        }
        default:
            return QObject::eventFilter(obj, event);
        }
    }
    edit_filter_text_old = edit_filter_text;
    return QObject::eventFilter(obj, event);
}

void GameListSearchField::setFilterResult(int visible, int total) {
    this->visible = visible;
    this->total = total;

    label_filter_result->setText(QStringLiteral("%1/%2").arg(visible).arg(total));
}

bool GameListSearchField::IsEmpty() const {
    return edit_filter->text().isEmpty();
}

QString GameList::GetLastFilterResultItem() const {
    QString file_path;
    const int folderCount = item_model->rowCount();

    for (int i = 0; i < folderCount; ++i) {
        const QStandardItem* folder = item_model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();

        for (int j = 0; j < children_count; ++j) {
            if (tree_view->isRowHidden(j, folder_index)) {
                continue;
            }

            const QStandardItem* child = folder->child(j, 0);
            file_path = child->data(GameListItemPath::FullPathRole).toString();
        }
    }

    return file_path;
}

void GameListSearchField::clear() {
    edit_filter->clear();
}

void GameListSearchField::setFocus() {
    if (edit_filter->isVisible()) {
        edit_filter->setFocus();
    }
}

GameListSearchField::GameListSearchField(GameList* parent) : QWidget{parent} {
    auto* const key_release_eater = new KeyReleaseEater(parent, this);
    layout_filter = new QHBoxLayout;
    layout_filter->setContentsMargins(8, 8, 8, 8);
    label_filter = new QLabel;
    edit_filter = new QLineEdit;
    edit_filter->clear();
    edit_filter->installEventFilter(key_release_eater);
    edit_filter->setClearButtonEnabled(true);
    connect(edit_filter, &QLineEdit::textChanged, parent, &GameList::OnTextChanged);
    label_filter_result = new QLabel;
    button_filter_close = new QToolButton(this);
    button_filter_close->setText(QStringLiteral("X"));
    button_filter_close->setCursor(Qt::ArrowCursor);
    button_filter_close->setStyleSheet(
        QStringLiteral("QToolButton{ border: none; padding: 0px; color: "
                       "#000000; font-weight: bold; background: #F0F0F0; border-radius: 8px; }"
                       "QToolButton:hover{ border: none; padding: 0px; color: "
                       "#EEEEEE; font-weight: bold; background: #E81123; border-radius: 8px; }"));
    connect(button_filter_close, &QToolButton::clicked, parent, &GameList::OnFilterCloseClicked);
    layout_filter->setSpacing(10);
    layout_filter->addWidget(label_filter);
    layout_filter->addWidget(edit_filter);
    layout_filter->addWidget(label_filter_result);
    layout_filter->addWidget(button_filter_close);
    setLayout(layout_filter);
    RetranslateUI();
}

/**
 * Checks if all words separated by spaces are contained in another string
 * This offers a word order insensitive search function
 *
 * @param String that gets checked if it contains all words of the userinput string
 * @param String containing all words getting checked
 * @return true if the haystack contains all words of userinput
 */
static bool ContainsAllWords(const QString& haystack, const QString& userinput) {
    const QStringList userinput_split = userinput.split(QLatin1Char{' '}, Qt::SkipEmptyParts);

    return std::all_of(userinput_split.begin(), userinput_split.end(),
                       [&haystack](const QString& s) { return haystack.contains(s); });
}

// Syncs the expanded state of Game Directories with settings to persist across sessions
void GameList::OnItemExpanded(const QModelIndex& item) {
    const auto type = item.data(GameListItem::TypeRole).value<GameListItemType>();
    const bool is_dir = type == GameListItemType::CustomDir ||
                        type == GameListItemType::InstalledDir ||
                        type == GameListItemType::SystemDir;

    if (!is_dir) {
        return;
    }

    UISettings::values.game_dirs[item.data(GameListDir::GameDirRole).toInt()].expanded =
        tree_view->isExpanded(item);
}

// Event in order to filter the gamelist after editing the searchfield
void GameList::OnTextChanged(const QString& new_text) {
    if (home_mode) {
        const int visible = home_view->SetFilter(new_text);
        search_field->setFilterResult(visible, home_view->GameCount());
    }
    const int folder_count = tree_view->model()->rowCount();
    QString edit_filter_text = new_text.toLower();
    QStandardItem* folder;
    int children_total = 0;

    // If the searchfield is empty every item is visible
    // Otherwise the filter gets applied
    if (edit_filter_text.isEmpty()) {
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(),
                                UISettings::values.favorited_ids.size() == 0);
        for (int i = 1; i < folder_count; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;
                tree_view->setRowHidden(j, folder_index, false);
            }
        }
        search_field->setFilterResult(children_total, children_total);
    } else {
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(), true);
        int result_count = 0;
        for (int i = 1; i < folder_count; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;
                const QStandardItem* child = folder->child(j, 0);
                const QString file_path =
                    child->data(GameListItemPath::FullPathRole).toString().toLower();
                const QString file_title =
                    child->data(GameListItemPath::LongTitleRole).toString().toLower();
                const QString file_program_id =
                    child->data(GameListItemPath::ProgramIdRole).toString().toLower();

                // Only items which filename in combination with its title contains all words
                // that are in the searchfield will be visible in the gamelist
                // The search is case insensitive because of toLower()
                // I decided not to use Qt::CaseInsensitive in containsAllWords to prevent
                // multiple conversions of edit_filter_text for each game in the gamelist
                const QString file_name =
                    file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) + QLatin1Char{' '} +
                    file_title;
                if (ContainsAllWords(file_name, edit_filter_text) ||
                    (file_program_id.length() == 16 &&
                     edit_filter_text.contains(file_program_id))) {
                    tree_view->setRowHidden(j, folder_index, false);
                    ++result_count;
                } else {
                    tree_view->setRowHidden(j, folder_index, true);
                }
                search_field->setFilterResult(result_count, children_total);
            }
        }
    }
}

void GameList::OnUpdateThemedIcons() {
    for (int i = 0; i < item_model->invisibleRootItem()->rowCount(); i++) {
        QStandardItem* child = item_model->invisibleRootItem()->child(i);

        const int icon_size = IconSizes.at(UISettings::values.game_list_icon_size.GetValue());
        switch (child->data(GameListItem::TypeRole).value<GameListItemType>()) {
        case GameListItemType::InstalledDir:
            child->setData(QIcon::fromTheme(QStringLiteral("sd_card")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        case GameListItemType::SystemDir:
            child->setData(QIcon::fromTheme(QStringLiteral("chip")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        case GameListItemType::CustomDir: {
            const UISettings::GameDir& game_dir =
                UISettings::values.game_dirs[child->data(GameListDir::GameDirRole).toInt()];
            const QString icon_name = QFileInfo::exists(game_dir.path)
                                          ? QStringLiteral("folder")
                                          : QStringLiteral("bad_folder");
            child->setData(QIcon::fromTheme(icon_name).pixmap(icon_size), Qt::DecorationRole);
            break;
        }
        case GameListItemType::AddDir:
            child->setData(QIcon::fromTheme(QStringLiteral("plus")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        case GameListItemType::Favorites:
            child->setData(
                QIcon::fromTheme(QStringLiteral("star"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        default:
            break;
        }
    }
}

void GameList::OnFilterCloseClicked() {
    main_window->filterBarSetChecked(false);
}

class CartridgeIconDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

        // Draw the default item (background, text, selection, etc.)
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        // Draw cartridge inserted icon
        quint32 can_insert = index.data(GameListItemPath::CanInsertRole).value<quint32>();
        QString game_path = index.data(GameListItemPath::FullPathRole).value<QString>();

        bool is_inserted = can_insert && UISettings::values.inserted_cartridge.GetValue() ==
                                             game_path.toStdString();

        if (is_inserted) {
            QPixmap pixmap = QIcon::fromTheme(QStringLiteral("cartridge")).pixmap(24);

            const int margin = 12;
            QSize pmSize = pixmap.size() / pixmap.devicePixelRatio();

            QRect pmRect(opt.rect.right() - pmSize.width() - margin,
                         opt.rect.center().y() - pmSize.height() / 2, pmSize.width(),
                         pmSize.height());

            painter->drawPixmap(pmRect, pixmap);
        }
    }
};

static QColor ColorFromSetting(const std::string& value) {
    return value.empty() ? QColor() : QColor(QString::fromStdString(value));
}

// Chave estavel de um jogo: ID do programa em hexadecimal ou, sem ele, hash do caminho.
static QString GameKey(u64 program_id, const QString& game_path) {
    if (program_id != 0) {
        return QStringLiteral("%1").arg(static_cast<qulonglong>(program_id), 16, 16,
                                        QLatin1Char{'0'});
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(game_path.toUtf8(), QCryptographicHash::Md5).toHex());
}

// Estado so da tela inicial (ultimo jogado). Fica ao lado das capas, na pasta do usuario.
static QString HomeStatePath() {
    return QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir)) +
           QStringLiteral("home_state.ini");
}

GameList::GameList(PlayTime::PlayTimeManager& play_time_manager_, GMainWindow* parent)
    : QWidget{parent}, play_time_manager{play_time_manager_} {
    watcher = new QFileSystemWatcher(this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &GameList::RefreshGameDirectory,
            Qt::UniqueConnection);

    this->main_window = parent;
    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    search_field = new GameListSearchField(this);
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);

    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_view->setStyleSheet(QStringLiteral("QTreeView{ border: none; }"));
    tree_view->setItemDelegateForColumn(0, new CartridgeIconDelegate(tree_view));
    tree_view->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    UpdateColumnVisibility();

    item_model->insertColumns(0, COLUMN_COUNT);
    RetranslateUI();
    item_model->setSortRole(GameListItemPath::SortRole);

    connect(main_window, &GMainWindow::UpdateThemedIcons, this, &GameList::OnUpdateThemedIcons);
    connect(main_window, &GMainWindow::InstalledTitlesChanged, this,
            &GameList::RefreshGameDirectory);
    connect(tree_view, &QTreeView::activated, this, &GameList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this, &GameList::PopupContextMenu);
    connect(tree_view, &QTreeView::expanded, this, &GameList::OnItemExpanded);
    connect(tree_view, &QTreeView::collapsed, this, &GameList::OnItemExpanded);
    connect(tree_view->header(), &QHeaderView::customContextMenuRequested, this,
            &GameList::PopupHeaderContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use
    // it with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    home_view = new Home::HomeView(this);
    home_view->SetLocale(UISettings::values.language.isEmpty()
                             ? QLocale::system()
                             : QLocale(UISettings::values.language));
    home_view->SetBackgroundColors(ColorFromSetting(UISettings::values.home_bg_top.GetValue()),
                                   ColorFromSetting(UISettings::values.home_bg_bottom.GetValue()));
    home_view->SetAccentColor(
        QColor(QString::fromStdString(UISettings::values.home_accent_color.GetValue())));
    connect(home_view, &Home::HomeView::GameActivated, this, &GameList::OnHomeGameActivated);
    connect(home_view, &Home::HomeView::GameContextMenuRequested, this,
            &GameList::PopupHomeContextMenu);
    connect(home_view, &Home::HomeView::FavoriteToggleRequested, this,
            [this](qulonglong id) { ToggleFavorite(id); });
    connect(home_view, &Home::HomeView::SettingsRequested, this,
            &GameList::OpenSettingsRequested);
    connect(home_view, &Home::HomeView::AddFolderRequested, this, &GameList::AddDirectory);
    connect(home_view, &Home::HomeView::SearchRequested, this,
            [this] { main_window->filterBarSetChecked(true); });
    connect(home_view, &Home::HomeView::ClassicViewRequested, this,
            [this] { SetHomeMode(false, false); }); // so nesta sessao

    layout->addWidget(home_view);
    layout->addWidget(tree_view);
    layout->addWidget(search_field);
    setLayout(layout);

    connect(this, &GameList::GameChosen, this, &GameList::RecordLastPlayed);
    SetHomeMode(UISettings::values.game_list_use_home_view.GetValue());
}

void GameList::SetHomeMode(bool enable, bool persist) {
    home_mode = enable;
    home_view->setVisible(enable);
    tree_view->setVisible(!enable);
    if (enable) {
        layout->removeWidget(search_field);
        home_view->SetSearchWidget(search_field);
        search_field->setVisible(filter_bar_visible);
        RebuildHomeView();
    } else {
        home_view->TakeSearchWidget();
        search_field->setParent(this);
        layout->addWidget(search_field);
        search_field->setVisible(filter_bar_visible);
    }
    if (persist) {
        UISettings::values.game_list_use_home_view.SetValue(enable);
    }
    emit HomeModeChanged(enable);
}

void GameList::ApplyHomeSettings() {
    home_view->SetLocale(UISettings::values.language.isEmpty()
                             ? QLocale::system()
                             : QLocale(UISettings::values.language));
    home_view->SetBackgroundColors(ColorFromSetting(UISettings::values.home_bg_top.GetValue()),
                                   ColorFromSetting(UISettings::values.home_bg_bottom.GetValue()));
    home_view->SetAccentColor(
        QColor(QString::fromStdString(UISettings::values.home_accent_color.GetValue())));
    const bool use_home = UISettings::values.game_list_use_home_view.GetValue();
    if (use_home != home_mode) {
        SetHomeMode(use_home);
    }
}

void GameList::RecordLastPlayed(const QString& path) {
    u64 program_id = 0;
    const auto* root = item_model->invisibleRootItem();
    for (int i = 0; i < root->rowCount() && program_id == 0; ++i) {
        const QStandardItem* dir = root->child(i, 0);
        for (int j = 0; dir != nullptr && j < dir->rowCount(); ++j) {
            const QStandardItem* item = dir->child(j, COLUMN_NAME);
            if (item != nullptr && item->data(GameListItemPath::FullPathRole).toString() == path) {
                program_id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
                break;
            }
        }
    }
    QSettings state(HomeStatePath(), QSettings::IniFormat);
    state.setValue(QStringLiteral("last_played/") + GameKey(program_id, path),
                   QDateTime::currentSecsSinceEpoch());
    state.sync();
    RebuildHomeView();
}

void GameList::OnHomeGameActivated(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo file_info(path);
    if (!file_info.exists() || file_info.isDir()) {
        return;
    }
    search_field->clear();
    emit GameChosen(path);
}

// Capas personalizadas ficam em <pasta do usuario>/covers/<id do jogo>.png
static QString CoverFilePath(u64 program_id, const QString& game_path) {
    const QString dir = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir)) +
                        QStringLiteral("covers");
    return dir + QLatin1Char{'/'} + GameKey(program_id, game_path) + QStringLiteral(".png");
}

void GameList::PopupHomeContextMenu(const Home::GameInfo& info, const QPoint& global_pos) {
    QMenu context_menu;
    const QString cover_path = CoverFilePath(info.program_id, info.path);
    QAction* change_cover = context_menu.addAction(tr("Change cover..."));
    connect(change_cover, &QAction::triggered, this, [this, info, cover_path] {
        const QString image = QFileDialog::getOpenFileName(
            this, tr("Choose a cover image"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (image.isEmpty()) {
            return;
        }
        QImage source(image);
        if (source.isNull()) {
            QMessageBox::warning(this, tr("Change cover"), tr("Could not open this image."));
            return;
        }
        // Mantem a proporcao original (so reduz); o card e o banner recortam ao exibir.
        QImage square = source.width() > 1280 || source.height() > 1280
                            ? source.scaled(1280, 1280, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation)
                            : source;
        QDir().mkpath(QFileInfo(cover_path).absolutePath());
        if (!square.save(cover_path, "PNG")) {
            QMessageBox::warning(this, tr("Change cover"), tr("Could not save the cover."));
            return;
        }
        RebuildHomeView();
    });
    if (QFileInfo::exists(cover_path)) {
        QAction* reset_cover = context_menu.addAction(tr("Restore original cover"));
        connect(reset_cover, &QAction::triggered, this, [this, cover_path] {
            QFile::remove(cover_path);
            RebuildHomeView();
        });
    }
    context_menu.addSeparator();
    AddGamePopup(context_menu, info.path, info.title, info.program_id, info.extdata_id,
                 static_cast<Service::FS::MediaType>(info.media_type), info.can_insert);
    context_menu.exec(global_pos);
}

void GameList::RebuildHomeView() {
    std::vector<Home::GameInfo> games;
    const QSettings last_played_state(HomeStatePath(), QSettings::IniFormat);
    const auto* root = item_model->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i) {
        const QStandardItem* dir = root->child(i, 0);
        if (dir == nullptr) {
            continue;
        }
        const auto type = dir->data(GameListItem::TypeRole).value<GameListItemType>();
        if (type != GameListItemType::CustomDir && type != GameListItemType::InstalledDir &&
            type != GameListItemType::SystemDir) {
            continue;
        }
        for (int j = 0; j < dir->rowCount(); ++j) {
            const QStandardItem* name = dir->child(j, COLUMN_NAME);
            if (name == nullptr) {
                continue;
            }
            Home::GameInfo info;
            info.path = name->data(GameListItemPath::FullPathRole).toString();
            const QString file_name = QFileInfo(info.path).completeBaseName();
            info.title = name->data(GameListItemPath::TitleRole).toString();
            if (info.title.isEmpty()) {
                info.title = file_name;
            }
            const QString long_title = name->data(GameListItemPath::LongTitleRole).toString();
            info.subtitle = long_title.isEmpty() ? file_name : long_title.simplified();
            info.icon = name->data(GameListItemPath::LargeIconRole).value<QPixmap>();
            info.program_id = name->data(GameListItemPath::ProgramIdRole).toULongLong();
            if (const QString cover = CoverFilePath(info.program_id, info.path);
                QFileInfo::exists(cover)) {
                if (QPixmap custom(cover); !custom.isNull()) {
                    info.icon = custom;
                    info.custom_cover = true;
                }
            }
            info.extdata_id = name->data(GameListItemPath::ExtdataIdRole).toULongLong();
            info.media_type = name->data(GameListItemPath::MediaTypeRole).toUInt();
            info.can_insert = name->data(GameListItemPath::CanInsertRole).toUInt() != 0;
            info.favorite = UISettings::values.favorited_ids.contains(info.program_id);
            info.last_played =
                last_played_state
                    .value(QStringLiteral("last_played/") + GameKey(info.program_id, info.path), 0)
                    .toLongLong();

            if (const auto* it = dir->child(j, COLUMN_REGION)) {
                info.region = it->text().replace(QLatin1Char{'\n'}, QStringLiteral(", "));
            }
            if (const auto* it = dir->child(j, COLUMN_COMPATIBILITY)) {
                info.compat_text = it->text();
                info.compat_key = it->data(GameListItemCompat::CompatNumberRole).toString();
            }
            if (const auto* it = dir->child(j, COLUMN_SIZE)) {
                info.size_text = it->text();
                info.size_bytes = it->data(GameListItemSize::SizeRole).toULongLong();
            }
            if (const auto* it = dir->child(j, COLUMN_PLAY_TIME)) {
                info.play_time_text = it->text();
                info.play_time_seconds = it->data(GameListItemPlayTime::PlayTimeRole).toULongLong();
            }
            games.push_back(std::move(info));
        }
    }
    home_view->SetGames(std::move(games));
}

GameList::~GameList() {
    emit ShouldCancelWorker();
}

void GameList::SetFilterFocus() {
    if (home_mode || tree_view->model()->rowCount() > 0) {
        search_field->setFocus();
    }
}

void GameList::SetFilterVisible(bool visibility) {
    filter_bar_visible = visibility;
    search_field->setVisible(visibility);
}

void GameList::SetDirectoryWatcherEnabled(bool enabled) {
    if (enabled) {
        connect(watcher, &QFileSystemWatcher::directoryChanged, this,
                &GameList::RefreshGameDirectory, Qt::UniqueConnection);
    } else {
        disconnect(watcher, &QFileSystemWatcher::directoryChanged, this,
                   &GameList::RefreshGameDirectory);
    }
}

void GameList::ClearFilter() {
    search_field->clear();
}

void GameList::AddDirEntry(GameListDir* entry_items) {
    item_model->invisibleRootItem()->appendRow(entry_items);
    tree_view->setExpanded(
        entry_items->index(),
        UISettings::values.game_dirs[entry_items->data(GameListDir::GameDirRole).toInt()].expanded);
}

void GameList::AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent) {
    parent->appendRow(entry_items);
}

void GameList::ValidateEntry(const QModelIndex& item) {
    const auto selected = item.sibling(item.row(), 0);

    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game: {
        const QString file_path = selected.data(GameListItemPath::FullPathRole).toString();
        if (file_path.isEmpty())
            return;
        const QFileInfo file_info(file_path);
        if (!file_info.exists() || file_info.isDir())
            return;
        // Users usually want to run a different game after closing one
        search_field->clear();
        emit GameChosen(file_path);
        break;
    }
    case GameListItemType::AddDir:
        emit AddDirectory();
        break;
    default:
        break;
    }
}

bool GameList::IsEmpty() const {
    for (int i = 0; i < item_model->rowCount(); i++) {
        const QStandardItem* child = item_model->invisibleRootItem()->child(i);
        const auto type = static_cast<GameListItemType>(child->type());

        if (!child->hasChildren() &&
            (type == GameListItemType::InstalledDir || type == GameListItemType::SystemDir)) {
            item_model->invisibleRootItem()->removeRow(child->row());
            i--;
        }
    }

    return !item_model->invisibleRootItem()->hasChildren();
}

void GameList::DonePopulating(const QStringList& watch_list) {
    emit ShowList(!IsEmpty());

    item_model->invisibleRootItem()->appendRow(new GameListAddDir());

    // Add favorites row
    item_model->invisibleRootItem()->insertRow(0, new GameListFavorites());
    tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(),
                            UISettings::values.favorited_ids.size() == 0);
    tree_view->expand(item_model->invisibleRootItem()->child(0)->index());
    for (const auto id : UISettings::values.favorited_ids) {
        AddFavorite(id);
    }

    // Clear out the old directories to watch for changes and add the new ones
    auto watch_dirs = watcher->directories();
    if (!watch_dirs.isEmpty()) {
        watcher->removePaths(watch_dirs);
    }
    // Workaround: Add the watch paths in chunks to allow the gui to refresh
    // This prevents the UI from stalling when a large number of watch paths are added
    // Also artificially caps the watcher to a certain number of directories
    constexpr qsizetype LIMIT_WATCH_DIRECTORIES = 5000;
    constexpr int SLICE_SIZE = 25;
    const qsizetype len = std::min(watch_list.length(), LIMIT_WATCH_DIRECTORIES);
    for (qsizetype i = 0; i < len; i += SLICE_SIZE) {
        watcher->addPaths(watch_list.mid(i, i + SLICE_SIZE));
        QCoreApplication::processEvents();
    }
    tree_view->setEnabled(true);
    const int folderCount = tree_view->model()->rowCount();
    int children_total = 0;
    for (int i = 1; i < folderCount; ++i) {
        children_total += item_model->item(i, 0)->rowCount();
    }
    search_field->setFilterResult(children_total, children_total);
    if (children_total > 0 && !home_mode) {
        search_field->setFocus();
    }
    item_model->sort(tree_view->header()->sortIndicatorSection(),
                     tree_view->header()->sortIndicatorOrder());

    // resize all columns except for Name to fit their contents
    for (int i = 1; i < COLUMN_COUNT; i++) {
        tree_view->resizeColumnToContents(i);
    }
    tree_view->header()->setStretchLastSection(true);

    RebuildHomeView();
    emit PopulatingCompleted();
}

void GameList::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    const auto selected = item.sibling(item.row(), 0);
    QMenu context_menu;
    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game:
        AddGamePopup(context_menu, selected.data(GameListItemPath::FullPathRole).toString(),
                     selected.data(GameListItemPath::TitleRole).toString(),
                     selected.data(GameListItemPath::ProgramIdRole).toULongLong(),
                     selected.data(GameListItemPath::ExtdataIdRole).toULongLong(),
                     static_cast<Service::FS::MediaType>(
                         selected.data(GameListItemPath::MediaTypeRole).toUInt()),
                     selected.data(GameListItemPath::CanInsertRole).toUInt() != 0);
        break;
    case GameListItemType::CustomDir:
        AddPermDirPopup(context_menu, selected);
        AddCustomDirPopup(context_menu, selected);
        break;
    case GameListItemType::InstalledDir:
    case GameListItemType::SystemDir:
        AddPermDirPopup(context_menu, selected);
        break;
    case GameListItemType::Favorites:
        AddFavoritesPopup(context_menu);
        break;
    default:
        break;
    }

    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::PopupHeaderContextMenu(const QPoint& menu_location) {
    const QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid()) {
        return;
    }

    QMenu context_menu;
    static const QMap<QString, Settings::Setting<bool>*> columns{
        {tr("Compatibility"), &UISettings::values.show_compat_column},
        {tr("Region"), &UISettings::values.show_region_column},
        {tr("File type"), &UISettings::values.show_type_column},
        {tr("Size"), &UISettings::values.show_size_column},
        {tr("Play time"), &UISettings::values.show_play_time_column}};

    QActionGroup* column_group = new QActionGroup(this);
    column_group->setExclusive(false);
    for (const auto& key : columns.keys()) {
        QAction* action = column_group->addAction(context_menu.addAction(key));
        action->setCheckable(true);
        action->setChecked(columns[key]->GetValue());
        connect(action, &QAction::toggled, [this, key](bool value) {
            *columns[key] = !columns[key]->GetValue();
            UpdateColumnVisibility();
        });
    }

    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::UpdateColumnVisibility() {
    tree_view->setColumnHidden(COLUMN_COMPATIBILITY, !UISettings::values.show_compat_column);
    tree_view->setColumnHidden(COLUMN_REGION, !UISettings::values.show_region_column);
    tree_view->setColumnHidden(COLUMN_FILE_TYPE, !UISettings::values.show_type_column);
    tree_view->setColumnHidden(COLUMN_SIZE, !UISettings::values.show_size_column);
    tree_view->setColumnHidden(COLUMN_PLAY_TIME, !UISettings::values.show_play_time_column);
}

#ifdef ENABLE_OPENGL
void ForEachOpenGLCacheFile(u64 program_id, auto func) {
    for (const std::string_view cache_type : {"separable", "conventional"}) {
        const std::string path = fmt::format("{}opengl/precompiled/{}/{:016X}.bin",
                                             FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir),
                                             cache_type, program_id);
        QFile file{QString::fromStdString(path)};
        func(file);
    }
    const std::string path =
        fmt::format("{}opengl/transferable/{:016X}.bin",
                    FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir), program_id);
    QFile file{QString::fromStdString(path)};
    func(file);
}
#endif

#ifdef ENABLE_VULKAN
void ForEachVulkanCacheFile(u64 program_id, auto func) {
    for (const std::string_view cache_type : {"vs", "fs", "gs", "pl"}) {
        const std::string path = fmt::format("{}vulkan/transferable/{:016X}_{}.vkch",
                                             FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir),
                                             program_id, cache_type);
        QFile file{QString::fromStdString(path)};
        func(file);
    }

    FileUtil::ForeachDirectoryEntry(
        nullptr,
        fmt::format("{}vulkan/pipeline", FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir)),
        [program_id, &func]([[maybe_unused]] u64* num_entries_out, const std::string& directory,
                            const std::string& virtual_name) {
            if (virtual_name.starts_with(fmt::format("{:016X}", program_id))) {
                QFile file{QString::fromStdString(directory + DIR_SEP + virtual_name)};
                func(file);
            }

            return true;
        });
}
#endif

void GameList::AddGamePopup(QMenu& context_menu, const QString& path, const QString& name,
                            u64 program_id, u64 extdata_id, Service::FS::MediaType media_type,
                            bool can_insert) {
    QAction* favorite = context_menu.addAction(tr("Favorite"));
    bool is_inserted =
        can_insert && UISettings::values.inserted_cartridge.GetValue() == path.toStdString();
    QAction* cartridge_insert = nullptr;
    if (can_insert) {
        cartridge_insert =
            context_menu.addAction(is_inserted ? tr("Eject Cartridge") : tr("Insert Cartridge"));
    }
    context_menu.addSeparator();
    QMenu* open_menu = context_menu.addMenu(tr("Open"));
    QAction* open_application_location = open_menu->addAction(tr("Application Location"));
    open_menu->addSeparator();
    QAction* open_save_location = open_menu->addAction(tr("Save Data Location"));
    QAction* open_extdata_location = open_menu->addAction(tr("Extra Data Location"));
    QAction* open_update_location = open_menu->addAction(tr("Update Data Location"));
    QAction* open_dlc_location = open_menu->addAction(tr("DLC Data Location"));
    open_menu->addSeparator();
    QAction* open_texture_dump_location = open_menu->addAction(tr("Texture Dump Location"));
    QAction* open_texture_load_location = open_menu->addAction(tr("Custom Texture Location"));
    QAction* open_mods_location = open_menu->addAction(tr("Mods Location"));

    QAction* dump_romfs = context_menu.addAction(tr("Dump RomFS"));

    QMenu* shader_menu = context_menu.addMenu(tr("Disk Shader Cache"));
    QAction* open_shader_cache_location = shader_menu->addAction(tr("Open Shader Cache Location"));
    shader_menu->addSeparator();
#ifdef ENABLE_OPENGL
    QAction* delete_opengl_disk_shader_cache =
        shader_menu->addAction(tr("Delete OpenGL Shader Cache"));
#endif
#ifdef ENABLE_VULKAN
    QAction* delete_vulkan_disk_shader_cache =
        shader_menu->addAction(tr("Delete Vulkan Shader Cache"));
#endif

    QMenu* uninstall_menu = context_menu.addMenu(tr("Uninstall"));
    QAction* uninstall_all = uninstall_menu->addAction(tr("Everything"));
    uninstall_menu->addSeparator();
    QAction* uninstall_game = uninstall_menu->addAction(tr("Application"));
    QAction* uninstall_update = uninstall_menu->addAction(tr("Update"));
    QAction* uninstall_dlc = uninstall_menu->addAction(tr("DLC"));

    QAction* remove_play_time_data = context_menu.addAction(tr("Remove Play Time Data"));

#if !defined(__APPLE__)
    QMenu* shortcut_menu = context_menu.addMenu(tr("Create Shortcut"));
    QAction* create_desktop_shortcut = shortcut_menu->addAction(tr("Add to Desktop"));
    QAction* create_applications_menu_shortcut =
        shortcut_menu->addAction(tr("Add to Applications Menu"));
#endif

#ifdef ENABLE_DEVELOPER_OPTIONS
    context_menu.addSeparator();
    QAction* stress_test_launch = context_menu.addAction(tr("Stress Test: App Launch"));
#endif

    context_menu.addSeparator();
    QAction* properties = context_menu.addAction(tr("Properties"));

    const u32 program_id_high = (program_id >> 32) & 0xFFFFFFFF;
    // TODO: Use proper bitmasks for these kinds of checks.
    const bool is_application = program_id_high == 0x00040000 || program_id_high == 0x00040002 ||
                                program_id_high == 0x00040010;

#ifdef ENABLE_OPENGL
    bool opengl_cache_exists = false;
    ForEachOpenGLCacheFile(
        program_id, [&opengl_cache_exists](QFile& file) { opengl_cache_exists |= file.exists(); });
#endif

#ifdef ENABLE_VULKAN
    bool vulkan_cache_exists = false;
    ForEachVulkanCacheFile(
        program_id, [&vulkan_cache_exists](QFile& file) { vulkan_cache_exists |= file.exists(); });
#endif

    favorite->setVisible(program_id != 0);
    favorite->setCheckable(true);
    favorite->setChecked(UISettings::values.favorited_ids.contains(program_id));

    std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    open_save_location->setEnabled(
        is_application && FileUtil::Exists(FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
                              sdmc_dir, program_id)));

    if (extdata_id) {
        open_extdata_location->setEnabled(
            is_application &&
            FileUtil::Exists(FileSys::GetExtDataPathFromId(sdmc_dir, extdata_id)));
    } else {
        open_extdata_location->setVisible(false);
    }

    const auto update_program_id = program_id | 0xE00000000;
    const auto dlc_program_id = program_id | 0x8C00000000;

    const auto is_installed =
        media_type == Service::FS::MediaType::NAND || media_type == Service::FS::MediaType::SDMC;
    const auto has_update =
        is_application && FileUtil::Exists(Service::AM::GetTitlePath(Service::FS::MediaType::SDMC,
                                                                     update_program_id) +
                                           "content/");
    const auto has_dlc =
        is_application &&
        FileUtil::Exists(Service::AM::GetTitlePath(Service::FS::MediaType::SDMC, dlc_program_id) +
                         "content/");

    open_application_location->setEnabled(is_installed);
    open_update_location->setEnabled(has_update);
    open_dlc_location->setEnabled(has_dlc);
    open_texture_dump_location->setEnabled(is_application);
    open_texture_load_location->setEnabled(is_application);
    open_mods_location->setEnabled(is_application);
    dump_romfs->setEnabled(is_application);

#ifdef ENABLE_OPENGL
    delete_opengl_disk_shader_cache->setEnabled(opengl_cache_exists);
#endif

#ifdef ENABLE_VULKAN
    delete_vulkan_disk_shader_cache->setEnabled(vulkan_cache_exists);
#endif

    uninstall_all->setEnabled(is_installed || has_update || has_dlc);
    uninstall_game->setEnabled(is_installed);
    uninstall_update->setEnabled(has_update);
    uninstall_dlc->setEnabled(has_dlc);

    connect(favorite, &QAction::triggered, [this, program_id]() { ToggleFavorite(program_id); });
    connect(open_save_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::SAVE_DATA);
    });
    connect(open_extdata_location, &QAction::triggered, this, [this, extdata_id] {
        emit OpenFolderRequested(extdata_id, GameListOpenTarget::EXT_DATA);
    });
    if (cartridge_insert) {
        connect(cartridge_insert, &QAction::triggered, this, [this, path, is_inserted] {
            if (is_inserted) {
                UISettings::values.inserted_cartridge.SetValue("");
            } else {
                UISettings::values.inserted_cartridge.SetValue(path.toStdString());
            }
            tree_view->viewport()->update();
        });
    }
    connect(open_application_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::APPLICATION);
    });
    connect(open_update_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::UPDATE_DATA);
    });
    connect(open_texture_dump_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::TEXTURE_DUMP);
        }
    });
    connect(open_texture_load_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::TEXTURE_LOAD);
        }
    });
    connect(open_mods_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}mods/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::MODS);
        }
    });
    connect(open_dlc_location, &QAction::triggered, this, [this, program_id] {
        const u64 trimmed_id = program_id & 0xFFFFFFF;
        const std::string dlc_path =
            fmt::format("{}Nintendo 3DS/00000000000000000000000000000000/"
                        "00000000000000000000000000000000/title/0004008c/{:08x}/content/",
                        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), trimmed_id);
        fmt::print("DLC path {}\n", dlc_path);
        if (FileUtil::CreateFullPath(dlc_path)) {
            emit OpenFolderRequested(trimmed_id, GameListOpenTarget::DLC_DATA);
        }
    });
    connect(dump_romfs, &QAction::triggered, this,
            [this, path, program_id] { emit DumpRomFSRequested(path, program_id); });
    connect(remove_play_time_data, &QAction::triggered,
            [this, program_id]() { emit RemovePlayTimeRequested(program_id); });
#ifdef ENABLE_DEVELOPER_OPTIONS
    connect(stress_test_launch, &QAction::triggered,
            [this, path]() { emit StartingLaunchStressTest(path); });
#endif
    connect(properties, &QAction::triggered, this,
            [this, path]() { emit OpenPerGameGeneralRequested(path); });
    connect(open_shader_cache_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::SHADER_CACHE);
        }
    });
#ifdef ENABLE_OPENGL
    connect(delete_opengl_disk_shader_cache, &QAction::triggered, this, [program_id] {
        ForEachOpenGLCacheFile(program_id, [](QFile& file) { file.remove(); });
    });
#endif
#ifdef ENABLE_VULKAN
    connect(delete_vulkan_disk_shader_cache, &QAction::triggered, this, [program_id] {
        ForEachVulkanCacheFile(program_id, [](QFile& file) { file.remove(); });
    });
#endif
    connect(uninstall_all, &QAction::triggered, this, [=, this] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Azahar"),
            tr("Are you sure you want to completely uninstall '%1'?\n\nThis will "
               "delete the application if installed, as well as any installed updates or DLC.")
                .arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            if (is_installed) {
                titles.emplace_back(media_type, program_id, name);
            }
            if (has_update) {
                titles.emplace_back(Service::FS::MediaType::SDMC, update_program_id,
                                    tr("%1 (Update)").arg(name));
            }
            if (has_dlc) {
                titles.emplace_back(Service::FS::MediaType::SDMC, dlc_program_id,
                                    tr("%1 (DLC)").arg(name));
            }
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_game, &QAction::triggered, this, [this, name, media_type, program_id] {
        QMessageBox::StandardButton answer =
            QMessageBox::question(this, QStringLiteral("Azahar"),
                                  tr("Are you sure you want to uninstall '%1'?").arg(name),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(media_type, program_id, name);
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_update, &QAction::triggered, this, [this, name, update_program_id] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Azahar"),
            tr("Are you sure you want to uninstall the update for '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(Service::FS::MediaType::SDMC, update_program_id,
                                tr("%1 (Update)").arg(name));
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_dlc, &QAction::triggered, this, [this, name, dlc_program_id] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Azahar"),
            tr("Are you sure you want to uninstall all DLC for '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(Service::FS::MediaType::SDMC, dlc_program_id,
                                tr("%1 (DLC)").arg(name));
            main_window->UninstallTitles(titles);
        }
    });
    // TODO: Implement shortcut creation for macOS
#if !defined(__APPLE__)
    connect(create_desktop_shortcut, &QAction::triggered, [this, program_id, path]() {
        emit CreateShortcut(program_id, path.toStdString(), GameListShortcutTarget::Desktop);
    });
    connect(create_applications_menu_shortcut, &QAction::triggered, [this, program_id, path]() {
        emit CreateShortcut(program_id, path.toStdString(), GameListShortcutTarget::Applications);
    });
#endif
}

void GameList::AddCustomDirPopup(QMenu& context_menu, QModelIndex selected) {
    UISettings::GameDir& game_dir =
        UISettings::values.game_dirs[selected.data(GameListDir::GameDirRole).toInt()];

    QAction* deep_scan = context_menu.addAction(tr("Scan Subfolders"));
    QAction* delete_dir = context_menu.addAction(tr("Remove Application Directory"));

    deep_scan->setCheckable(true);
    deep_scan->setChecked(game_dir.deep_scan);

    connect(deep_scan, &QAction::triggered, this, [this, &game_dir] {
        game_dir.deep_scan = !game_dir.deep_scan;
        PopulateAsync(UISettings::values.game_dirs);
    });
    connect(delete_dir, &QAction::triggered, this, [this, &game_dir, selected] {
        UISettings::values.game_dirs.removeOne(game_dir);
        item_model->invisibleRootItem()->removeRow(selected.row());
    });
}

void GameList::AddPermDirPopup(QMenu& context_menu, QModelIndex selected) {
    const int game_dir_index = selected.data(GameListDir::GameDirRole).toInt();

    QAction* move_up =
        context_menu.addAction(tr("Move Up").prepend(QString::fromWCharArray(L"\u25b2 ")));
    QAction* move_down =
        context_menu.addAction(tr("Move Down").prepend(QString::fromWCharArray(L"\u25bc ")));
    QAction* open_directory_location = context_menu.addAction(tr("Open Directory Location"));

    const int row = selected.row();

    move_up->setEnabled(row > 1);
    move_down->setEnabled(row < item_model->rowCount() - 2);

    connect(move_up, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row - 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        GetModel()->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        GetModel()->setData(selected.sibling(row - 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row - 1, item);
        tree_view->setExpanded(selected, UISettings::values.game_dirs[game_dir_index].expanded);
    });

    connect(move_down, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row + 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        GetModel()->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        GetModel()->setData(selected.sibling(row + 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        const QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row + 1, item);
        tree_view->setExpanded(selected, UISettings::values.game_dirs[game_dir_index].expanded);
    });

    connect(open_directory_location, &QAction::triggered, this, [this, game_dir_index] {
        emit OpenDirectory(UISettings::values.game_dirs[game_dir_index].path);
    });
}

void GameList::AddFavoritesPopup(QMenu& context_menu) {
    QAction* clear = context_menu.addAction(tr("Clear"));

    connect(clear, &QAction::triggered, [this] {
        for (const auto id : UISettings::values.favorited_ids) {
            RemoveFavorite(id);
        }
        UISettings::values.favorited_ids.clear();
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(), true);
    });
}

void GameList::LoadCompatibilityList() {
    QFile compat_list{QStringLiteral(":compatibility_list/compatibility_list.json")};

    if (!compat_list.open(QFile::ReadOnly | QFile::Text)) {
        LOG_ERROR(Frontend, "Unable to open application compatibility list");
        return;
    }

    if (compat_list.size() == 0) {
        LOG_WARNING(Frontend, "Application compatibility list is empty");
        return;
    }

    const QByteArray content = compat_list.readAll();
    if (content.isEmpty()) {
        LOG_ERROR(Frontend, "Unable to completely read application compatibility list");
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(content);
    const QJsonArray arr = json.array();

    for (const QJsonValue& value : arr) {
        const QJsonObject game = value.toObject();
        const QString compatibility_key = QStringLiteral("compatibility");

        if (!game.contains(compatibility_key) || !game[compatibility_key].isDouble()) {
            continue;
        }

        const int compatibility = game[compatibility_key].toInt();
        const QString directory = game[QStringLiteral("directory")].toString();
        const QJsonArray ids = game[QStringLiteral("releases")].toArray();

        for (const QJsonValue& id_ref : ids) {
            const QJsonObject id_object = id_ref.toObject();
            const QString id = id_object[QStringLiteral("id")].toString();

            compatibility_list.emplace(id.toUpper().toStdString(),
                                       std::make_pair(QString::number(compatibility), directory));
        }
    }
}

void GameList::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameList::RetranslateUI() {
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name"));
    item_model->setHeaderData(COLUMN_COMPATIBILITY, Qt::Horizontal, tr("Compatibility"));
    item_model->setHeaderData(COLUMN_REGION, Qt::Horizontal, tr("Region"));
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, tr("File type"));
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, tr("Size"));
    item_model->setHeaderData(COLUMN_PLAY_TIME, Qt::Horizontal, tr("Play time"));
}

void GameListSearchField::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameListSearchField::RetranslateUI() {
    label_filter->setText(tr("Filter:"));
    edit_filter->setPlaceholderText(tr("Enter pattern to filter"));
}

QStandardItemModel* GameList::GetModel() const {
    return item_model;
}

void GameList::PopulateAsync(QVector<UISettings::GameDir>& game_dirs) {
    tree_view->setEnabled(false);

    // Update the columns in case UISettings has changed
    UpdateColumnVisibility();

    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());
    search_field->clear();

    emit ShouldCancelWorker();

    GameListWorker* worker = new GameListWorker(game_dirs, compatibility_list, play_time_manager);

    connect(worker, &GameListWorker::EntryReady, this, &GameList::AddEntry, Qt::QueuedConnection);
    connect(worker, &GameListWorker::DirEntryReady, this, &GameList::AddDirEntry,
            Qt::QueuedConnection);
    connect(worker, &GameListWorker::Finished, this, &GameList::DonePopulating,
            Qt::QueuedConnection);
    // Use DirectConnection here because worker->Cancel() is thread-safe and we want it to
    // cancel without delay.
    connect(this, &GameList::ShouldCancelWorker, worker, &GameListWorker::Cancel,
            Qt::DirectConnection);

    QThreadPool::globalInstance()->start(worker);
    current_worker = std::move(worker);
}

void GameList::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout() {
    auto* header = tree_view->header();

    if (header->restoreState(UISettings::values.gamelist_header_state)) {
        return;
    }

    // We are using the name column to display icons and titles
    // so make it as large as possible as default.
    header->resizeSection(COLUMN_NAME, header->width());
}

const QStringList GameList::supported_file_extensions = {
    QStringLiteral("3dsx"), QStringLiteral("elf"),  QStringLiteral("axf"),   QStringLiteral("cci"),
    QStringLiteral("cxi"),  QStringLiteral("app"),  QStringLiteral("z3dsx"), QStringLiteral("zcci"),
    QStringLiteral("zcxi"), QStringLiteral("bcci"), QStringLiteral("3ds"),
};

void GameList::RefreshGameDirectory() {
    // Do not scan directories when the system is powered on, it will be
    // repopulated on shutdown anyways.
    if (Core::System::GetInstance().IsPoweredOn()) {
        return;
    }

    const auto time_now = std::chrono::steady_clock::now();

    // Max of 1 refresh every 1 second.
    if (time_last_refresh + std::chrono::seconds(1) > time_now) {
        return;
    }

    time_last_refresh = time_now;

    if (!UISettings::values.game_dirs.isEmpty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "Change detected in the applications directory. Reloading game list.");
        PopulateAsync(UISettings::values.game_dirs);
    }
}

void GameList::ToggleFavorite(u64 program_id) {
    if (!UISettings::values.favorited_ids.contains(program_id)) {
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(),
                                !search_field->IsEmpty());
        UISettings::values.favorited_ids.append(program_id);
        AddFavorite(program_id);
        item_model->sort(tree_view->header()->sortIndicatorSection(),
                         tree_view->header()->sortIndicatorOrder());
    } else {
        UISettings::values.favorited_ids.removeOne(program_id);
        RemoveFavorite(program_id);
        if (UISettings::values.favorited_ids.size() == 0) {
            tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(), true);
        }
    }
    home_view->SetFavorite(program_id, UISettings::values.favorited_ids.contains(program_id));
}

void GameList::AddFavorite(u64 program_id) {
    auto* favorites_row = item_model->item(0);

    for (int i = 1; i < item_model->rowCount() - 1; i++) {
        const auto* folder = item_model->item(i);
        for (int j = 0; j < folder->rowCount(); j++) {
            if (folder->child(j)->data(GameListItemPath::ProgramIdRole).toULongLong() ==
                program_id) {
                QList<QStandardItem*> list;
                for (int k = 0; k < COLUMN_COUNT; k++) {
                    list.append(folder->child(j, k)->clone());
                }
                list[0]->setData(folder->child(j)->data(GameListItem::SortRole),
                                 GameListItem::SortRole);
                list[0]->setText(folder->child(j)->data(Qt::DisplayRole).toString());

                favorites_row->appendRow(list);
                return;
            }
        }
    }
}

void GameList::RemoveFavorite(u64 program_id) {
    auto* favorites_row = item_model->item(0);

    for (int i = 0; i < favorites_row->rowCount(); i++) {
        const auto* game = favorites_row->child(i);
        if (game->data(GameListItemPath::ProgramIdRole).toULongLong() == program_id) {
            favorites_row->removeRow(i);
            return;
        }
    }
}

QString GameList::FindGameByProgramID(u64 program_id, int role) {
    return FindGameByProgramID(item_model->invisibleRootItem(), program_id, role);
}

QString GameList::FindGameByProgramID(QStandardItem* current_item, u64 program_id, int role) {
    if (current_item->type() == static_cast<int>(GameListItemType::Game) &&
        current_item->data(GameListItemPath::ProgramIdRole).toULongLong() == program_id) {
        return current_item->data(role).toString();
    } else if (current_item->hasChildren()) {
        for (int child_id = 0; child_id < current_item->rowCount(); child_id++) {
            QString path = FindGameByProgramID(current_item->child(child_id, 0), program_id, role);
            if (!path.isEmpty())
                return path;
        }
    }
    return QString();
}

GameListPlaceholder::GameListPlaceholder(GMainWindow* parent) : QWidget{parent} {
    connect(parent, &GMainWindow::UpdateThemedIcons, this,
            &GameListPlaceholder::onUpdateThemedIcons);

    layout = new QVBoxLayout;
    image = new QLabel;
    text = new QLabel;
    layout->setAlignment(Qt::AlignCenter);
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));

    text->setText(tr("Double-click to add a new folder to the application list"));
    QFont font = text->font();
    font.setPointSize(20);
    text->setFont(font);
    text->setAlignment(Qt::AlignHCenter);
    image->setAlignment(Qt::AlignHCenter);

    layout->addWidget(image);
    layout->addWidget(text);
    setLayout(layout);
}

GameListPlaceholder::~GameListPlaceholder() = default;

void GameListPlaceholder::onUpdateThemedIcons() {
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));
}

void GameListPlaceholder::mouseDoubleClickEvent([[maybe_unused]] QMouseEvent* event) {
    emit GameListPlaceholder::AddDirectory();
}
