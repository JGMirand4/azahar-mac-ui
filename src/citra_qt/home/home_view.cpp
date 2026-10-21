// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QDate>
#include <QFontMetrics>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include "citra_qt/home/battery_status.h"
#include "citra_qt/home/home_view.h"

namespace Home {

namespace {

constexpr int kIconSize = 96;
constexpr int kCellWidth = 150;
constexpr int kCellHeight = 156;
constexpr int kShadowLeft = 14;
constexpr int kShadowTop = 8;
constexpr int kShadowRight = 14;
constexpr int kShadowBottom = 20;

constexpr int GameRowRole = Qt::UserRole + 301;
constexpr int SortNameRole = Qt::UserRole + 302;
constexpr int SortTimeRole = Qt::UserRole + 303;
constexpr int SortSizeRole = Qt::UserRole + 304;

QFont RoundedFont(int pixel_size, QFont::Weight weight) {
    QFont font;
    font.setFamilies({QStringLiteral("SF Pro Rounded"), QStringLiteral("Arial Rounded MT Bold"),
                      QStringLiteral("Avenir Next"), QApplication::font().family()});
    font.setPixelSize(pixel_size);
    font.setWeight(weight);
    return font;
}

QFont BodyFont(int pixel_size, QFont::Weight weight) {
    QFont font = QApplication::font();
    font.setPixelSize(pixel_size);
    font.setWeight(weight);
    return font;
}

QString CompatColor(const QString& key) {
    static const QHash<QString, QString> colors{
        {QStringLiteral("0"), QStringLiteral("#5c93ed")},  {QStringLiteral("1"), QStringLiteral("#47d35c")},
        {QStringLiteral("2"), QStringLiteral("#94b242")},  {QStringLiteral("3"), QStringLiteral("#f2d624")},
        {QStringLiteral("4"), QStringLiteral("#ff0000")},  {QStringLiteral("5"), QStringLiteral("#828282")},
        {QStringLiteral("99"), QStringLiteral("#000000")},
    };
    return colors.value(key, QStringLiteral("#828282"));
}

QColor AverageColor(const QPixmap& pixmap, const QColor& fallback) {
    if (pixmap.isNull()) {
        return fallback;
    }
    const QImage one =
        pixmap.toImage().scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const QColor color = one.pixelColor(0, 0);
    return color.alpha() < 16 ? fallback : color;
}

enum class Glyph { Gear, Star, Dots, Home, Grid, Search, Folder, Plus, Sort, Chevron, List };

QPolygonF StarPolygon() {
    return QPolygonF({QPointF(12, 2.5), QPointF(15.1, 8.8), QPointF(22, 9.8), QPointF(17, 14.7),
                      QPointF(18.2, 21.5), QPointF(12, 18.2), QPointF(5.8, 21.5), QPointF(7, 14.7),
                      QPointF(2, 9.8), QPointF(8.9, 8.8)});
}

/// Desenha um glifo simples numa grade 24x24.
QPixmap MakeGlyph(Glyph glyph, const QColor& color, int size, qreal dpr, bool filled = false,
                  const QColor& fill_color = QColor()) {
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(filled ? QBrush(fill_color.isValid() ? fill_color : color) : QBrush(Qt::NoBrush));

    switch (glyph) {
    case Glyph::Gear: {
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(12, 12), 6.2, 6.2);
        p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
        p.setPen(QPen(color, 3.2, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * M_PI / 4.0;
            p.drawLine(QPointF(12 + 8.9 * std::cos(angle), 12 + 8.9 * std::sin(angle)),
                       QPointF(12 + 10.6 * std::cos(angle), 12 + 10.6 * std::sin(angle)));
        }
        break;
    }
    case Glyph::Star:
        p.drawPolygon(StarPolygon());
        break;
    case Glyph::Dots:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(5, 12), 1.9, 1.9);
        p.drawEllipse(QPointF(12, 12), 1.9, 1.9);
        p.drawEllipse(QPointF(19, 12), 1.9, 1.9);
        break;
    case Glyph::Home: {
        QPainterPath path;
        path.moveTo(3, 11);
        path.lineTo(12, 3);
        path.lineTo(21, 11);
        path.lineTo(21, 20);
        path.lineTo(15, 20);
        path.lineTo(15, 14);
        path.lineTo(9, 14);
        path.lineTo(9, 20);
        path.lineTo(3, 20);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Glyph::Grid:
        p.drawRoundedRect(QRectF(3.5, 3.5, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(13.5, 3.5, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(3.5, 13.5, 7, 7), 2, 2);
        p.drawRoundedRect(QRectF(13.5, 13.5, 7, 7), 2, 2);
        break;
    case Glyph::Search:
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(11, 11), 7, 7);
        p.drawLine(QPointF(16.2, 16.2), QPointF(20.5, 20.5));
        break;
    case Glyph::Folder: {
        QPainterPath path;
        path.moveTo(3, 7);
        path.quadTo(3, 5, 5, 5);
        path.lineTo(9, 5);
        path.lineTo(11, 7.5);
        path.lineTo(19, 7.5);
        path.quadTo(21, 7.5, 21, 9.5);
        path.lineTo(21, 17);
        path.quadTo(21, 19, 19, 19);
        path.lineTo(5, 19);
        path.quadTo(3, 19, 3, 17);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Glyph::Plus:
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    case Glyph::Sort:
        p.drawLine(QPointF(7, 4), QPointF(7, 20));
        p.drawLine(QPointF(7, 20), QPointF(4, 17));
        p.drawLine(QPointF(7, 20), QPointF(10, 17));
        p.drawLine(QPointF(17, 20), QPointF(17, 4));
        p.drawLine(QPointF(17, 4), QPointF(14, 7));
        p.drawLine(QPointF(17, 4), QPointF(20, 7));
        break;
    case Glyph::Chevron:
        p.drawPolyline(QPolygonF({QPointF(6, 9), QPointF(12, 15), QPointF(18, 9)}));
        break;
    case Glyph::List:
        p.drawLine(QPointF(4, 6), QPointF(20, 6));
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        p.drawLine(QPointF(4, 18), QPointF(20, 18));
        break;
    }
    return pixmap;
}

QIcon MakeIcon(Glyph glyph, const QColor& color, int size, qreal dpr, bool filled = false,
               const QColor& fill_color = QColor()) {
    return QIcon(MakeGlyph(glyph, color, size, dpr, filled, fill_color));
}

} // namespace

/// Painel com cantos arredondados, vidro translucido e sombra suave.
class GlassPanel : public QWidget {
public:
    GlassPanel(const Theme* theme_, qreal radius_, bool dotted_, QWidget* parent = nullptr)
        : QWidget(parent), theme(theme_), radius(radius_), dotted(dotted_) {}

    /// Margens do layout interno: a sombra mais o respiro desejado.
    static QMargins Padding(int left, int top, int right, int bottom) {
        return QMargins(kShadowLeft + left, kShadowTop + top, kShadowRight + right,
                        kShadowBottom + bottom);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF body = QRectF(rect()).adjusted(kShadowLeft, kShadowTop, -kShadowRight,
                                                    -kShadowBottom);

        // Sombra: camadas translucidas fora do corpo do painel.
        QPainterPath outside;
        outside.addRect(QRectF(rect()));
        QPainterPath body_path;
        body_path.addRoundedRect(body, radius, radius);
        outside.addPath(body_path);
        p.save();
        p.setClipPath(outside);
        p.setPen(Qt::NoPen);
        constexpr int layers = 9;
        for (int i = 0; i < layers; ++i) {
            const qreal grow = i * 1.5;
            QColor c = theme->shadow;
            c.setAlphaF(0.045 * (1.0 - static_cast<qreal>(i) / layers));
            p.setBrush(c);
            p.drawRoundedRect(body.adjusted(-grow, -grow + 9, grow, grow + 9), radius + grow,
                              radius + grow);
        }
        p.restore();

        // Vidro.
        QLinearGradient glass(body.topLeft(), body.bottomLeft());
        glass.setColorAt(0.0, QColor(255, 255, 255, dotted ? 150 : 205));
        glass.setColorAt(1.0, QColor(255, 255, 255, dotted ? 120 : 170));
        p.setPen(Qt::NoPen);
        p.setBrush(glass);
        p.drawPath(body_path);

        if (dotted) {
            const qreal dpr = devicePixelRatioF();
            QPixmap tile(QSize(18, 18) * dpr);
            tile.setDevicePixelRatio(dpr);
            tile.fill(Qt::transparent);
            {
                QPainter t(&tile);
                t.setRenderHint(QPainter::Antialiasing);
                QColor dot = theme->accent;
                dot.setAlpha(30);
                t.setPen(Qt::NoPen);
                t.setBrush(dot);
                t.drawEllipse(QPointF(9, 9), 1.4, 1.4);
            }
            p.setBrush(QBrush(tile));
            p.drawPath(body_path);
        }

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 235), 1.0));
        p.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }

private:
    const Theme* theme;
    qreal radius;
    bool dotted;
};

/// Logo do app na barra superior.
class LogoWidget : public QWidget {
public:
    explicit LogoWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(52, 52);
    }

    void SetPixmap(const QPixmap& pixmap_) {
        pixmap = pixmap_;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (pixmap.isNull()) {
            return;
        }
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(rect(), pixmap);
    }

private:
    QPixmap pixmap;
};

/// Indicador de bateria desenhado a mao.
class BatteryWidget : public QWidget {
public:
    explicit BatteryWidget(const Theme* theme_, QWidget* parent = nullptr)
        : QWidget(parent), theme(theme_) {
        setFixedSize(36, 18);
    }

    void SetLevel(int percent_, bool charging_) {
        percent = std::clamp(percent_, 0, 100);
        charging = charging_;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF shell(1, 1, 30, 16);
        p.setPen(QPen(theme->text, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(shell, 5, 5);

        p.setPen(Qt::NoPen);
        p.setBrush(theme->text);
        p.drawRoundedRect(QRectF(32, 6, 3, 6), 1.2, 1.2);

        QColor fill = theme->battery_ok;
        if (percent <= 10) {
            fill = QColor(0xD9, 0x3B, 0x30);
        } else if (percent <= 20) {
            fill = QColor(0xE0, 0xA1, 0x00);
        }
        const QRectF inner = shell.adjusted(3, 3, -3, -3);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(inner.left(), inner.top(), inner.width() * percent / 100.0,
                                 inner.height()),
                          2, 2);
        if (charging) {
            p.setBrush(QColor(255, 255, 255, 230));
            QPolygonF bolt({QPointF(17, 3.5), QPointF(11, 10), QPointF(15, 10), QPointF(13, 15.5),
                            QPointF(20, 8.5), QPointF(16, 8.5)});
            p.drawPolygon(bolt);
        }
    }

private:
    const Theme* theme;
    int percent = 100;
    bool charging = false;
};

/// Arte do jogo em destaque: ceu, colinas e o icone do jogo em tamanho grande,
/// tudo colorido a partir da cor media do icone.
/// Parte da imagem que cobre todo o alvo sem distorcer (recorte centralizado).
static QRectF AspectFillSource(const QPixmap& pixmap, const QRectF& target) {
    const QRectF all(pixmap.rect());
    if (all.isEmpty() || target.isEmpty()) {
        return all;
    }
    const qreal target_ratio = target.width() / target.height();
    const qreal source_ratio = all.width() / all.height();
    if (source_ratio > target_ratio) {
        const qreal w = all.height() * target_ratio;
        return QRectF(all.left() + (all.width() - w) / 2, all.top(), w, all.height());
    }
    const qreal h = all.width() / target_ratio;
    return QRectF(all.left(), all.top() + (all.height() - h) / 2, all.width(), h);
}

class HeroArt : public QWidget {
public:
    explicit HeroArt(const Theme* theme_, QWidget* parent = nullptr)
        : QWidget(parent), theme(theme_) {
        animation = new QVariantAnimation(this);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setDuration(280);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            fade = value.toReal();
            update();
        });
    }

    void SetGame(const QPixmap& icon_, const QString& title, bool custom_cover_ = false) {
        icon = icon_;
        custom_cover = custom_cover_;
        initial = title.isEmpty() ? QString() : title.left(1).toUpper();
        base = AverageColor(icon, theme->accent_light);
        animation->stop();
        animation->start();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0, 14, -14, -14);
        if (r.width() < 40 || r.height() < 40) {
            return;
        }
        QPainterPath clip;
        clip.addRoundedRect(r, 22, 22);
        p.setClipPath(clip);

        if (custom_cover && !icon.isNull()) {
            // Capa personalizada: preenche todo o banner, sem distorcer.
            p.setOpacity(0.35 + 0.65 * fade);
            p.drawPixmap(r, icon, AspectFillSource(icon, r));
            p.setOpacity(1.0);
            QLinearGradient shade(QPointF(r.left(), r.top() + r.height() * 0.55), r.bottomLeft());
            shade.setColorAt(0.0, QColor(0, 0, 0, 0));
            shade.setColorAt(1.0, QColor(0, 0, 0, 60));
            p.fillRect(r, shade);
            return;
        }

        float hue = base.hslHueF();
        if (hue < 0.0f) {
            hue = theme->accent.hslHueF();
        }
        const float sat = std::clamp(static_cast<float>(base.hslSaturationF()), 0.25f, 0.8f);
        const QColor sky_top = QColor::fromHslF(hue, sat, 0.72f);
        const QColor sky_bottom = QColor::fromHslF(hue, sat * 0.6f, 0.90f);
        const QColor hill_a = QColor::fromHslF(hue, sat * 0.6f, 0.56f);
        const QColor hill_b = QColor::fromHslF(hue, sat * 0.6f, 0.46f);

        QLinearGradient sky(r.topLeft(), r.bottomLeft());
        sky.setColorAt(0.0, sky_top);
        sky.setColorAt(1.0, sky_bottom);
        p.fillRect(r, sky);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 247, 200, 235));
        p.drawEllipse(QPointF(r.left() + r.width() * 0.76, r.top() + r.height() * 0.24),
                      r.height() * 0.085, r.height() * 0.085);

        p.setOpacity(0.35 + 0.65 * fade);
        p.setBrush(hill_a);
        p.drawEllipse(QPointF(r.left() + r.width() * 0.2, r.bottom() + r.height() * 0.12),
                      r.width() * 0.68, r.height() * 0.62);
        p.setBrush(hill_b);
        p.drawEllipse(QPointF(r.left() + r.width() * 0.92, r.bottom() + r.height() * 0.2),
                      r.width() * 0.52, r.height() * 0.58);
        p.setOpacity(1.0);

        QLinearGradient gloss(r.topLeft(), QPointF(r.left(), r.top() + r.height() * 0.4));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 60));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(r, gloss);

        // Icone do jogo em tamanho grande.
        const qreal size = std::min<qreal>(r.height() * 0.6, 168.0) * (0.9 + 0.1 * fade);
        const QRectF icon_rect(r.center().x() - size / 2, r.center().y() - size / 2 - 6, size,
                               size);
        const qreal radius = size * 0.28;
        p.setOpacity(fade);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 8; ++i) {
            QColor c = theme->shadow;
            c.setAlphaF(0.06);
            p.setBrush(c);
            p.drawRoundedRect(icon_rect.adjusted(-i * 1.6, -i * 1.6 + 14, i * 1.6, i * 1.6 + 14),
                              radius + i * 1.6, radius + i * 1.6);
        }
        p.setBrush(Qt::white);
        p.drawRoundedRect(icon_rect.adjusted(-5, -5, 5, 5), radius + 5, radius + 5);

        QPainterPath icon_clip;
        icon_clip.addRoundedRect(icon_rect, radius, radius);
        p.setClipPath(icon_clip);
        if (!icon.isNull()) {
            p.drawPixmap(icon_rect, icon, QRectF(icon.rect()));
        } else {
            QLinearGradient fill(icon_rect.topLeft(), icon_rect.bottomRight());
            fill.setColorAt(0.0, theme->accent_light);
            fill.setColorAt(1.0, theme->accent);
            p.fillRect(icon_rect, fill);
            p.setPen(Qt::white);
            p.setFont(RoundedFont(static_cast<int>(size * 0.45), QFont::Bold));
            p.drawText(icon_rect, Qt::AlignCenter, initial);
        }
    }

private:
    const Theme* theme;
    QPixmap icon;
    bool custom_cover = false;
    QString initial;
    QColor base;
    QVariantAnimation* animation = nullptr;
    qreal fade = 1.0;
};

/// Desenha cada jogo como um cartao arredondado, como no menu HOME do 3DS.
class CardDelegate : public QStyledItemDelegate {
public:
    CardDelegate(HomeView* view_, QObject* parent) : QStyledItemDelegate(parent), view(view_) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(kCellWidth, kCellHeight);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const int row = index.data(GameRowRole).toInt();
        if (row < 0 || row >= static_cast<int>(view->games.size())) {
            return;
        }
        const GameInfo& game = view->games[row];
        const Theme& theme = view->theme;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;

        p->save();
        p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                          QPainter::TextAntialiasing);

        const QRectF cell = option.rect;
        qreal scale = 1.0;
        qreal lift = 0.0;
        if (selected) {
            scale = 1.07;
            lift = 5.0;
        } else if (hover) {
            scale = 1.03;
            lift = 3.0;
        }
        const QPointF center(cell.center().x(), cell.top() + 16 + kIconSize / 2.0 - lift);
        const qreal side = kIconSize * scale;
        const QRectF rect(center.x() - side / 2, center.y() - side / 2, side, side);
        const qreal radius = 27.0 * scale;

        // Sombra.
        p->setPen(Qt::NoPen);
        for (int i = 0; i < 6; ++i) {
            QColor c = selected ? theme.accent : theme.shadow;
            c.setAlphaF(selected ? 0.10 : 0.07);
            p->setBrush(c);
            p->drawRoundedRect(rect.adjusted(-i * 1.4, -i * 1.4 + 8, i * 1.4, i * 1.4 + 8),
                               radius + i * 1.4, radius + i * 1.4);
        }

        if (selected) {
            p->setBrush(theme.accent);
            p->drawRoundedRect(rect.adjusted(-8, -8, 8, 8), radius + 8, radius + 8);
            p->setBrush(Qt::white);
            p->drawRoundedRect(rect.adjusted(-4, -4, 4, 4), radius + 4, radius + 4);
        }

        QPainterPath clip;
        clip.addRoundedRect(rect, radius, radius);
        p->setClipPath(clip);
        if (!game.icon.isNull()) {
            p->drawPixmap(rect, game.icon, AspectFillSource(game.icon, rect));
        } else {
            QLinearGradient fill(rect.topLeft(), rect.bottomRight());
            fill.setColorAt(0.0, theme.accent_light);
            fill.setColorAt(1.0, theme.accent);
            p->fillRect(rect, fill);
            p->setPen(Qt::white);
            p->setFont(RoundedFont(static_cast<int>(side * 0.45), QFont::Bold));
            p->drawText(rect, Qt::AlignCenter,
                        game.title.isEmpty() ? QString() : game.title.left(1).toUpper());
        }
        QLinearGradient gloss(rect.topLeft(), QPointF(rect.left(), rect.top() + side * 0.45));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 70));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        p->fillRect(rect, gloss);
        p->setClipping(false);

        if (game.favorite) {
            p->save();
            p->translate(rect.right() - 16, rect.top() - 10);
            p->scale(23.0 / 24.0, 23.0 / 24.0);
            p->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(theme.favorite);
            p->drawPolygon(StarPolygon());
            p->restore();
        }

        // Titulo e tempo de jogo.
        const QFont title_font = RoundedFont(14, QFont::DemiBold);
        p->setFont(title_font);
        p->setPen(selected ? theme.accent : theme.text);
        const QRectF title_rect(cell.left() + 4, cell.top() + 16 + kIconSize + 8, cell.width() - 8,
                                18);
        p->drawText(title_rect, Qt::AlignHCenter | Qt::AlignVCenter,
                    QFontMetrics(title_font)
                        .elidedText(game.title, Qt::ElideRight, static_cast<int>(title_rect.width())));

        p->setFont(BodyFont(12, QFont::DemiBold));
        p->setPen(theme.text_muted);
        p->drawText(QRectF(cell.left() + 4, title_rect.bottom(), cell.width() - 8, 14),
                    Qt::AlignHCenter | Qt::AlignVCenter, game.play_time_text);
        p->restore();
    }

private:
    HomeView* view;
};

HomeView::HomeView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("homeRoot"));
    setMinimumSize(720, 600);
    BuildUi();
    ApplyLightPalette();
    RebuildStyleSheet();
    UpdateClock();
    UpdateHero();
}

HomeView::~HomeView() = default;

// A paleta e sempre clara, independente do modo escuro do sistema: o fundo (gradiente) e os
// paineis sao desenhados por nos, entao os widgets de apoio nao podem pintar o cinza escuro
// do sistema por cima.
void HomeView::ApplyLightPalette() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::AlternateBase, Qt::transparent);
    pal.setColor(QPalette::Button, QColor(0xF7, 0xFA, 0xFE));
    pal.setColor(QPalette::WindowText, theme.text);
    pal.setColor(QPalette::Text, theme.text);
    pal.setColor(QPalette::ButtonText, theme.text);
    pal.setColor(QPalette::PlaceholderText, theme.text_muted);
    pal.setColor(QPalette::Highlight, theme.accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);
    setAutoFillBackground(false);
    for (QWidget* child : findChildren<QWidget*>()) {
        child->setPalette(pal);
        child->setAutoFillBackground(false);
    }
}

void HomeView::BuildUi() {
    const qreal dpr = devicePixelRatioF();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40 - kShadowLeft, 22, 40 - kShadowRight, 12);
    root->setSpacing(0);

    // ---- Barra superior ----
    auto* top = new QWidget(this);
    auto* top_layout = new QGridLayout(top);
    top_layout->setContentsMargins(kShadowLeft, 0, kShadowRight, 4);
    top_layout->setColumnStretch(0, 1);
    top_layout->setColumnStretch(2, 1);

    auto* brand = new QWidget(top);
    auto* brand_layout = new QHBoxLayout(brand);
    brand_layout->setContentsMargins(0, 0, 0, 0);
    brand_layout->setSpacing(12);
    logo = new LogoWidget(brand);
    logo->SetPixmap(QPixmap(QStringLiteral(":/icons/default/256x256/azahar.png")));
    auto* brand_text = new QVBoxLayout;
    brand_text->setSpacing(0);
    auto* app_name = new QLabel(tr("Azahar"), brand);
    app_name->setObjectName(QStringLiteral("appName"));
    app_name->setFont(RoundedFont(24, QFont::DemiBold));
    auto* app_tag = new QLabel(tr("NINTENDO 3DS EMULATOR"), brand);
    app_tag->setObjectName(QStringLiteral("appTag"));
    QFont tag_font = BodyFont(11, QFont::Bold);
    tag_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    app_tag->setFont(tag_font);
    brand_text->addWidget(app_name);
    brand_text->addWidget(app_tag);
    brand_layout->addWidget(logo);
    brand_layout->addLayout(brand_text);
    top_layout->addWidget(brand, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

    auto* clock_pill = new QFrame(top);
    clock_pill->setObjectName(QStringLiteral("pill"));
    clock_pill->setFixedHeight(46);
    auto* clock_layout = new QHBoxLayout(clock_pill);
    clock_layout->setContentsMargins(22, 0, 22, 0);
    clock_layout->setSpacing(14);
    date_label = new QLabel(clock_pill);
    date_label->setObjectName(QStringLiteral("clockDate"));
    date_label->setFont(BodyFont(14, QFont::Bold));
    auto* separator = new QFrame(clock_pill);
    separator->setObjectName(QStringLiteral("pillSeparator"));
    separator->setFixedSize(1, 22);
    time_label = new QLabel(clock_pill);
    time_label->setObjectName(QStringLiteral("clockTime"));
    QFont time_font = RoundedFont(28, QFont::Medium);
    time_label->setFont(time_font);
    clock_layout->addWidget(date_label);
    clock_layout->addWidget(separator);
    clock_layout->addWidget(time_label);
    top_layout->addWidget(clock_pill, 0, 1, Qt::AlignCenter);

    auto* right = new QWidget(top);
    auto* right_layout = new QHBoxLayout(right);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(12);
    battery_pill = new QFrame(right);
    battery_pill->setObjectName(QStringLiteral("pill"));
    battery_pill->setFixedHeight(44);
    auto* battery_layout = new QHBoxLayout(battery_pill);
    battery_layout->setContentsMargins(18, 0, 16, 0);
    battery_layout->setSpacing(10);
    battery_label = new QLabel(battery_pill);
    battery_label->setObjectName(QStringLiteral("batteryText"));
    battery_label->setFont(BodyFont(15, QFont::Bold));
    battery = new BatteryWidget(&theme, battery_pill);
    battery_layout->addWidget(battery_label);
    battery_layout->addWidget(battery);
    battery_pill->hide();

    settings_button = new QPushButton(right);
    settings_button->setObjectName(QStringLiteral("topButton"));
    settings_button->setCursor(Qt::PointingHandCursor);
    settings_button->setToolTip(tr("Settings"));
    settings_button->setAccessibleName(tr("Settings"));
    settings_button->setIcon(MakeIcon(Glyph::Gear, theme.text, 22, dpr));
    settings_button->setIconSize(QSize(22, 22));
    right_layout->addWidget(battery_pill);
    right_layout->addWidget(settings_button);
    top_layout->addWidget(right, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(top);

    // ---- Jogo em destaque ----
    auto* hero = new GlassPanel(&theme, 32, false, this);
    hero_panel = hero;
    hero->setFixedHeight(300 + kShadowTop + kShadowBottom);
    auto* hero_layout = new QHBoxLayout(hero);
    hero_layout->setContentsMargins(GlassPanel::Padding(0, 0, 0, 0));
    hero_layout->setSpacing(0);

    auto* info = new QWidget(hero);
    auto* info_layout = new QVBoxLayout(info);
    info_layout->setContentsMargins(40, 22, 16, 22);
    info_layout->setSpacing(10);

    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    hero_compat = new QLabel(info);
    hero_compat->setObjectName(QStringLiteral("chip"));
    hero_compat->setTextFormat(Qt::RichText);
    hero_region = new QLabel(info);
    hero_region->setObjectName(QStringLiteral("chip"));
    hero_size = new QLabel(info);
    hero_size->setObjectName(QStringLiteral("chip"));
    for (QLabel* chip : {hero_compat, hero_region, hero_size}) {
        chip->setFont(BodyFont(13, QFont::Bold));
        chips->addWidget(chip);
    }
    chips->addStretch(1);
    info_layout->addLayout(chips);

    hero_title = new QLabel(info);
    hero_title->setObjectName(QStringLiteral("heroTitle"));
    hero_title->setFont(RoundedFont(32, QFont::Bold));
    hero_title->setWordWrap(true);
    hero_title->setMaximumHeight(84);
    info_layout->addWidget(hero_title);

    hero_desc = new QLabel(info);
    hero_desc->setObjectName(QStringLiteral("heroDesc"));
    hero_desc->setFont(BodyFont(15, QFont::DemiBold));
    hero_desc->setWordWrap(true);
    hero_desc->setMaximumHeight(42);
    info_layout->addWidget(hero_desc);
    info_layout->addStretch(1);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(12);
    play_button = new QPushButton(tr("Play"), info);
    play_button->setObjectName(QStringLiteral("play"));
    play_button->setCursor(Qt::PointingHandCursor);
    play_button->setFont(RoundedFont(19, QFont::Bold));
    {
        QPixmap badge(QSize(32, 32) * dpr);
        badge.setDevicePixelRatio(dpr);
        badge.fill(Qt::transparent);
        QPainter bp(&badge);
        bp.setRenderHint(QPainter::Antialiasing);
        bp.setPen(Qt::NoPen);
        bp.setBrush(Qt::white);
        bp.drawEllipse(QRectF(0, 0, 32, 32));
        bp.setPen(theme.accent);
        bp.setFont(RoundedFont(18, QFont::Bold));
        bp.drawText(QRectF(0, 0, 32, 32), Qt::AlignCenter, QStringLiteral("A"));
        bp.end();
        play_button->setIcon(QIcon(badge));
        play_button->setIconSize(QSize(32, 32));
    }
    connect(play_button, &QPushButton::clicked, this, [this] {
        if (const GameInfo* game = CurrentGame()) {
            emit GameActivated(game->path);
        }
    });
    actions->addWidget(play_button);

    hero_hours = new QLabel(info);
    hero_hours->setObjectName(QStringLiteral("heroHours"));
    hero_hours->setFont(BodyFont(14, QFont::Bold));
    actions->addWidget(hero_hours);
    actions->addStretch(1);

    more_button = new QPushButton(info);
    more_button->setObjectName(QStringLiteral("roundButton"));
    more_button->setCursor(Qt::PointingHandCursor);
    more_button->setToolTip(tr("More options"));
    more_button->setAccessibleName(tr("More options"));
    more_button->setIcon(MakeIcon(Glyph::Dots, theme.text, 22, dpr));
    more_button->setIconSize(QSize(22, 22));
    connect(more_button, &QPushButton::clicked, this, [this] {
        ShowContextMenuForCurrent(more_button->mapToGlobal(QPoint(0, more_button->height())));
    });
    actions->addWidget(more_button);

    favorite_button = new QPushButton(info);
    favorite_button->setObjectName(QStringLiteral("roundButton"));
    favorite_button->setCursor(Qt::PointingHandCursor);
    favorite_button->setCheckable(true);
    favorite_button->setToolTip(tr("Favorite"));
    favorite_button->setAccessibleName(tr("Favorite"));
    favorite_button->setIconSize(QSize(22, 22));
    connect(favorite_button, &QPushButton::clicked, this, [this] {
        if (const GameInfo* game = CurrentGame()) {
            emit FavoriteToggleRequested(game->program_id);
        }
    });
    actions->addWidget(favorite_button);
    info_layout->addLayout(actions);

    hero_art = new HeroArt(&theme, hero);
    hero_layout->addWidget(info, 46);
    hero_layout->addWidget(hero_art, 54);
    root->addWidget(hero);

    // ---- Grade de jogos ----
    auto* grid_panel = new GlassPanel(&theme, 32, true, this);
    grid_layout = new QVBoxLayout(grid_panel);
    grid_layout->setContentsMargins(GlassPanel::Padding(28, 18, 28, 12));
    grid_layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    grid_title = new QLabel(tr("My Games"), grid_panel);
    grid_title->setObjectName(QStringLiteral("sectionTitle"));
    grid_title->setFont(RoundedFont(25, QFont::DemiBold));
    grid_count = new QLabel(grid_panel);
    grid_count->setObjectName(QStringLiteral("sectionCount"));
    grid_count->setFont(BodyFont(14, QFont::Bold));
    header->addWidget(grid_title);
    header->addWidget(grid_count, 0, Qt::AlignBottom);
    header->addStretch(1);

    sort_button = new QPushButton(grid_panel);
    sort_button->setObjectName(QStringLiteral("sortPill"));
    sort_button->setCursor(Qt::PointingHandCursor);
    sort_button->setIcon(MakeIcon(Glyph::Sort, theme.ink, 16, dpr));
    sort_button->setIconSize(QSize(16, 16));
    auto* sort_menu = new QMenu(sort_button);
    const auto add_sort = [this, sort_menu](const QString& text, SortMode mode) {
        QAction* action = sort_menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, mode] { SetSortMode(mode); });
    };
    add_sort(tr("Name"), SortMode::Name);
    add_sort(tr("Play time"), SortMode::PlayTime);
    add_sort(tr("Size"), SortMode::Size);
    sort_button->setMenu(sort_menu);
    header->addWidget(sort_button);

    auto* segment = new QFrame(grid_panel);
    segment->setObjectName(QStringLiteral("segment"));
    auto* segment_layout = new QHBoxLayout(segment);
    segment_layout->setContentsMargins(3, 3, 3, 3);
    segment_layout->setSpacing(0);
    auto* grid_view_button = new QPushButton(segment);
    grid_view_button->setObjectName(QStringLiteral("segButton"));
    grid_view_button->setCheckable(true);
    grid_view_button->setChecked(true);
    grid_view_button->setToolTip(tr("Grid view"));
    grid_view_button->setAccessibleName(tr("Grid view"));
    QIcon grid_icon;
    grid_icon.addPixmap(MakeGlyph(Glyph::Grid, theme.ink, 16, dpr), QIcon::Normal, QIcon::Off);
    grid_icon.addPixmap(MakeGlyph(Glyph::Grid, Qt::white, 16, dpr, true), QIcon::Normal,
                        QIcon::On);
    grid_view_button->setIcon(grid_icon);
    grid_view_button->setIconSize(QSize(16, 16));
    auto* list_view_button = new QPushButton(segment);
    list_view_button->setObjectName(QStringLiteral("segButton"));
    list_view_button->setToolTip(tr("List view"));
    list_view_button->setAccessibleName(tr("List view"));
    list_view_button->setIcon(MakeIcon(Glyph::List, theme.ink, 16, dpr));
    list_view_button->setIconSize(QSize(16, 16));
    connect(list_view_button, &QPushButton::clicked, this, [this] { emit ClassicViewRequested(); });
    segment_layout->addWidget(grid_view_button);
    segment_layout->addWidget(list_view_button);
    header->addWidget(segment);
    grid_layout->addLayout(header);

    model = new QStandardItemModel(this);
    grid = new QListView(grid_panel);
    grid->setObjectName(QStringLiteral("gameGrid"));
    grid->setModel(model);
    grid->setViewMode(QListView::IconMode);
    grid->setResizeMode(QListView::Adjust);
    grid->setMovement(QListView::Static);
    grid->setFlow(QListView::LeftToRight);
    grid->setWrapping(true);
    grid->setUniformItemSizes(true);
    grid->setSpacing(0);
    grid->setGridSize(QSize(kCellWidth, kCellHeight));
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid->setFrameShape(QFrame::NoFrame);
    grid->setMouseTracking(true);
    grid->setContextMenuPolicy(Qt::CustomContextMenu);
    grid->setItemDelegate(new CardDelegate(this, grid));
    grid->viewport()->setAutoFillBackground(false);
    grid_layout->addWidget(grid, 1);

    connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { UpdateHero(); });
    connect(grid, &QListView::activated, this, [this](const QModelIndex& index) {
        const int row = index.data(GameRowRole).toInt();
        if (row >= 0 && row < static_cast<int>(games.size())) {
            emit GameActivated(games[row].path);
        }
    });
    connect(grid, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = grid->indexAt(pos);
        if (!index.isValid()) {
            return;
        }
        grid->setCurrentIndex(index);
        ShowContextMenuForCurrent(grid->viewport()->mapToGlobal(pos));
    });
    root->addWidget(grid_panel, 1);

    // ---- Dock inferior ----
    auto* dock = new GlassPanel(&theme, 30, false, this);
    dock->setFixedHeight(60 + kShadowTop + kShadowBottom);
    auto* dock_layout = new QHBoxLayout(dock);
    dock_layout->setContentsMargins(GlassPanel::Padding(8, 0, 8, 0));
    dock_layout->setSpacing(6);

    const auto make_dock = [this, dock, dpr](const QString& text, Glyph glyph, bool filled) {
        auto* button = new QPushButton(text, dock);
        button->setObjectName(QStringLiteral("dockItem"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFont(BodyFont(15, QFont::Bold));
        QIcon icon;
        icon.addPixmap(MakeGlyph(glyph, theme.ink, 22, dpr), QIcon::Normal, QIcon::Off);
        icon.addPixmap(MakeGlyph(glyph, theme.accent, 22, dpr, filled), QIcon::Normal, QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        return button;
    };
    dock_home = make_dock(tr("Home"), Glyph::Home, true);
    dock_home->setCheckable(true);
    dock_home->setChecked(true);
    dock_library = make_dock(tr("Library"), Glyph::Grid, false);
    dock_search = make_dock(tr("Search"), Glyph::Search, false);
    dock_folders = make_dock(tr("Folders"), Glyph::Folder, false);
    for (QPushButton* button : {dock_home, dock_library, dock_search, dock_folders}) {
        dock_layout->addWidget(button);
    }
    dock_library->setCheckable(true);
    auto* nav_group = new QButtonGroup(this);
    nav_group->setExclusive(true);
    nav_group->addButton(dock_home);
    nav_group->addButton(dock_library);
    connect(dock_home, &QPushButton::clicked, this, [this] {
        SetLibraryMode(false);
        grid->scrollToTop();
        for (int row = 0; row < model->rowCount(); ++row) {
            if (!grid->isRowHidden(row)) {
                grid->setCurrentIndex(model->index(row, 0));
                break;
            }
        }
    });
    connect(dock_library, &QPushButton::clicked, this, [this] {
        SetLibraryMode(true);
        FocusGrid();
    });
    connect(dock_search, &QPushButton::clicked, this, [this] { emit SearchRequested(); });
    connect(dock_folders, &QPushButton::clicked, this, [this] { emit AddFolderRequested(); });
    connect(settings_button, &QPushButton::clicked, this, [this] { emit SettingsRequested(); });
    root->addWidget(dock, 0, Qt::AlignHCenter);

    clock_timer = new QTimer(this);
    clock_timer->setInterval(15000);
    connect(clock_timer, &QTimer::timeout, this, [this] { UpdateClock(); });
    clock_timer->start();
}

void HomeView::RebuildStyleSheet() {
    QString qss = QStringLiteral(R"(
#homeRoot QWidget { background: transparent; }
#homeRoot QLabel { background: transparent; color: @text@; }
#homeRoot QLabel#appTag { color: @muted@; }
#homeRoot QLabel#clockDate { color: @muted@; }
#homeRoot QLabel#heroDesc { color: @body@; }
#homeRoot QLabel#heroHours { color: @muted@; }
#homeRoot QLabel#sectionCount { color: @muted@; }
#homeRoot QFrame#pill {
    background: rgba(255, 255, 255, 200);
    border: 1px solid rgba(255, 255, 255, 240);
    border-radius: 22px;
}
#homeRoot QFrame#pillSeparator { background: rgba(31, 42, 55, 42); border: none; }
#homeRoot QLabel#chip {
    background: #FFFFFF;
    border: 1px solid rgba(31, 42, 55, 31);
    border-radius: 13px;
    padding: 5px 12px;
    color: @ink@;
}
#homeRoot QPushButton#play {
    border: none;
    border-radius: 27px;
    padding: 0px 30px 0px 11px;
    min-height: 54px;
    color: #FFFFFF;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accent_light@, stop:1 @accent@);
}
#homeRoot QPushButton#play:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accent_hover@, stop:1 @accent_light@);
}
#homeRoot QPushButton#play:pressed { background: @accent@; }
#homeRoot QPushButton#roundButton, #homeRoot QPushButton#topButton {
    background: rgba(255, 255, 255, 235);
    border: 1px solid rgba(31, 42, 55, 26);
    border-radius: 25px;
    min-width: 50px; max-width: 50px; min-height: 50px; max-height: 50px;
}
#homeRoot QPushButton#topButton {
    border-radius: 22px;
    min-width: 44px; max-width: 44px; min-height: 44px; max-height: 44px;
}
#homeRoot QPushButton#roundButton:hover, #homeRoot QPushButton#topButton:hover { background: #FFFFFF; }
#homeRoot QPushButton#roundButton:pressed, #homeRoot QPushButton#topButton:pressed { background: #E6EEF8; }
#homeRoot QPushButton#roundButton:checked { background: #FFF6D6; border-color: rgba(245, 179, 1, 140); }
#homeRoot QPushButton#sortPill {
    background: #FFFFFF;
    border: 1px solid rgba(31, 42, 55, 26);
    border-radius: 19px;
    padding: 0px 16px;
    min-height: 38px;
    color: @ink@;
}
#homeRoot QPushButton#sortPill:hover { background: #F4F8FD; }
#homeRoot QPushButton#sortPill::menu-indicator { image: none; width: 0px; }
#homeRoot QFrame#segment {
    background: #FFFFFF;
    border: 1px solid rgba(31, 42, 55, 26);
    border-radius: 19px;
}
#homeRoot QPushButton#segButton {
    background: transparent;
    border: none;
    border-radius: 16px;
    min-width: 34px; max-width: 34px; min-height: 32px; max-height: 32px;
}
#homeRoot QPushButton#segButton:checked { background: @accent@; }
#homeRoot QPushButton#dockItem {
    background: transparent;
    border: none;
    border-radius: 23px;
    padding: 0px 22px;
    min-height: 46px;
    color: @ink@;
}
#homeRoot QPushButton#dockItem:hover { background: rgba(255, 255, 255, 190); }
#homeRoot QPushButton#dockItem:checked { background: #FFFFFF; color: @accent@; }
#homeRoot QListView#gameGrid { background: transparent; border: none; outline: none; }
#homeRoot QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 0px; }
#homeRoot QScrollBar::handle:vertical { background: rgba(31, 42, 55, 70); border-radius: 4px; min-height: 40px; }
#homeRoot QScrollBar::add-line:vertical, #homeRoot QScrollBar::sub-line:vertical { height: 0px; }
#homeRoot QScrollBar::add-page:vertical, #homeRoot QScrollBar::sub-page:vertical { background: transparent; }
)");
    qss.replace(QStringLiteral("@text@"), theme.text.name());
    qss.replace(QStringLiteral("@body@"), theme.text_body.name());
    qss.replace(QStringLiteral("@muted@"), theme.text_muted.name());
    qss.replace(QStringLiteral("@ink@"), theme.ink.name());
    qss.replace(QStringLiteral("@accent@"), theme.accent.name());
    qss.replace(QStringLiteral("@accent_light@"), theme.accent_light.name());
    qss.replace(QStringLiteral("@accent_hover@"), theme.accent_light.lighter(112).name());
    setStyleSheet(qss);
}

void HomeView::SetAccentColor(const QColor& accent) {
    theme = Theme::FromAccent(accent);
    ApplyLightPalette();
    RebuildStyleSheet();
    update();
    if (grid != nullptr) {
        grid->viewport()->update();
    }
    UpdateHero();
}

void HomeView::SetGames(std::vector<GameInfo> new_games) {
    games = std::move(new_games);
    RefreshModel();
}

void HomeView::RefreshModel() {
    const qulonglong current_id = CurrentGame() != nullptr ? CurrentGame()->program_id : 0;

    model->clear();
    for (int i = 0; i < static_cast<int>(games.size()); ++i) {
        const GameInfo& game = games[i];
        auto* item = new QStandardItem;
        item->setData(i, GameRowRole);
        item->setData(game.title.toLower(), SortNameRole);
        item->setData(game.play_time_seconds, SortTimeRole);
        item->setData(game.size_bytes, SortSizeRole);
        item->setToolTip(game.title);
        item->setEditable(false);
        model->appendRow(item);
    }
    ApplySort();

    // Mantem o mesmo jogo selecionado depois de atualizar a lista.
    int select_row = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (grid->isRowHidden(row)) {
            continue;
        }
        if (select_row < 0) {
            select_row = row;
        }
        const int game_index = model->index(row, 0).data(GameRowRole).toInt();
        if (current_id != 0 && games[game_index].program_id == current_id) {
            select_row = row;
            break;
        }
    }
    if (select_row >= 0) {
        grid->setCurrentIndex(model->index(select_row, 0));
    }
    UpdateHero();
}

void HomeView::ApplySort() {
    switch (sort_mode) {
    case SortMode::Name:
        model->setSortRole(SortNameRole);
        model->sort(0, Qt::AscendingOrder);
        break;
    case SortMode::PlayTime:
        model->setSortRole(SortTimeRole);
        model->sort(0, Qt::DescendingOrder);
        break;
    case SortMode::Size:
        model->setSortRole(SortSizeRole);
        model->sort(0, Qt::DescendingOrder);
        break;
    }
    sort_button->setText(tr("Sort: %1").arg(SortModeText()));
    SetFilter(filter_text);
}

QString HomeView::SortModeText() const {
    switch (sort_mode) {
    case SortMode::Name:
        return tr("Name");
    case SortMode::PlayTime:
        return tr("Play time");
    case SortMode::Size:
        return tr("Size");
    }
    return {};
}

void HomeView::SetSortMode(SortMode mode) {
    sort_mode = mode;
    ApplySort();
}

int HomeView::SetFilter(const QString& text) {
    filter_text = text.simplified().toLower();
    const QStringList words = filter_text.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    int visible = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const int game_index = model->index(row, 0).data(GameRowRole).toInt();
        const GameInfo& game = games[game_index];
        const QString haystack =
            (game.title + QLatin1Char(' ') + game.subtitle + QLatin1Char(' ') +
             game.path.mid(game.path.lastIndexOf(QLatin1Char('/')) + 1) + QLatin1Char(' ') +
             QStringLiteral("%1").arg(game.program_id, 16, 16, QLatin1Char('0')))
                .toLower();
        const bool match = std::all_of(words.begin(), words.end(), [&haystack](const QString& w) {
            return haystack.contains(w);
        });
        grid->setRowHidden(row, !match);
        if (match) {
            ++visible;
        }
    }
    grid_count->setText(tr("%n game(s)", nullptr, visible));

    // Se o jogo selecionado sumiu do filtro, seleciona o primeiro visivel.
    const QModelIndex current = grid->currentIndex();
    if (!current.isValid() || grid->isRowHidden(current.row())) {
        for (int row = 0; row < model->rowCount(); ++row) {
            if (!grid->isRowHidden(row)) {
                grid->setCurrentIndex(model->index(row, 0));
                break;
            }
        }
    }
    return visible;
}

int HomeView::GameCount() const {
    return static_cast<int>(games.size());
}

const GameInfo* HomeView::GameAtRow(int row) const {
    if (row < 0 || row >= model->rowCount()) {
        return nullptr;
    }
    const int game_index = model->index(row, 0).data(GameRowRole).toInt();
    return game_index >= 0 && game_index < static_cast<int>(games.size()) ? &games[game_index]
                                                                           : nullptr;
}

const GameInfo* HomeView::CurrentGame() const {
    const QModelIndex index = grid->currentIndex();
    if (!index.isValid() || grid->isRowHidden(index.row())) {
        return nullptr;
    }
    return GameAtRow(index.row());
}

void HomeView::SetFavorite(qulonglong program_id, bool favorite) {
    for (GameInfo& game : games) {
        if (game.program_id == program_id) {
            game.favorite = favorite;
        }
    }
    grid->viewport()->update();
    UpdateHero();
}

void HomeView::SetSearchWidget(QWidget* widget) {
    search_widget = widget;
    grid_layout->insertWidget(1, widget);
}

void HomeView::TakeSearchWidget() {
    if (search_widget != nullptr) {
        grid_layout->removeWidget(search_widget);
        search_widget = nullptr;
    }
}

/// Modo biblioteca: esconde o destaque para a grade ocupar toda a janela.
void HomeView::SetLibraryMode(bool enabled) {
    library_mode = enabled;
    hero_panel->setVisible(!enabled);
    dock_home->setChecked(!enabled);
    dock_library->setChecked(enabled);
}

void HomeView::FocusGrid() {
    grid->setFocus();
}

void HomeView::ShowContextMenuForCurrent(const QPoint& global_pos) {
    if (const GameInfo* game = CurrentGame()) {
        const GameInfo copy = *game; // a lista pode ser recarregada enquanto o menu esta aberto
        emit GameContextMenuRequested(copy, global_pos);
    }
}

void HomeView::UpdateHero() {
    const GameInfo* game = CurrentGame();
    const qreal dpr = devicePixelRatioF();
    const bool has_game = game != nullptr;

    for (QLabel* chip : {hero_compat, hero_region, hero_size}) {
        chip->setVisible(has_game && !compact);
    }
    hero_desc->setVisible(!compact);
    play_button->setEnabled(has_game);
    more_button->setEnabled(has_game);
    favorite_button->setEnabled(has_game);

    if (!has_game) {
        hero_title->setText(tr("No games found"));
        hero_desc->clear();
        hero_hours->clear();
        hero_art->SetGame(QPixmap(), QString());
        favorite_button->setIcon(MakeIcon(Glyph::Star, theme.text, 22, dpr));
        return;
    }

    hero_title->setText(game->title.isEmpty() ? tr("Unknown title") : game->title);
    hero_desc->setText(game->subtitle);

    hero_compat->setVisible(!compact && !game->compat_text.isEmpty());
    hero_compat->setText(QStringLiteral("<span style='color:%1;'>&#9679;</span>&nbsp;%2")
                             .arg(CompatColor(game->compat_key), game->compat_text.toHtmlEscaped()));
    hero_region->setVisible(!compact && !game->region.isEmpty());
    hero_region->setText(game->region);
    hero_size->setVisible(!compact && !game->size_text.isEmpty());
    hero_size->setText(game->size_text);

    hero_hours->setText(game->play_time_seconds > 0 ? tr("%1 played").arg(game->play_time_text)
                                                    : tr("Not played yet"));
    favorite_button->setChecked(game->favorite);
    favorite_button->setIcon(
        MakeIcon(Glyph::Star, theme.text, 22, dpr, game->favorite, theme.favorite));
    hero_art->SetGame(game->icon, game->title, game->custom_cover);
}

void HomeView::SetLocale(const QLocale& locale) {
    ui_locale = locale;
    if (date_label != nullptr) {
        UpdateClock();
    }
}

void HomeView::UpdateClock() {
    const QLocale& locale = ui_locale;
    QString date = locale.toString(QDate::currentDate(), QStringLiteral("ddd, d MMM"));
    if (!date.isEmpty()) {
        date[0] = date[0].toUpper();
    }
    date_label->setText(date);
    // Hora curta sem segundos, respeitando 12h/24h do sistema.
    QString time_format = locale.timeFormat(QLocale::ShortFormat);
    time_format.remove(QRegularExpression(QStringLiteral("[:.]s{1,2}")));
    time_label->setText(locale.toString(QTime::currentTime(), time_format));

    int percent = 0;
    bool charging = false;
    if (QueryBattery(percent, charging)) {
        battery_label->setText(QStringLiteral("%1%").arg(percent));
        battery->SetLevel(percent, charging);
        battery_pill->show();
    } else {
        battery_pill->hide();
    }
}

void HomeView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect();

    QLinearGradient base(r.topLeft(), r.bottomRight());
    base.setColorAt(0.0, theme.bg_top);
    base.setColorAt(0.5, theme.bg_mid);
    base.setColorAt(1.0, theme.bg_bottom);
    p.fillRect(r, base);

    p.setPen(Qt::NoPen);
    QRadialGradient blob_a(QPointF(r.left() + 120, r.top() + 80), 440);
    QColor a0 = theme.blob_a;
    QColor a1 = theme.blob_a;
    a1.setAlpha(0);
    blob_a.setColorAt(0.0, a0);
    blob_a.setColorAt(0.68, a1);
    blob_a.setColorAt(1.0, a1);
    p.setBrush(blob_a);
    p.drawRect(r);

    QRadialGradient blob_b(QPointF(r.right() - 60, r.bottom() - 40), 480);
    QColor b0 = theme.blob_b;
    QColor b1 = theme.blob_b;
    b1.setAlpha(0);
    blob_b.setColorAt(0.0, b0);
    blob_b.setColorAt(0.68, b1);
    blob_b.setColorAt(1.0, b1);
    p.setBrush(blob_b);
    p.drawRect(r);
}

void HomeView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Em janelas estreitas a arte do jogo some para dar espaco ao texto.
    hero_art->setVisible(width() >= 880);
    // O destaque ocupa cerca de 29% da altura, para sobrar espaco para a grade.
    // Em janelas baixas ele fica compacto: so titulo e botoes.
    const bool was_compact = compact;
    compact = height() < 760;
    const int hero_height =
        compact ? 168 : std::clamp(static_cast<int>(height() * 0.29), 232, 300);
    hero_panel->setFixedHeight(hero_height + kShadowTop + kShadowBottom);
    if (compact != was_compact) {
        UpdateHero();
    }
}

} // namespace Home
