// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cmath>
#include <functional>
#include <QApplication>
#include <QDate>
#include <QFontMetrics>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCache>
#include <QGuiApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include "citra_qt/home/battery_status.h"
#include "citra_qt/home/gamepad.h"
#include "citra_qt/home/home_view.h"
#include "citra_qt/home/sound_bank.h"

namespace Home {

namespace {

constexpr int kIconSize = 96;
constexpr int kCellWidth = 150;
constexpr int kCellHeight = 156;
constexpr int kRowHeight = 76;
constexpr int kShadowLeft = 14;
constexpr int kShadowTop = 8;
constexpr int kShadowRight = 14;
constexpr int kShadowBottom = 20;

constexpr int GameRowRole = Qt::UserRole + 301;
constexpr int SortNameRole = Qt::UserRole + 302;
constexpr int SortTimeRole = Qt::UserRole + 303;
constexpr int SortSizeRole = Qt::UserRole + 304;
constexpr int SortRecentRole = Qt::UserRole + 305;

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

enum class Glyph { Gear, Star, Dots, Home, Grid, Search, Folder, Plus, Sort, Chevron, List, Carousel, Fullscreen, Dice, Console };

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
        // Engrenagem de 8 dentes: contorno unico com furo no centro.
        QPainterPath cog;
        constexpr int kTeeth = 8;
        constexpr qreal kPi = 3.14159265358979323846;
        for (int i = 0; i < kTeeth; ++i) {
            const qreal base = i * 2 * kPi / kTeeth;
            const qreal half_root = 0.20 * 2 * kPi / kTeeth * 1.7;
            const qreal half_tip = 0.20 * 2 * kPi / kTeeth * 1.1;
            const qreal angles[4] = {base - half_root, base - half_tip, base + half_tip,
                                     base + half_root};
            const qreal radii[4] = {7.4, 10.0, 10.0, 7.4};
            for (int k = 0; k < 4; ++k) {
                const QPointF point(12 + radii[k] * std::cos(angles[k]),
                                    12 + radii[k] * std::sin(angles[k]));
                if (i == 0 && k == 0) {
                    cog.moveTo(point);
                } else {
                    cog.lineTo(point);
                }
            }
        }
        cog.closeSubpath();
        p.setBrush(Qt::NoBrush);
        p.drawPath(cog);
        p.drawEllipse(QPointF(12, 12), 3.0, 3.0);
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
    case Glyph::Fullscreen:
        for (const int sx : {-1, 1}) {
            for (const int sy : {-1, 1}) {
                const QPointF corner(12 + sx * 7.5, 12 + sy * 7.5);
                p.drawPolyline(QPolygonF({corner + QPointF(-sx * 4.5, 0), corner,
                                          corner + QPointF(0, -sy * 4.5)}));
            }
        }
        break;
    case Glyph::Console: {
        // Console portatil aberto (so contornos e telas): o interior fica vazado, sem preenchimento.
        const QColor ink = color;
        p.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(3.8, 1.8, 16.4, 10.2), 2.4, 2.4);   // tampa
        p.drawRoundedRect(QRectF(3.8, 12.9, 16.4, 9.3), 2.4, 2.4);   // base
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawRoundedRect(QRectF(6.7, 3.6, 10.6, 6.2), 0.8, 0.8);    // tela de cima
        p.drawRoundedRect(QRectF(8.4, 14.4, 7.2, 4.9), 0.8, 0.8);    // tela de baixo
        p.drawEllipse(QPointF(12, 2.9), 0.32, 0.32);                  // camera
        for (const qreal y : {6.0, 7.0, 8.0}) {                       // alto-falantes
            p.drawEllipse(QPointF(5.3, y), 0.22, 0.22);
            p.drawEllipse(QPointF(18.7, y), 0.22, 0.22);
        }
        p.setPen(QPen(ink, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(6.0, 15.2), 1.35, 1.35);                // direcional analogico
        p.drawLine(QPointF(4.9, 18.6), QPointF(7.1, 18.6));           // cruzeta
        p.drawLine(QPointF(6.0, 17.5), QPointF(6.0, 19.7));
        for (const QPointF& b : {QPointF(18.0, 14.2), QPointF(18.0, 16.4), QPointF(16.9, 15.3),
                                 QPointF(19.1, 15.3)}) {              // A B X Y
            p.drawEllipse(b, 0.55, 0.55);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        for (const qreal x : {9.0, 11.2, 13.4}) {                     // botoes de baixo
            p.drawRoundedRect(QRectF(x, 20.2, 1.8, 0.8), 0.3, 0.3);
        }
        p.drawRect(QRectF(18.4, 19.4, 0.9, 0.9));
        break;
    }
    case Glyph::Dice:
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(3.5, 3.5, 17, 17), 4.5, 4.5);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        for (const QPointF& dot : {QPointF(8.5, 8.5), QPointF(15.5, 8.5), QPointF(12, 12),
                                   QPointF(8.5, 15.5), QPointF(15.5, 15.5)}) {
            p.drawEllipse(dot, 1.5, 1.5);
        }
        break;
    case Glyph::Carousel:
        p.drawRoundedRect(QRectF(8, 5.5, 8, 13), 2, 2);
        p.drawRoundedRect(QRectF(2.5, 8.5, 3.5, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(18, 8.5, 3.5, 7), 1.5, 1.5);
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

/// Emblema da marca: quadrado arredondado com degrade da cor de destaque e uma flor de
/// laranjeira (azahar) branca no centro.
QPixmap MakeBrandBadge(const Theme& theme, int size, qreal dpr) {
    // Preferimos o icone do app (claro ou escuro, conforme a configuracao); a flor desenhada
    // abaixo e o plano B.
    const QIcon app_icon = QGuiApplication::windowIcon();
    if (!app_icon.isNull()) {
        const QPixmap from_icon = app_icon.pixmap(QSize(size, size), dpr);
        if (!from_icon.isNull()) {
            return from_icon;
        }
    }
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 1, size - 2, size - 2);
    QLinearGradient bg(r.topLeft(), r.bottomRight());
    bg.setColorAt(0.0, theme.accent_light);
    bg.setColorAt(1.0, theme.accent);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, size * 0.30, size * 0.30);
    // Brilho suave no topo.
    QLinearGradient shine(r.topLeft(), QPointF(r.left(), r.center().y()));
    shine.setColorAt(0.0, QColor(255, 255, 255, 70));
    shine.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(shine);
    p.drawRoundedRect(r, size * 0.30, size * 0.30);
    // Flor: cinco petalas.
    const QPointF c = r.center();
    p.translate(c);
    for (int i = 0; i < 5; ++i) {
        p.save();
        p.rotate(i * 72.0);
        p.setBrush(QColor(255, 255, 255, 245));
        p.setPen(QPen(QColor(0, 0, 0, 28), 0.6));
        p.drawEllipse(QRectF(-size * 0.095, -size * 0.36, size * 0.19, size * 0.30));
        p.restore();
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xF7, 0xB5, 0x2C));
    p.drawEllipse(QPointF(0, 0), size * 0.085, size * 0.085);
    p.setBrush(QColor(0xFF, 0xDF, 0x7A));
    p.drawEllipse(QPointF(-size * 0.02, -size * 0.02), size * 0.035, size * 0.035);
    return pm;
}

/// Moeda dourada (estilo "Moeda de Jogo" do 3DS) com uma estrela no centro.
QPixmap MakeCoin(int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 1, size - 2, size - 2);
    QLinearGradient rim(r.topLeft(), r.bottomRight());
    rim.setColorAt(0.0, QColor(0xFF, 0xE0, 0x6B));
    rim.setColorAt(1.0, QColor(0xD9, 0x96, 0x12));
    p.setPen(QPen(QColor(0xB8, 0x77, 0x0A), 1.0));
    p.setBrush(rim);
    p.drawEllipse(r);
    const QRectF inner = r.adjusted(size * 0.11, size * 0.11, -size * 0.11, -size * 0.11);
    QLinearGradient face(inner.topLeft(), inner.bottomRight());
    face.setColorAt(0.0, QColor(0xFF, 0xF1, 0xA8));
    face.setColorAt(1.0, QColor(0xF2, 0xB7, 0x2A));
    p.setPen(QPen(QColor(0xD9, 0x96, 0x12, 200), 0.8));
    p.setBrush(face);
    p.drawEllipse(inner);
    QPainterPath star;
    const QPointF c = r.center();
    const qreal outer = size * 0.27;
    const qreal inside = outer * 0.45;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2 + i * M_PI / 5;
        const qreal radius = (i % 2 == 0) ? outer : inside;
        const QPointF pt(c.x() + radius * std::cos(angle), c.y() + radius * std::sin(angle));
        if (i == 0) {
            star.moveTo(pt);
        } else {
            star.lineTo(pt);
        }
    }
    star.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xC9, 0x84, 0x0C));
    p.drawPath(star);
    return pm;
}

/// Barra lateral classica das capas de jogos de 3DS: faixa branca com o quadrado laranja da
/// "Nintendo Network" no topo, o logotipo "NINTENDO 3DS" na vertical (lido de baixo para cima,
/// com o "3" vermelho) e um filete preto na borda direita. Deve ser desenhada com o recorte
/// arredondado da capa ativo.
void DrawCaseSpine(QPainter* p, const QRectF& cover) {
    const qreal width = std::max<qreal>(10.0, cover.width() * 0.17);
    const QRectF strip(cover.left(), cover.top(), width, cover.height());
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0xFA, 0xFA, 0xFA));
    p->drawRect(strip);
    p->setBrush(QColor(0x1A, 0x1A, 0x1A));
    p->drawRect(QRectF(strip.right() - std::max<qreal>(1.0, width * 0.07), strip.top(),
                       std::max<qreal>(1.0, width * 0.07), strip.height()));

    // Quadrado laranja com o "n" da Nintendo Network.
    const qreal badge = width * 0.62;
    const QRectF badge_rect(strip.left() + (width - badge) / 2 - width * 0.03,
                            strip.top() + width * 0.18, badge, badge);
    p->setBrush(QColor(0xE8, 0x7B, 0x2B));
    p->drawRoundedRect(badge_rect, badge * 0.2, badge * 0.2);
    if (badge >= 7.0) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(Qt::white, std::max<qreal>(0.8, badge * 0.11), Qt::SolidLine,
                       Qt::RoundCap, Qt::RoundJoin));
        QPainterPath n;
        const QPointF o = badge_rect.topLeft();
        const auto pt = [&](qreal x, qreal y) { return o + QPointF(x * badge, y * badge); };
        n.moveTo(pt(0.30, 0.72));
        n.lineTo(pt(0.30, 0.34));
        n.cubicTo(pt(0.30, 0.26), pt(0.36, 0.24), pt(0.42, 0.30));
        n.lineTo(pt(0.58, 0.50));
        n.cubicTo(pt(0.64, 0.58), pt(0.70, 0.56), pt(0.70, 0.46));
        n.lineTo(pt(0.70, 0.28));
        p->drawPath(n);
    }

    // Logotipo na vertical, de baixo para cima.
    const qreal top_limit = badge_rect.bottom() + width * 0.18;
    const qreal available = strip.bottom() - width * 0.22 - top_limit;
    if (width >= 12.0 && available > width * 2.0) {
        qreal big = width * 0.66;
        qreal small = width * 0.30;
        const auto make = [](qreal px) {
            QFont font = BodyFont(std::max(4, static_cast<int>(px)), QFont::Black);
            font.setPixelSize(std::max(4, static_cast<int>(px)));
            return font;
        };
        const auto total = [&](qreal b, qreal s) {
            return QFontMetricsF(make(s)).horizontalAdvance(QStringLiteral("NINTENDO")) +
                   b * 0.25 + QFontMetricsF(make(b)).horizontalAdvance(QStringLiteral("3DS"));
        };
        const qreal length = total(big, small);
        if (length > available) {
            const qreal k = available / length;
            big *= k;
            small *= k;
        }
        p->translate(strip.left() + width * 0.5 - width * 0.03, strip.bottom() - width * 0.22);
        p->rotate(-90);
        const QFont small_font = make(small);
        const QFont big_font = make(big);
        const QFontMetricsF small_metrics(small_font);
        const QFontMetricsF big_metrics(big_font);
        p->setFont(small_font);
        p->setPen(QColor(0x1A, 0x1A, 0x1A));
        p->drawText(QPointF(0, small_metrics.ascent() / 2 - small_metrics.descent() / 2 +
                                   small_metrics.descent() * 0.2),
                    QStringLiteral("NINTENDO"));
        qreal x = small_metrics.horizontalAdvance(QStringLiteral("NINTENDO")) + big * 0.25;
        const qreal baseline = big_metrics.ascent() / 2 - big_metrics.descent() / 2 +
                               big_metrics.descent() * 0.2;
        p->setFont(big_font);
        p->setPen(QColor(0xE4, 0x1E, 0x26));
        p->drawText(QPointF(x, baseline), QStringLiteral("3"));
        x += big_metrics.horizontalAdvance(QStringLiteral("3"));
        p->setPen(QColor(0x1A, 0x1A, 0x1A));
        p->drawText(QPointF(x, baseline), QStringLiteral("DS"));
    }
    p->restore();
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
        glass.setColorAt(0.0, QColor(255, 255, 255, 248));
        glass.setColorAt(1.0, QColor(250, 252, 255, 244));
        const QColor tint = property("tint").value<QColor>();
        if (tint.isValid()) {
            // Destaque: a cor da capa tinge suavemente o lado esquerdo do painel.
            glass = QLinearGradient(body.topLeft(), body.topRight());
            glass.setColorAt(0.0, Theme::Mix(QColor(255, 255, 255), tint, 0.24f));
            glass.setColorAt(0.55, Theme::Mix(QColor(255, 255, 255), tint, 0.07f));
            glass.setColorAt(1.0, QColor(252, 253, 255));
        }
        QBrush glass_brush(glass);
        if (tint.isValid() && property("tint_center").toBool()) {
            // Biblioteca em carrossel: a cor da capa vira um clarao suave atras do jogo.
            QRadialGradient glow(QPointF(body.center().x(), body.top() + body.height() * 0.40),
                                 body.width() * 0.6);
            glow.setColorAt(0.0, Theme::Mix(QColor(255, 255, 255), tint, 0.30f));
            glow.setColorAt(0.6, Theme::Mix(QColor(255, 255, 255), tint, 0.09f));
            glow.setColorAt(1.0, QColor(252, 253, 255));
            glass_brush = QBrush(glow);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(glass_brush);
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
                dot.setAlpha(20);
                t.setPen(Qt::NoPen);
                t.setBrush(dot);
                t.drawEllipse(QPointF(9, 9), 1.4, 1.4);
            }
            p.setBrush(QBrush(tile));
            p.drawPath(body_path);
        }

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(31, 42, 55, 22), 1.0));
        p.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }

private:
    const Theme* theme;
    qreal radius;
    bool dotted;
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

/// Maior retangulo com a proporcao da imagem que cabe dentro de `box` (centralizado).
/// Usado nas capas reais: elas aparecem inteiras, sem recorte e sem distorcao.
static QRectF FitRect(const QPixmap& pixmap, const QRectF& box) {
    if (pixmap.isNull() || pixmap.height() == 0) {
        return box;
    }
    const qreal ratio = static_cast<qreal>(pixmap.width()) / pixmap.height();
    qreal w = box.width();
    qreal h = w / ratio;
    if (h > box.height()) {
        h = box.height();
        w = h * ratio;
    }
    return QRectF(box.center().x() - w / 2, box.center().y() - h / 2, w, h);
}

/// Estilo do icone dos jogos: 0 = normal (estojo), 1 = cartucho de 3DS.
static int g_icon_style = 0;

/// Texto do rodape da etiqueta do cartucho: "CTR-<codigo>-<regiao>" (ex.: CTR-BNDP-EUR).
static QString CartridgeFooter(const QString& product_id, const QString& region) {
    QString reg = region.section(QLatin1Char(','), 0, 0).trimmed().left(3).toUpper();
    QString text = QStringLiteral("CTR");
    if (!product_id.isEmpty()) {
        text += QLatin1Char('-') + product_id.toUpper();
    }
    if (!reg.isEmpty()) {
        text += QLatin1Char('-') + reg;
    }
    return text;
}

/// Retangulo do cartucho dentro do espaco quadrado do icone (o cartucho real e quase quadrado).
static QRectF CartridgeRect(const QRectF& slot) {
    const qreal w = slot.width() * 0.92;
    const qreal h = w * 0.99;
    return QRectF(slot.center().x() - w / 2, slot.center().y() - h / 2, w, h);
}

/// Poligono com os cantos suavizados (curvas quadraticas em cada vertice).
static QPainterPath RoundedPolygonPath(const QPolygonF& pts, const std::vector<qreal>& radii) {
    QPainterPath path;
    const int n = pts.size();
    for (int i = 0; i < n; ++i) {
        const QPointF prev = pts[(i + n - 1) % n];
        const QPointF cur = pts[i];
        const QPointF next = pts[(i + 1) % n];
        const QPointF d1 = prev - cur;
        const QPointF d2 = next - cur;
        const qreal l1 = std::hypot(d1.x(), d1.y());
        const qreal l2 = std::hypot(d2.x(), d2.y());
        const qreal r = std::min({radii[i], l1 / 2, l2 / 2});
        const QPointF a = cur + d1 / l1 * r;
        const QPointF b = cur + d2 / l2 * r;
        if (i == 0) {
            path.moveTo(a);
        } else {
            path.lineTo(a);
        }
        path.quadTo(cur, b);
    }
    path.closeSubpath();
    return path;
}

/// Contorno do cartucho de 3DS: "orelha" no canto superior direito, degrau no canto inferior
/// esquerdo e chanfro no inferior direito, como o cartucho real.
static QPainterPath CartridgePath(const QRectF& r) {
    const auto pt = [&r](qreal nx, qreal ny) {
        return QPointF(r.left() + nx * r.width(), r.top() + ny * r.height());
    };
    const qreal big = r.width() * 0.028;
    const qreal small = r.width() * 0.012;
    const QPolygonF pts({pt(0.0, 0.0), pt(1.0, 0.0), pt(1.0, 0.175), pt(0.952, 0.215),
                         pt(0.952, 0.90), pt(0.905, 1.0), pt(0.075, 1.0), pt(0.035, 0.965),
                         pt(0.035, 0.905), pt(0.0, 0.875)});
    return RoundedPolygonPath(pts, {big, big, small, small, small, big, big, small, small, small});
}

/// Desenha o jogo como um cartucho de 3DS: corpo cinza claro, etiqueta branca com o logotipo no
/// topo, a arte do jogo no meio e uma faixa de rodape.
static void PaintCartridge(QPainter* p, const QRectF& slot, const QPixmap& icon, bool real_cover,
                           const QString& initial, const QString& footer, const Theme& theme,
                           bool selected, bool with_shadow) {
    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                      QPainter::TextAntialiasing);
    const QRectF r = CartridgeRect(slot);
    const qreal w = r.width();
    const qreal h = r.height();
    const QPainterPath body = CartridgePath(r);

    if (with_shadow) {
        for (int i = 0; i < 6; ++i) {
            QColor c = selected ? theme.accent : theme.shadow;
            c.setAlphaF(selected ? 0.10 : 0.07);
            QPainterPath shifted = body;
            shifted.translate(0, 8);
            p->setPen(QPen(c, i * 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(c);
            p->drawPath(shifted);
        }
    }
    if (selected) {
        p->setBrush(theme.accent);
        p->setPen(QPen(theme.accent, 16, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(body);
        p->setBrush(Qt::white);
        p->setPen(QPen(Qt::white, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(body);
    }

    // Corpo de plastico cinza claro.
    QLinearGradient shell(r.topLeft(), r.bottomRight());
    shell.setColorAt(0.0, QColor(0xEE, 0xEF, 0xF0));
    shell.setColorAt(1.0, QColor(0xD2, 0xD4, 0xD8));
    p->setPen(QPen(QColor(0x9A, 0x9E, 0xA5), std::max(1.0, w * 0.008)));
    p->setBrush(shell);
    p->drawPath(body);

    // Etiqueta branca: quase a face toda do cartucho.
    const QRectF label(r.left() + w * 0.075, r.top() + h * 0.076, w * 0.795, h * 0.873);
    const qreal label_radius = w * 0.035;
    QPainterPath label_path;
    label_path.addRoundedRect(label, label_radius, label_radius);
    p->setPen(QPen(QColor(0, 0, 0, 40), std::max(1.0, w * 0.008)));
    p->setBrush(Qt::white);
    p->drawPath(label_path);

    const qreal header_h = label.height() * 0.155;
    const qreal footer_h = label.height() * 0.165;
    const QRectF header(label.left(), label.top(), label.width(), header_h);
    const QRectF art(label.left(), header.bottom(), label.width(),
                     label.height() - header_h - footer_h);
    const QRectF footer_rect(label.left(), art.bottom(), label.width(), footer_h);

    // Arte do jogo.
    p->save();
    p->setClipRect(art);
    if (!icon.isNull()) {
        if (real_cover) {
            p->fillRect(art, QColor(0xF4, 0xF6, 0xFA));
            p->drawPixmap(FitRect(icon, art), icon, QRectF(icon.rect()));
        } else {
            p->drawPixmap(art, icon, AspectFillSource(icon, art));
        }
    } else {
        QLinearGradient fill(art.topLeft(), art.bottomRight());
        fill.setColorAt(0.0, theme.accent_light);
        fill.setColorAt(1.0, theme.accent);
        p->fillRect(art, fill);
        p->setPen(Qt::white);
        p->setFont(RoundedFont(std::max(8, static_cast<int>(art.height() * 0.5)), QFont::Bold));
        p->drawText(art, Qt::AlignCenter, initial);
    }
    p->restore();

    // Logotipo no topo da etiqueta e regiao no rodape (somem quando o cartucho e pequeno).
    if (w >= 70.0) {
        const auto make = [](qreal px) {
            QFont font = BodyFont(std::max(4, static_cast<int>(px)), QFont::Black);
            font.setPixelSize(std::max(4, static_cast<int>(px)));
            return font;
        };
        static const QPixmap logo(QStringLiteral(":/home3ds/nintendo3ds_logo.png"));
        if (!logo.isNull()) {
            const QRectF box = header.adjusted(header.width() * 0.10, header.height() * 0.16,
                                               -header.width() * 0.10, -header.height() * 0.16);
            const qreal k = std::min(box.width() / logo.width(), box.height() / logo.height());
            const QSizeF size(logo.width() * k, logo.height() * k);
            p->drawPixmap(QRectF(QPointF(box.center().x() - size.width() / 2,
                                         box.center().y() - size.height() / 2),
                                 size),
                          logo, QRectF(logo.rect()));
        }
        if (!footer.isEmpty()) {
            p->setFont(make(footer_h * 0.34));
            p->setPen(QColor(0x3A, 0x40, 0x48));
            p->drawText(footer_rect.adjusted(footer_rect.width() * 0.06, 0, 0, 0),
                        Qt::AlignLeft | Qt::AlignVCenter, footer);
        }
    }
    p->restore();
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

    QColor Base() const {
        return base;
    }

    void SetGame(const QPixmap& icon_, const QString& title, bool real_cover_ = false,
                 const QString& region_ = QString()) {
        icon = icon_;
        region = region_;
        real_cover = real_cover_;
        initial = title.isEmpty() ? QString() : title.left(1).toUpper();
        const qint64 key = icon.isNull() ? 0 : icon.cacheKey();
        if (key != 0 && key == base_key) {
            // mesma imagem: reaproveita a cor media ja calculada
        } else {
            base = AverageColor(icon, theme->accent_light);
            base_key = key;
        }
        animation->stop();
        animation->start();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0, 14, 0, -14);
        if (r.width() < 40 || r.height() < 40) {
            return;
        }

        p.setPen(Qt::NoPen);

        // Capa em tamanho normal, quadrada, no centro.
        const qreal size = std::min<qreal>(r.height() * 0.82, 196.0) * (0.92 + 0.08 * fade);
        QRectF icon_rect(r.center().x() - size / 2, r.center().y() - size / 2 - 4, size, size);
        const bool boxed = real_cover && !icon.isNull();
        const QRectF square_rect = icon_rect;
        if (boxed) {
            icon_rect = FitRect(icon, icon_rect);
        }
        const qreal radius = boxed ? 10.0 : size * 0.16;
        p.setOpacity(fade);
        if (g_icon_style == 1) {
            PaintCartridge(&p, square_rect, icon, boxed, initial, region, *theme, false, true);
            return;
        }
        for (int i = 0; i < 8; ++i) {
            QColor c = theme->shadow;
            c.setAlphaF(0.06);
            p.setBrush(c);
            p.drawRoundedRect(icon_rect.adjusted(-i * 1.6, -i * 1.6 + 14, i * 1.6, i * 1.6 + 14),
                              radius + i * 1.6, radius + i * 1.6);
        }
        p.setBrush(Qt::white);
        p.drawRoundedRect(icon_rect.adjusted(-4, -4, 4, 4), radius + 4, radius + 4);

        QPainterPath icon_clip;
        icon_clip.addRoundedRect(icon_rect, radius, radius);
        p.setClipPath(icon_clip);
        if (!icon.isNull()) {
            p.drawPixmap(icon_rect, icon, AspectFillSource(icon, icon_rect));
        } else {
            QLinearGradient fill(icon_rect.topLeft(), icon_rect.bottomRight());
            fill.setColorAt(0.0, theme->accent_light);
            fill.setColorAt(1.0, theme->accent);
            p.fillRect(icon_rect, fill);
            p.setPen(Qt::white);
            p.setFont(RoundedFont(static_cast<int>(size * 0.45), QFont::Bold));
            p.drawText(icon_rect, Qt::AlignCenter, initial);
        }
        if (!boxed) {
            DrawCaseSpine(&p, icon_rect);
        }
    }

private:
    const Theme* theme;
    QPixmap icon;
    bool real_cover = false;
    QString region;
    QString initial;
    QColor base;
    QVariantAnimation* animation = nullptr;
    qint64 base_key = 0;
    qreal fade = 1.0;
};

/// Desenha a capa de um jogo dentro do quadrado `slot` (ja com escala e deslocamento aplicados):
/// sombra, moldura de selecao, arte (icone com barra 3DS ou capa real inteira) e estrela.
static void PaintTile(QPainter* p, const QRectF& slot, const GameInfo& game, const Theme& theme,
                      bool selected, qreal radius_scale) {
    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                      QPainter::TextAntialiasing);
    QRectF rect = slot;
    // Capa real (baixada ou escolhida): aparece inteira, na proporcao original.
    const bool boxed = game.custom_cover && !game.icon.isNull();
    if (boxed) {
        rect = FitRect(game.icon, slot);
    }
    const qreal side = slot.width();
    if (g_icon_style == 1) {
        rect = CartridgeRect(slot);
        PaintCartridge(p, slot, game.icon, boxed,
                       game.title.isEmpty() ? QString() : game.title.left(1).toUpper(),
                       CartridgeFooter(game.product_id, game.region), theme, selected, true);
    } else {
        const qreal radius = (boxed ? 12.0 : 27.0) * radius_scale;

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
            p->setFont(RoundedFont(std::max(8, static_cast<int>(side * 0.45)), QFont::Bold));
            p->drawText(rect, Qt::AlignCenter,
                        game.title.isEmpty() ? QString() : game.title.left(1).toUpper());
        }
        QLinearGradient gloss(rect.topLeft(), QPointF(rect.left(), rect.top() + side * 0.45));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 70));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        p->fillRect(rect, gloss);
        if (!boxed) {
            DrawCaseSpine(p, rect);
        }
        p->setClipping(false);
    }

    if (game.favorite) {
        p->save();
        const qreal k = std::clamp(side / 96.0, 0.8, 1.4);
        p->translate(rect.right() - 16 * k, rect.top() - 10 * k);
        p->scale(23.0 * k / 24.0, 23.0 * k / 24.0);
        p->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(theme.favorite);
        p->drawPolygon(StarPolygon());
        p->restore();
    }
    p->restore();
}

/// Carrossel horizontal estilo menu HOME do 3DS: o jogo selecionado fica grande no centro e
/// os vizinhos diminuem e esmaecem para os lados. Usa o mesmo modelo e a mesma selecao da grade.
class CarouselWidget : public QWidget {
public:
    CarouselWidget(HomeView* view_, QWidget* parent) : QWidget(parent), view(view_) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setMinimumHeight(static_cast<int>(kSelectedBase * 0.55 + kTop + 128));
        animation = new QVariantAnimation(this);
        animation->setDuration(340);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            position = value.toReal();
            update();
        });
    }

    /// Recalcula os jogos visiveis e anima ate o selecionado.
    void Refresh(bool animate) {
        rows.clear();
        for (int row = 0; row < view->model->rowCount(); ++row) {
            if (!view->grid->isRowHidden(row)) {
                rows.push_back(row);
            }
        }
        current = 0;
        const QModelIndex index = view->grid->currentIndex();
        if (index.isValid()) {
            const auto it = std::find(rows.begin(), rows.end(), index.row());
            if (it != rows.end()) {
                current = static_cast<int>(it - rows.begin());
            }
        }
        animation->stop();
        if (animate && isVisible() && !rows.empty()) {
            animation->setStartValue(
                std::clamp(position, 0.0, static_cast<qreal>(rows.size() - 1)));
            animation->setEndValue(static_cast<qreal>(current));
            animation->start();
        } else {
            position = current;
        }
        update();
    }

protected:
    /// Desenha um jogo reaproveitando uma imagem ja pronta (capa, sombra, lombada e textos sao
    /// caros). A imagem e feita uma vez no tamanho do selecionado e so escalada durante a
    /// animacao, o que deixa a navegacao fluida.
    void DrawTile(QPainter* p, const QRectF& rect, int i, bool selected) {
        constexpr qreal kMargin = 36.0;
        const GameInfo& game = GameAt(i);
        const int side = std::max(8, qRound(Sel()));
        const qreal dpr = devicePixelRatioF();
        const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10")
                                .arg(game.path)
                                .arg(selected)
                                .arg(side)
                                .arg(g_icon_style)
                                .arg(game.icon.isNull() ? 0 : game.icon.cacheKey())
                                .arg(game.favorite)
                                .arg(view->theme.accent.rgb())
                                .arg(game.product_id, game.region)
                                .arg(dpr);
        QPixmap* cached = tile_cache.object(key);
        if (cached == nullptr) {
            const int full = static_cast<int>(side + 2 * kMargin);
            auto* pm = new QPixmap(QSize(full, full) * dpr);
            pm->setDevicePixelRatio(dpr);
            pm->fill(Qt::transparent);
            {
                QPainter tp(pm);
                PaintTile(&tp, QRectF(kMargin, kMargin, side, side), game, view->theme, selected,
                          side / kIconSize);
            }
            tile_cache.insert(key, pm);
            cached = pm;
        }
        const qreal k = rect.width() / side;
        const QRectF target(rect.left() - kMargin * k, rect.top() - kMargin * k,
                            (side + 2 * kMargin) * k, (side + 2 * kMargin) * k);
        p->drawPixmap(target, *cached, QRectF(QPointF(0, 0), QSizeF(cached->size())));
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                         QPainter::TextAntialiasing);
        if (rows.empty()) {
            return;
        }
        // Brilho suave da cor de destaque atras do jogo selecionado.
        {
            const QPointF center(width() / 2.0, Top() + Sel() / 2.0 + 8);
            p.save();
            p.translate(center);
            p.scale(1.0, 0.5); // achata o circulo numa elipse que nao encosta no topo
            QRadialGradient glow(QPointF(0, 0), Sel() * 1.3);
            QColor c = view->theme.accent;
            c.setAlphaF(0.20);
            glow.setColorAt(0.0, c);
            c.setAlphaF(0.0);
            glow.setColorAt(1.0, c);
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(QPointF(0, 0), Sel() * 1.3, Sel() * 1.3);
            p.restore();
        }
        std::vector<int> order = VisibleItems();
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            return std::abs(a - position) > std::abs(b - position); // mais longe primeiro
        });
        for (const int i : order) {
            qreal opacity = 1.0;
            QRectF rect = TileRect(i, &opacity);
            if (opacity <= 0.01) {
                continue;
            }
            // O selecionado "sobe" um pouco em relacao aos vizinhos.
            rect.translate(0, -12.0 * std::max(0.0, 1.0 - std::abs(i - position)));
            // As capas das pontas esmaecem em vez de serem cortadas pela borda do painel.
            const qreal edge = std::min(rect.center().x(), width() - rect.center().x());
            opacity *= std::clamp(edge / 110.0, 0.0, 1.0);
            if (opacity <= 0.01) {
                continue;
            }
            p.setOpacity(opacity);
            DrawTile(&p, rect, i, std::abs(i - position) < 0.5);
        }
        p.setOpacity(1.0);

        const GameInfo& game = GameAt(current);
        const qreal cx = width() / 2.0;
        const qreal title_top = Top() + Sel() + 22;
        const QFont title_font = RoundedFont(22, QFont::DemiBold);
        p.setFont(title_font);
        p.setPen(view->theme.text);
        const QRectF title_rect(cx - 320, title_top, 640, 32);
        p.drawText(title_rect, Qt::AlignHCenter | Qt::AlignVCenter,
                   QFontMetrics(title_font)
                       .elidedText(game.title, Qt::ElideRight, static_cast<int>(title_rect.width())));
        p.setFont(BodyFont(14, QFont::DemiBold));
        p.setPen(view->theme.text_muted);
        QString line = QStringLiteral("%1 / %2").arg(current + 1).arg(rows.size());
        if (!game.play_time_text.isEmpty()) {
            line = game.play_time_text + QStringLiteral("   \u00B7   ") + line;
        }
        const QRectF line_rect(cx - 320, title_rect.bottom(), 640, 24);
        p.drawText(line_rect, Qt::AlignHCenter | Qt::AlignVCenter, line);

        // Barra de posicao: mostra onde o jogo atual esta na lista.
        if (rows.size() > 1) {
            const qreal bar_w = 168.0;
            const QRectF track(cx - bar_w / 2, line_rect.bottom() + 8, bar_w, 4);
            QColor track_color = view->theme.text_muted;
            track_color.setAlphaF(0.22);
            p.setPen(Qt::NoPen);
            p.setBrush(track_color);
            p.drawRoundedRect(track, 2, 2);
            const qreal frac = std::clamp(position / (rows.size() - 1), 0.0, 1.0);
            const qreal thumb_w = std::max(22.0, bar_w / static_cast<qreal>(rows.size()));
            p.setBrush(view->theme.accent);
            p.drawRoundedRect(QRectF(track.left() + (bar_w - thumb_w) * frac, track.top(), thumb_w,
                                     track.height()),
                              2, 2);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        setCursor(HitTest(event->position()) >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        QWidget::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus();
        const int hit = HitTest(event->position());
        if (hit < 0) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (hit == current) {
            Activate();
        } else {
            Select(hit);
        }
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        const int hit = HitTest(event->pos());
        if (hit >= 0) {
            Select(hit);
            view->ShowContextMenuForCurrent(event->globalPos());
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        const QPoint delta = event->angleDelta();
        wheel_accum += std::abs(delta.x()) > std::abs(delta.y()) ? delta.x() : -delta.y();
        while (wheel_accum >= 90) {
            Select(current + 1);
            wheel_accum -= 90;
        }
        while (wheel_accum <= -90) {
            Select(current - 1);
            wheel_accum += 90;
        }
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
        case Qt::Key_Left:
            Select(current - 1);
            break;
        case Qt::Key_Right:
            Select(current + 1);
            break;
        case Qt::Key_Home:
            Select(0);
            break;
        case Qt::Key_End:
            Select(static_cast<int>(rows.size()) - 1);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            Activate();
            break;
        default:
            QWidget::keyPressEvent(event); // o HomeView trata os demais atalhos
            return;
        }
        event->accept();
    }

    void focusInEvent(QFocusEvent* event) override {
        QWidget::focusInEvent(event);
        update();
    }

private:
    /// O carrossel encolhe quando a janela e baixa, para o titulo e a barra sempre caberem.
    qreal Scale() const {
        return std::clamp((height() - kTop - 128.0) / kSelectedBase, 0.55, 1.4);
    }
    /// Sobra de altura: o conjunto (capas, titulo, barra) fica um pouco acima do centro.
    qreal Top() const {
        return kTop + std::max(0.0, (height() - (kTop + Sel() + 120.0)) * 0.4);
    }
    qreal Sel() const {
        return kSelectedBase * Scale();
    }
    qreal Side() const {
        return kSideBase * Scale();
    }
    qreal Gap() const {
        return kGapBase * Scale();
    }
    static constexpr qreal kSelectedBase = 208.0;
    static constexpr qreal kSideBase = 128.0;
    static constexpr qreal kGapBase = 40.0;
    static constexpr qreal kTop = 26.0;

    const GameInfo& GameAt(int i) const {
        const int game_index = view->model->index(rows[i], 0).data(GameRowRole).toInt();
        return view->games[game_index];
    }

    std::vector<int> VisibleItems() const {
        std::vector<int> items;
        const int first = std::max(0, static_cast<int>(std::floor(position)) - 6);
        const int last = std::min(static_cast<int>(rows.size()) - 1,
                                  static_cast<int>(std::ceil(position)) + 6);
        for (int i = first; i <= last; ++i) {
            items.push_back(i);
        }
        return items;
    }

    /// Posicao e tamanho do cartao `i` na posicao atual (fracionaria) da animacao.
    QRectF TileRect(int i, qreal* opacity) const {
        const qreal d = i - position;
        const qreal ad = std::abs(d);
        const qreal near_step = Sel() / 2 + Side() / 2 + Gap();
        qreal size;
        qreal offset;
        if (ad < 1.0) {
            size = Sel() - (Sel() - Side()) * ad;
            offset = ad * near_step;
        } else {
            size = Side() * std::pow(0.93, ad - 1.0);
            offset = near_step + (ad - 1.0) * (Side() * 0.92 + Gap());
        }
        *opacity = ad < 1.0 ? 1.0 : std::clamp(1.0 - 0.22 * (ad - 1.0), 0.0, 1.0);
        const qreal x = width() / 2.0 + (d < 0 ? -offset : offset);
        const qreal cy = Top() + Sel() / 2.0;
        return QRectF(x - size / 2, cy - size / 2, size, size);
    }

    int HitTest(const QPointF& pos) const {
        std::vector<int> order = VisibleItems();
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            return std::abs(a - position) < std::abs(b - position); // mais perto primeiro
        });
        for (const int i : order) {
            qreal opacity = 1.0;
            if (TileRect(i, &opacity).adjusted(-6, -6, 6, 6).contains(pos) && opacity > 0.2) {
                return i;
            }
        }
        return -1;
    }

    void Select(int i) {
        if (rows.empty()) {
            return;
        }
        i = std::clamp(i, 0, static_cast<int>(rows.size()) - 1);
        view->grid->setCurrentIndex(view->model->index(rows[i], 0));
    }

    void Activate() {
        if (rows.empty()) {
            return;
        }
        view->RequestLaunch(GameAt(current).path);
    }

    HomeView* view;
    std::vector<int> rows;
    int current = 0;
    QCache<QString, QPixmap> tile_cache{48};
    qreal position = 0.0;
    int wheel_accum = 0;
    QVariantAnimation* animation = nullptr;
};

/// Barra de dicas de controle no rodape, como nos consoles: teclas e o que cada uma faz.
/// Botao de controle redondo. `id`: 0 = baixo (confirmar), 1 = direita (voltar), 2 = esquerda,
/// 3 = cima. PlayStation: circulo branco com o simbolo colorido; Xbox: circulo colorido com a letra.
static QPixmap MakePadBadge(bool playstation, int id, int size, qreal dpr) {
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    if (playstation) {
        static const QColor kColors[4] = {QColor(0x4F, 0x7C, 0xF0), QColor(0xE8, 0x55, 0x55),
                                          QColor(0xD9, 0x6F, 0xB4), QColor(0x2F, 0xAE, 0x7A)};
        p.setPen(QPen(QColor(31, 42, 55, 46), 1.0));
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(0.8, 0.8, 22.4, 22.4));
        p.setPen(QPen(kColors[id], 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        switch (id) {
        case 0:
            p.drawLine(QPointF(8, 8), QPointF(16, 16));
            p.drawLine(QPointF(16, 8), QPointF(8, 16));
            break;
        case 1:
            p.drawEllipse(QPointF(12, 12), 4.8, 4.8);
            break;
        case 2:
            p.drawRoundedRect(QRectF(7.6, 7.6, 8.8, 8.8), 1.2, 1.2);
            break;
        default:
            p.drawPolygon(QPolygonF({QPointF(12, 6.4), QPointF(17.6, 16.2), QPointF(6.4, 16.2)}));
            break;
        }
    } else {
        static const QColor kColors[4] = {QColor(0x3F, 0xA9, 0x4B), QColor(0xE0, 0x4B, 0x43),
                                          QColor(0x3F, 0x7F, 0xE0), QColor(0xE8, 0xA8, 0x0C)};
        static const char kLetters[4] = {'A', 'B', 'X', 'Y'};
        p.setPen(Qt::NoPen);
        p.setBrush(kColors[id]);
        p.drawEllipse(QRectF(0.8, 0.8, 22.4, 22.4));
        p.setPen(Qt::white);
        p.setFont(RoundedFont(13, QFont::Bold));
        p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, QString(QLatin1Char(kLetters[id])));
    }
    return pixmap;
}

/// Titulos das colunas da lista; usa a mesma conta de posicoes que `CardDelegate::PaintRow`.
class ListHeader : public QWidget {
public:
    ListHeader(HomeView* view_, QWidget* parent) : QWidget(parent), view(view_) {
        setFixedHeight(22);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    HomeView* view;
};

class HintBar : public QWidget {
public:
    HintBar(const Theme* theme_, QWidget* parent) : QWidget(parent), theme(theme_) {
        setFixedHeight(34);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void SetHints(QList<QPair<QString, QString>> new_hints) {
        hints = std::move(new_hints);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        const QFont key_font = BodyFont(12, QFont::Bold);
        const QFont label_font = BodyFont(13, QFont::DemiBold);
        const QFontMetrics key_metrics(key_font);
        const QFontMetrics label_metrics(label_font);
        constexpr int kKeyHeight = 22;
        constexpr int kGapInside = 8;
        constexpr int kGapBetween = 24;

        // Dicas com "@" viram o botao redondo do controle: "@p0" = PlayStation, "@x0" = Xbox.
        const auto key_width_of = [&](const QString& key) {
            return key.startsWith(QLatin1Char('@')) ? 24
                                                    : std::max(28, key_metrics.horizontalAdvance(key) + 16);
        };
        const auto measure = [&](const QList<QPair<QString, QString>>& list) {
            int sum = 0;
            for (const auto& hint : list) {
                sum += key_width_of(hint.first) + kGapInside +
                       label_metrics.horizontalAdvance(hint.second) + kGapBetween;
            }
            return sum - kGapBetween;
        };
        // Janela estreita: tira as dicas menos importantes (as ultimas), em vez de cortar.
        QList<QPair<QString, QString>> shown = hints;
        while (shown.size() > 3 && measure(shown) > width() - 16) {
            shown.removeLast();
        }
        const int total = measure(shown);
        int x = std::max(0, (width() - total) / 2);
        const int cy = height() / 2;
        for (const auto& hint : shown) {
            const int key_width = key_width_of(hint.first);
            const QRectF cap(x, cy - kKeyHeight / 2.0, key_width, kKeyHeight);
            if (hint.first.startsWith(QLatin1Char('@'))) {
                p.drawPixmap(QPointF(x, cy - 12), MakePadBadge(hint.first[1] == QLatin1Char('p'),
                                                                 hint.first[2].digitValue(), 24,
                                                                 devicePixelRatioF()));
            } else {
                // Tecla: base mais escura embaixo para dar relevo.
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 40));
                p.drawRoundedRect(cap.adjusted(0, 2, 0, 2), 7, 7);
                p.setBrush(QColor(0xFF, 0xFF, 0xFF));
                p.drawRoundedRect(cap, 7, 7);
                p.setFont(key_font);
                p.setPen(QColor(0x2B, 0x36, 0x44));
                p.drawText(cap, Qt::AlignCenter, hint.first);
            }
            x += key_width + kGapInside;
            p.setFont(label_font);
            p.setPen(theme->on_bg);
            p.drawText(QRectF(x, 0, label_metrics.horizontalAdvance(hint.second) + 2, height()),
                       Qt::AlignLeft | Qt::AlignVCenter, hint.second);
            x += label_metrics.horizontalAdvance(hint.second) + kGapBetween;
        }
    }

private:
    const Theme* theme;
    QList<QPair<QString, QString>> hints;
};

/// Camada por cima de tudo para as transicoes: abertura do app (nome do app que some) e
/// abertura de jogo (a capa cresce enquanto a tela escurece).
class Curtain : public QWidget {
public:
    Curtain(HomeView* view_, QWidget* parent) : QWidget(parent), view(view_) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        animation = new QVariantAnimation(this);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            t = value.toReal();
            update();
        });
        connect(animation, &QVariantAnimation::finished, this, [this] { OnFinished(); });
        hide();
    }

    bool IntroPlayed() const {
        return intro_played;
    }

    void StartIntro() {
        intro_played = true;
        Run(Mode::Intro, 1200, QEasingCurve::Linear, 0.0, 1.0);
    }

    void StartLaunch(const GameInfo& game_, const QRectF& from_, std::function<void()> done_) {
        game = game_;
        from = from_;
        done = std::move(done_);
        Run(Mode::Launch, 420, QEasingCurve::InOutCubic, 0.0, 1.0);
    }

    void FadeOut(int ms) {
        if (mode == Mode::Hold) {
            Run(Mode::Fade, ms, QEasingCurve::InOutQuad, 1.0, 0.0);
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                         QPainter::TextAntialiasing);
        const Theme& theme = view->theme;
        const QColor veil_color(0x0B, 0x12, 0x20);
        if (mode == Mode::Intro) {
            // Segura o nome do app e depois revela a tela.
            const qreal fade = t < 0.45 ? 0.0 : std::clamp((t - 0.45) / 0.55, 0.0, 1.0);
            const qreal alpha = 1.0 - fade * fade * (3.0 - 2.0 * fade);
            p.setOpacity(alpha);
            p.fillRect(rect(), theme.bg);
            p.setPen(theme.on_bg);
            QFont name = RoundedFont(58, QFont::Bold);
            p.setFont(name);
            p.translate(rect().center());
            const qreal scale = 1.0 + 0.06 * t;
            p.scale(scale, scale);
            p.drawText(QRectF(-300, -52, 600, 70), Qt::AlignCenter, QStringLiteral("Hanami"));
            QFont tag = BodyFont(13, QFont::Bold);
            tag.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
            p.setFont(tag);
            p.setPen(theme.on_bg_muted);
            p.drawText(QRectF(-300, 24, 600, 24), Qt::AlignCenter,
                       QStringLiteral("NINTENDO 3DS EMULATOR"));
            return;
        }
        // Abertura de jogo (e o escurecimento que fica enquanto o jogo carrega).
        QColor veil = veil_color;
        veil.setAlphaF(0.94 * (mode == Mode::Launch ? t : t));
        p.fillRect(rect(), veil);
        if (mode == Mode::Launch) {
            const qreal side = std::min(width(), height()) * 0.46;
            const QRectF target(rect().center().x() - side / 2, rect().center().y() - side / 2,
                                side, side);
            const auto lerp = [this](qreal a, qreal b) { return a + (b - a) * t; };
            const QRectF now(lerp(from.left(), target.left()), lerp(from.top(), target.top()),
                             lerp(from.width(), target.width()),
                             lerp(from.height(), target.height()));
            p.setOpacity(t < 0.8 ? 1.0 : 1.0 - (t - 0.8) / 0.2 * 0.6);
            PaintTile(&p, now, game, theme, false, now.width() / kIconSize);
        }
    }

private:
    enum class Mode { Idle, Intro, Launch, Hold, Fade };

    void Run(Mode new_mode, int ms, QEasingCurve::Type curve, qreal start, qreal end) {
        animation->stop();
        mode = new_mode;
        t = start;
        setGeometry(parentWidget()->rect());
        raise();
        show();
        animation->setDuration(ms);
        animation->setEasingCurve(curve);
        animation->setStartValue(start);
        animation->setEndValue(end);
        animation->start();
    }

    void OnFinished() {
        switch (mode) {
        case Mode::Launch: {
            mode = Mode::Hold;
            t = 1.0;
            update();
            auto callback = std::move(done);
            done = nullptr;
            if (callback) {
                callback();
            }
            break;
        }
        case Mode::Intro:
        case Mode::Fade:
            mode = Mode::Idle;
            hide();
            break;
        default:
            break;
        }
    }

    HomeView* view;
    QVariantAnimation* animation = nullptr;
    Mode mode = Mode::Idle;
    qreal t = 0.0;
    bool intro_played = false;
    GameInfo game;
    QRectF from;
    std::function<void()> done;
};

/// Desenha cada jogo como um cartao arredondado, como no menu HOME do 3DS.
class CardDelegate : public QStyledItemDelegate {
public:
    CardDelegate(HomeView* view_, QObject* parent) : QStyledItemDelegate(parent), view(view_) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        if (view->list_mode) {
            return QSize(std::max(240, view->grid->viewport()->width()), kRowHeight);
        }
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

        if (view->list_mode) {
            PaintRow(p, option.rect, game, theme, selected, hover);
            return;
        }

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
        PaintTile(p, QRectF(center.x() - side / 2, center.y() - side / 2, side, side), game,
                  theme, selected, scale);

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
    /// Linha da lista detalhada: capa pequena, titulo/subtitulo e colunas a direita.
    static void PaintRow(QPainter* p, const QRect& cell, const GameInfo& game, const Theme& theme,
                         bool selected, bool hover) {
        p->save();
        p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
                          QPainter::TextAntialiasing);
        const QRectF row = QRectF(cell).adjusted(4, 4, -4, -4);
        p->setPen(Qt::NoPen);
        if (selected) {
            p->setBrush(Theme::Mix(Qt::white, theme.accent, 0.10f));
            p->setPen(QPen(theme.accent, 1.5));
        } else if (hover) {
            p->setBrush(QColor(0xEE, 0xF3, 0xFA));
        } else {
            p->setBrush(QColor(0xF7, 0xF9, 0xFC));
        }
        p->drawRoundedRect(row.adjusted(0.75, 0.75, -0.75, -0.75), 18, 18);
        p->setPen(Qt::NoPen);

        const qreal icon_side = row.height() - 16;
        const QRectF icon_rect(row.left() + 10, row.top() + 8, icon_side, icon_side);
        const bool boxed = game.custom_cover && !game.icon.isNull();
        if (g_icon_style == 1) {
            PaintCartridge(p, icon_rect, game.icon, boxed,
                           game.title.isEmpty() ? QString() : game.title.left(1).toUpper(),
                           CartridgeFooter(game.product_id, game.region), theme, false, false);
        } else {
            const QRectF art_rect = boxed ? FitRect(game.icon, icon_rect) : icon_rect;
            QPainterPath clip;
            clip.addRoundedRect(art_rect, boxed ? 7 : 13, boxed ? 7 : 13);
            p->save();
            p->setClipPath(clip);
            if (!game.icon.isNull()) {
                p->drawPixmap(art_rect, game.icon, AspectFillSource(game.icon, art_rect));
            } else {
                QLinearGradient fill(icon_rect.topLeft(), icon_rect.bottomRight());
                fill.setColorAt(0.0, theme.accent_light);
                fill.setColorAt(1.0, theme.accent);
                p->fillRect(icon_rect, fill);
                p->setPen(Qt::white);
                p->setFont(RoundedFont(22, QFont::Bold));
                p->drawText(icon_rect, Qt::AlignCenter,
                            game.title.isEmpty() ? QString() : game.title.left(1).toUpper());
            }
            if (!boxed) {
                DrawCaseSpine(p, icon_rect);
            }
            p->restore();
        }

        // Colunas da direita (somem em janelas estreitas).
        qreal right = row.right() - 18;
        const auto column = [&](qreal width, const QString& text, int flags, const QColor& color,
                                const QFont& font) {
            if (right - width < row.left() + 300 || text.isEmpty()) {
                return;
            }
            p->setFont(font);
            p->setPen(color);
            p->drawText(QRectF(right - width, row.top(), width, row.height()),
                        flags | Qt::AlignVCenter, text);
            right -= width + 10;
        };
        column(110, game.play_time_text, Qt::AlignRight, theme.text_muted,
               BodyFont(13, QFont::DemiBold));
        column(80, game.size_text, Qt::AlignRight, theme.text_muted, BodyFont(13, QFont::DemiBold));
        column(54, game.region, Qt::AlignHCenter, theme.text_muted, BodyFont(13, QFont::Bold));
        if (right - 130 > row.left() + 300 && !game.compat_text.isEmpty()) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(CompatColor(game.compat_key)));
            p->drawEllipse(QPointF(right - 130 + 6, row.center().y()), 5, 5);
            p->setFont(BodyFont(13, QFont::DemiBold));
            p->setPen(theme.text_body);
            p->drawText(QRectF(right - 130 + 18, row.top(), 112, row.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, game.compat_text);
            right -= 140;
        }

        // Titulo e subtitulo.
        const qreal text_left = icon_rect.right() + 16;
        const qreal star_space = game.favorite ? 26.0 : 0.0;
        const qreal text_width = std::max<qreal>(60.0, right - text_left - 8 - star_space);
        const QFont title_font = RoundedFont(16, QFont::DemiBold);
        const QFontMetrics title_metrics(title_font);
        const QString title = title_metrics.elidedText(game.title, Qt::ElideRight,
                                                       static_cast<int>(text_width));
        p->setFont(title_font);
        p->setPen(selected ? theme.accent : theme.text);
        p->drawText(QRectF(text_left, row.top() + 12, text_width, 26),
                    Qt::AlignLeft | Qt::AlignVCenter, title);
        if (game.favorite) {
            p->save();
            p->translate(text_left + title_metrics.horizontalAdvance(title) + 8,
                         row.top() + 14);
            p->scale(18.0 / 24.0, 18.0 / 24.0);
            p->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(theme.favorite);
            p->drawPolygon(StarPolygon());
            p->restore();
        }
        p->setFont(BodyFont(13, QFont::Medium));
        p->setPen(theme.text_muted);
        p->drawText(QRectF(text_left, row.top() + 38, text_width, 22),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(BodyFont(13, QFont::Medium))
                        .elidedText(game.subtitle, Qt::ElideRight, static_cast<int>(text_width)));
        p->restore();
    }

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
    brand_layout->setSpacing(14);
    brand_badge = new QLabel(brand);
    brand_badge->setFixedSize(52, 52);
    brand_badge->setPixmap(MakeBrandBadge(theme, 52, dpr));
    brand_layout->addWidget(brand_badge);
    auto* brand_text = new QVBoxLayout;
    brand_text->setSpacing(0);
    auto* app_name = new QLabel(tr("Hanami"), brand);
    app_name->setObjectName(QStringLiteral("appName"));
    app_name->setFont(RoundedFont(27, QFont::Bold));
    auto* app_tag = new QLabel(tr("NINTENDO 3DS EMULATOR"), brand);
    app_tag->setObjectName(QStringLiteral("appTag"));
    QFont tag_font = BodyFont(11, QFont::Bold);
    tag_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    app_tag->setFont(tag_font);
    brand_text->addWidget(app_name);
    brand_text->addWidget(app_tag);
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
    console_button = new QPushButton(right);
    console_button->setObjectName(QStringLiteral("topButton"));
    console_button->setCursor(Qt::PointingHandCursor);
    console_button->setCheckable(true);
    console_button->setToolTip(tr("Console mode (fullscreen)") +
                               QStringLiteral("  \u2022  Ctrl+Shift+F"));
    console_button->setAccessibleName(tr("Console mode (fullscreen)"));
    console_button->setIcon(MakeIcon(Glyph::Fullscreen, theme.text, 22, dpr));
    console_button->setIconSize(QSize(22, 22));
    // O estado (marcado ou nao) vem do menu; o botao so pede a troca.
    connect(console_button, &QPushButton::clicked, this, [this](bool checked) {
        console_button->setChecked(!checked); // volta ao estado real ate o app confirmar
        emit ConsoleModeRequested();
    });
    coin_pill = new QFrame(right);
    coin_pill->setObjectName(QStringLiteral("pill"));
    coin_pill->setFixedHeight(46);
    coin_pill->setToolTip(tr("Total play time across your library"));
    auto* coin_layout = new QHBoxLayout(coin_pill);
    coin_layout->setContentsMargins(8, 0, 20, 0);
    coin_layout->setSpacing(10);
    auto* coin_icon = new QLabel(coin_pill);
    coin_icon->setFixedSize(32, 32);
    coin_icon->setPixmap(MakeCoin(32, dpr));
    coin_label = new QLabel(coin_pill);
    coin_label->setObjectName(QStringLiteral("coinText"));
    coin_layout->addWidget(coin_icon);
    coin_layout->addWidget(coin_label);
    coin_pill->hide();
    right_layout->addWidget(coin_pill);
    right_layout->addWidget(battery_pill);
    right_layout->addWidget(console_button);
    right_layout->addWidget(settings_button);
    console_emblem = new QLabel(right);
    console_emblem->setFixedSize(50, 50);
    console_emblem->setAlignment(Qt::AlignCenter);
    console_emblem->setPixmap(MakeGlyph(Glyph::Console, theme.on_bg, 46, dpr));
    right_layout->addWidget(console_emblem);
    top_layout->addWidget(right, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(top);

    // ---- Jogo em destaque ----
    auto* hero = new GlassPanel(&theme, 32, false, this);
    hero_panel = hero;
    hero->setFixedHeight(280 + kShadowTop + kShadowBottom);
    auto* hero_layout = new QHBoxLayout(hero);
    hero_layout->setContentsMargins(GlassPanel::Padding(0, 0, 0, 0));
    hero_layout->setSpacing(0);

    auto* info = new QWidget(hero);
    auto* info_layout = new QVBoxLayout(info);
    info_layout->setContentsMargins(14, 24, 36, 24);
    info_layout->setSpacing(8);

    hero_eyebrow = new QLabel(info);
    hero_eyebrow->setObjectName(QStringLiteral("heroEyebrow"));
    hero_eyebrow->setFont(BodyFont(12, QFont::Bold));
    info_layout->addWidget(hero_eyebrow);

    hero_title = new QLabel(info);
    hero_title->setObjectName(QStringLiteral("heroTitle"));
    hero_title->setFont(RoundedFont(34, QFont::Bold));
    hero_title->setWordWrap(true);
    hero_title->setMaximumHeight(84);
    info_layout->addWidget(hero_title);

    hero_desc = new QLabel(info);
    hero_desc->setObjectName(QStringLiteral("heroDesc"));
    hero_desc->setFont(BodyFont(15, QFont::DemiBold));
    hero_desc->setWordWrap(true);
    hero_desc->setMaximumHeight(42);
    info_layout->addWidget(hero_desc);
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
    info_layout->addSpacing(4);
    info_layout->addLayout(chips);

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
            RequestLaunch(game->path);
        }
    });
    actions->addWidget(play_button);

    favorite_button = new QPushButton(info);
    favorite_button->setObjectName(QStringLiteral("roundButton"));
    favorite_button->setCursor(Qt::PointingHandCursor);
    favorite_button->setCheckable(true);
    favorite_button->setToolTip(tr("Favorite"));
    favorite_button->setAccessibleName(tr("Favorite"));
    favorite_button->setIconSize(QSize(22, 22));
    connect(favorite_button, &QPushButton::clicked, this, [this] {
        if (const GameInfo* game = CurrentGame()) {
            PlaySound(static_cast<int>(SoundBank::Sound::Favorite));
            emit FavoriteToggleRequested(game->program_id);
        }
    });
    actions->addWidget(favorite_button);
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

    hero_hours = new QLabel(info);
    hero_hours->setObjectName(QStringLiteral("heroHours"));
    hero_hours->setFont(BodyFont(14, QFont::Bold));
    actions->addSpacing(6);
    actions->addWidget(hero_hours, 0, Qt::AlignVCenter);

    actions->addStretch(1);
    info_layout->addLayout(actions);

    hero_art = new HeroArt(&theme, hero);
    hero_art->setFixedWidth(272);
    hero_layout->addSpacing(10);
    hero_layout->addWidget(hero_art, 0);
    hero_layout->addWidget(info, 1);
    root->addWidget(hero);

    // ---- Grade de jogos ----
    auto* grid_panel = new GlassPanel(&theme, 32, true, this);
    grid_panel_widget = grid_panel;
    tint_anim = new QVariantAnimation(this);
    tint_anim->setDuration(380);
    tint_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(tint_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        tint_now = value.value<QColor>();
        ApplyTint();
    });
    grid_layout = new QVBoxLayout(grid_panel);
    grid_layout->setContentsMargins(GlassPanel::Padding(28, 18, 28, 12));
    grid_layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    grid_title = new QLabel(tr("My Games"), grid_panel);
    grid_title->setObjectName(QStringLiteral("sectionTitle"));
    grid_title->setFont(RoundedFont(25, QFont::DemiBold));
    grid_title->setTextFormat(Qt::RichText);
    header->addWidget(grid_title, 0, Qt::AlignVCenter);
    header->addStretch(1);


    sort_button = new QPushButton(grid_panel);
    sort_button->setObjectName(QStringLiteral("sortPill"));
    sort_button->setCursor(Qt::PointingHandCursor);
    sort_button->setFont(BodyFont(13, QFont::DemiBold));
    sort_button->setToolTip(tr("Sort") + QStringLiteral("  \u2022  Select"));
    sort_button->setIcon(MakeIcon(Glyph::Sort, theme.ink, 16, dpr));
    sort_button->setIconSize(QSize(16, 16));
    auto* sort_menu = new QMenu(sort_button);
    const auto add_sort = [this, sort_menu](const QString& text, SortMode mode) {
        QAction* action = sort_menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, mode] {
            SetSortMode(mode);
            emit PreferencesChanged();
        });
    };
    add_sort(tr("Recently played"), SortMode::Recent);
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
    auto* mode_group = new QButtonGroup(this);
    mode_group->setExclusive(true);
    const struct {
        Glyph glyph;
        bool filled;
        const char* tip;
    } modes[3] = {{Glyph::Carousel, false, QT_TR_NOOP("Carousel view")},
                  {Glyph::Grid, true, QT_TR_NOOP("Grid view")},
                  {Glyph::List, false, QT_TR_NOOP("List view")}};
    for (int i = 0; i < 3; ++i) {
        auto* button = new QPushButton(segment);
        button->setObjectName(QStringLiteral("segButton"));
        button->setCheckable(true);
        button->setChecked(i == 0);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tr(modes[i].tip) + QStringLiteral("  \u2022  L2 / R2"));
        button->setAccessibleName(tr(modes[i].tip));
        QIcon icon;
        icon.addPixmap(MakeGlyph(modes[i].glyph, theme.ink, 16, dpr), QIcon::Normal, QIcon::Off);
        icon.addPixmap(MakeGlyph(modes[i].glyph, Qt::white, 16, dpr, modes[i].filled),
                       QIcon::Normal, QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(16, 16));
        mode_group->addButton(button, i);
        mode_buttons[i] = button;
        segment_layout->addWidget(button);
    }
    connect(mode_group, &QButtonGroup::idClicked, this, [this](int id) {
        PlaySound(static_cast<int>(SoundBank::Sound::Move));
        SetViewMode(id);
        emit PreferencesChanged();
    });
    header->addWidget(segment);
    grid_layout->addLayout(header);

    // Filtros rapidos (Todos / Favoritos / Jogados) e o botao de jogo aleatorio.
    auto* chip_row = new QHBoxLayout;
    chip_row->setSpacing(8);
    auto* quick_group = new QButtonGroup(this);
    quick_group->setExclusive(true);
    for (int i = 0; i < 3; ++i) {
        auto* chip = new QPushButton(grid_panel);
        chip->setObjectName(QStringLiteral("filterChip"));
        chip->setCheckable(true);
        chip->setChecked(i == 0);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFont(BodyFont(13, QFont::Bold));
        quick_group->addButton(chip, i);
        quick_buttons[i] = chip;
        chip_row->addWidget(chip);
    }
    connect(quick_group, &QButtonGroup::idClicked, this, [this](int id) {
        PlaySound(static_cast<int>(SoundBank::Sound::Move));
        SetQuickFilter(id);
        emit PreferencesChanged();
    });
    chip_row->addStretch(1);
    auto* surprise = new QPushButton(tr("Surprise me"), grid_panel);
    surprise_button = surprise;
    surprise->setObjectName(QStringLiteral("surpriseChip"));
    surprise->setCursor(Qt::PointingHandCursor);
    surprise->setFont(BodyFont(13, QFont::Bold));
    surprise->setToolTip(tr("Surprise me") + QStringLiteral("  \u2022  R"));
    surprise->setIcon(MakeIcon(Glyph::Dice, theme.accent, 16, dpr));
    surprise->setIconSize(QSize(16, 16));
    connect(surprise, &QPushButton::clicked, this, [this] { PickRandomGame(); });
    chip_row->addWidget(surprise);
    grid_layout->addLayout(chip_row);

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
    grid->viewport()->installEventFilter(this);
    list_header = new ListHeader(this, grid_panel);
    list_header->hide();
    grid_layout->addWidget(list_header);
    grid_layout->addWidget(grid, 1);
    grid->installEventFilter(this);
    carousel = new CarouselWidget(this, grid_panel);
    grid_layout->addWidget(carousel, 1);

    empty_state = new QWidget(grid_panel);
    auto* empty_layout = new QVBoxLayout(empty_state);
    empty_layout->setAlignment(Qt::AlignCenter);
    empty_layout->setSpacing(14);
    empty_icon = new QLabel(empty_state);
    empty_icon->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_icon);
    empty_label = new QLabel(empty_state);
    empty_label->setObjectName(QStringLiteral("emptyLabel"));
    empty_label->setTextFormat(Qt::RichText);
    empty_label->setFont(RoundedFont(20, QFont::Bold));
    empty_label->setAlignment(Qt::AlignCenter);
    empty_label->setWordWrap(true);
    empty_layout->addWidget(empty_label);
    empty_add_button = new QPushButton(tr("Add games folder"), empty_state);
    empty_add_button->setObjectName(QStringLiteral("play"));
    empty_add_button->setCursor(Qt::PointingHandCursor);
    empty_add_button->setFont(RoundedFont(17, QFont::Bold));
    connect(empty_add_button, &QPushButton::clicked, this, [this] { emit AddFolderRequested(); });
    empty_layout->addWidget(empty_add_button, 0, Qt::AlignHCenter);
    grid_layout->addWidget(empty_state, 1);
    empty_state->hide();

    hero_timer = new QTimer(this);
    hero_timer->setSingleShot(true);
    hero_timer->setInterval(45);
    connect(hero_timer, &QTimer::timeout, this, [this] { UpdateHero(); });
    connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) {
                // O carrossel anima na hora; o destaque (textos, cor, icones) e atualizado em
                // seguida, uma vez so, para a navegacao rapida nao engasgar.
                carousel->Refresh(true);
                hero_timer->start();
                if (isVisible()) {
                    PlaySound(static_cast<int>(SoundBank::Sound::Move));
                }
            });
    connect(grid, &QListView::activated, this, [this](const QModelIndex& index) {
        const int row = index.data(GameRowRole).toInt();
        if (row >= 0 && row < static_cast<int>(games.size())) {
            RequestLaunch(games[row].path);
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
    hints = new HintBar(&theme, this);
    root->addWidget(hints);

    clock_timer = new QTimer(this);
    clock_timer->setInterval(15000);
    connect(clock_timer, &QTimer::timeout, this, [this] { UpdateClock(); });
    clock_timer->start();
    curtain = new Curtain(this, this);
    gamepad = new GamepadNav(this);
    connect(gamepad, &GamepadNav::ButtonPressed, this,
            [this](int button) { HandlePadButton(button); });
    connect(gamepad, &GamepadNav::ConnectionChanged, this, [this](bool connected) {
        pad_hints = connected;
        UpdateHints();
    });
    carousel->installEventFilter(this);
    ApplyViewMode();
    UpdateHints();
}

void HomeView::RebuildStyleSheet() {
    if (brand_badge != nullptr) {
        brand_badge->setPixmap(MakeBrandBadge(theme, 52, devicePixelRatioF()));
    }
    if (console_emblem != nullptr) {
        console_emblem->setPixmap(MakeGlyph(Glyph::Console, theme.on_bg, 46, devicePixelRatioF()));
    }
    if (surprise_button != nullptr) {
        surprise_button->setIcon(MakeIcon(Glyph::Dice, theme.accent, 16, devicePixelRatioF()));
    }
    QString qss = QStringLiteral(R"(
#homeRoot QWidget { background: transparent; }
#homeRoot QLabel { background: transparent; color: @text@; }
#homeRoot QLabel#appName { color: @on_bg@; }
#homeRoot QLabel#appTag { color: @on_bg_muted@; }
#homeRoot QLabel#heroEyebrow { color: @accent@; }
#homeRoot QLabel#clockDate { color: @muted@; }
#homeRoot QLabel#coinText { color: @text@; background: transparent; }
#homeRoot QLabel#heroDesc { color: @body@; }
#homeRoot QLabel#heroHours { color: @muted@; }
#homeRoot QFrame#pill {
    background: #FFFFFF;
    border: 1px solid rgba(31, 42, 55, 26);
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
    background: #FFFFFF;
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
#homeRoot QPushButton#topButton:checked { background: #DCEBFB; border-color: @accent@; }
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
#homeRoot QPushButton#filterChip {
    background: #FFFFFF;
    border: 1px solid rgba(31, 42, 55, 34);
    border-radius: 16px;
    padding: 0px 16px;
    min-height: 32px; max-height: 32px;
    color: @ink@;
}
#homeRoot QPushButton:focus { border: 2px solid @accent@; }
#homeRoot QPushButton#play:focus { border: 2px solid #FFFFFF; }
#homeRoot QPushButton#filterChip:hover { background: #EEF3FA; border-color: @accent@; }
#homeRoot QPushButton#filterChip:checked { background: @accent@; border-color: @accent@; color: #FFFFFF; }
#homeRoot QPushButton#surpriseChip {
    background: #FFFFFF;
    border: 1px solid @accent@;
    border-radius: 16px;
    padding: 0px 16px 0px 12px;
    min-height: 32px; max-height: 32px;
    color: @accent@;
}
#homeRoot QPushButton#surpriseChip:hover { background: #E8F1FC; }
#homeRoot QPushButton#surpriseChip:pressed { background: #D4E5FA; }
#homeRoot QPushButton#dockItem {
    background: transparent;
    border: none;
    border-radius: 23px;
    padding: 0px 22px;
    min-height: 46px;
    color: @ink@;
}
#homeRoot QPushButton#dockItem:hover { background: #EEF3FA; }
#homeRoot QPushButton#dockItem:checked { background: #FFFFFF; color: @accent@; }
#homeRoot QListView#gameGrid { background: transparent; border: none; outline: none; }
#homeRoot QMenu { background: #FFFFFF; border: 1px solid rgba(31, 42, 55, 40); border-radius: 14px; padding: 6px; }
#homeRoot QMenu::item { padding: 8px 26px 8px 18px; border-radius: 9px; color: @text@; }
#homeRoot QMenu::item:selected { background: @accent@; color: #FFFFFF; }
#homeRoot QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 0px; }
#homeRoot QScrollBar::handle:vertical { background: rgba(31, 42, 55, 70); border-radius: 4px; min-height: 40px; }
#homeRoot QScrollBar::add-line:vertical, #homeRoot QScrollBar::sub-line:vertical { height: 0px; }
#homeRoot QScrollBar::add-page:vertical, #homeRoot QScrollBar::sub-page:vertical { background: transparent; }
)");
    qss.replace(QStringLiteral("@text@"), theme.text.name());
    qss.replace(QStringLiteral("@body@"), theme.text_body.name());
    qss.replace(QStringLiteral("@muted@"), theme.text_muted.name());
    qss.replace(QStringLiteral("@ink@"), theme.ink.name());
    qss.replace(QStringLiteral("@on_bg_muted@"), theme.on_bg_muted.name());
    qss.replace(QStringLiteral("@on_bg@"), theme.on_bg.name());
    qss.replace(QStringLiteral("@accent@"), theme.accent.name());
    qss.replace(QStringLiteral("@accent_light@"), theme.accent_light.name());
    qss.replace(QStringLiteral("@accent_hover@"), theme.accent_light.lighter(112).name());
    setStyleSheet(qss);
}

void HomeView::SetAccentColor(const QColor& accent) {
    accent_color = accent;
    theme = Theme::FromColors(accent_color, bg_top_color);
    ApplyLightPalette();
    RebuildStyleSheet();
    update();
    if (grid != nullptr) {
        grid->viewport()->update();
    }
    UpdateHero();
}

void HomeView::UpdateEmptyState(int visible) {
    visible_count = visible;
    const bool empty = visible == 0;
    ApplyViewMode();
    UpdateHeroSize();
    if (empty) {
        const bool no_games = games.empty();
        const bool searching = !filter_text.isEmpty();
        QString text;
        Glyph empty_glyph = Glyph::Search;
        if (no_games) {
            text = tr("No games yet.\nAdd the folder where your games are.");
            empty_glyph = Glyph::Folder;
        } else if (!searching && quick_filter == 1) {
            text = tr("No favorites yet.\nPress F on a game to add it.");
            empty_glyph = Glyph::Star;
        } else if (!searching && quick_filter == 2) {
            text = tr("Nothing played yet.\nGames you play show up here.");
            empty_glyph = Glyph::Home;
        } else {
            text = tr("No games match your search.");
        }
        const QStringList lines = text.split(QLatin1Char('\n'));
        QString html = QStringLiteral("<div style='font-size:24px; font-weight:700;'>%1</div>")
                           .arg(lines.value(0).toHtmlEscaped());
        if (lines.size() > 1) {
            html += QStringLiteral("<div style='font-size:15px; font-weight:500; color:%1;'>%2</div>")
                        .arg(theme.text_muted.name(), lines.value(1).toHtmlEscaped());
        }
        empty_label->setText(html);
        QColor icon_color = theme.text_muted;
        icon_color.setAlphaF(0.55);
        const qreal dpr = devicePixelRatioF();
        empty_icon->setPixmap(MakeGlyph(empty_glyph, icon_color, 72, dpr));
        empty_add_button->setVisible(no_games);
    }
}

void HomeView::SetBackgroundColor(const QColor& color) {
    bg_top_color = color;
    SetAccentColor(accent_color);
}

void HomeView::SetGames(std::vector<GameInfo> new_games) {
    games = std::move(new_games);
    qulonglong total_seconds = 0;
    for (const GameInfo& game : games) {
        total_seconds += game.play_time_seconds;
    }
    coin_pill->setVisible(total_seconds > 0);
    if (total_seconds > 0) {
        const qulonglong hours = total_seconds / 3600;
        const QString value = hours > 0 ? QString::number(hours) : QString::number(total_seconds / 60);
        const QString unit = hours > 0 ? tr("h played") : tr("min played");
        coin_label->setText(QStringLiteral("<span style='font-size:22px; font-weight:700;'>%1</span>"
                                           "&nbsp;<span style='font-size:13px;'>%2</span>")
                                .arg(value, unit.toHtmlEscaped()));
    }
    RefreshModel();
}

void HomeView::RefreshModel() {
    const bool was_muted = sound_mute;
    sound_mute = true;
    const qulonglong current_id = CurrentGame() != nullptr ? CurrentGame()->program_id : 0;

    UpdateQuickCounts();
    model->clear();
    for (int i = 0; i < static_cast<int>(games.size()); ++i) {
        const GameInfo& game = games[i];
        auto* item = new QStandardItem;
        item->setData(i, GameRowRole);
        item->setData(game.title.toLower(), SortNameRole);
        item->setData(game.play_time_seconds, SortTimeRole);
        item->setData(game.size_bytes, SortSizeRole);
        item->setData(game.last_played, SortRecentRole);
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
            select_row = row; // com a ordem "recentes", a primeira linha e o ultimo jogado
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
    sound_mute = was_muted;
    UpdateHero();
}

void HomeView::ApplySort() {
    switch (sort_mode) {
    case SortMode::Recent:
        // Mais recentes primeiro; quem nunca foi jogado fica em ordem alfabetica no fim.
        model->setSortRole(SortRecentRole);
        model->sort(0, Qt::DescendingOrder);
        break;
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
    case SortMode::Recent:
        return tr("Recently played");
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
    const bool was_muted = sound_mute;
    sound_mute = true;
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
        const bool quick_ok = quick_filter == 0 || (quick_filter == 1 && game.favorite) ||
                              (quick_filter == 2 && (game.last_played > 0 ||
                                                     game.play_time_seconds > 0));
        const bool match = quick_ok &&
                           std::all_of(words.begin(), words.end(), [&haystack](const QString& w) {
                               return haystack.contains(w);
                           });
        grid->setRowHidden(row, !match);
        if (match) {
            ++visible;
        }
    }
    grid_title->setText(
        QStringLiteral("%1 <span style=\"font-size:14px; font-weight:700; color:%2;\">%3</span>")
            .arg(tr("My Games").toHtmlEscaped(), theme.text_muted.name(),
                 tr("%n game(s)", nullptr, visible).toHtmlEscaped()));
    UpdateEmptyState(visible);

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
    carousel->Refresh(false);
    sound_mute = was_muted;
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
    UpdateQuickCounts();
    SetFilter(filter_text);
    grid->viewport()->update();
    UpdateHero();
}

void HomeView::UpdateQuickCounts() {
    int favorites = 0;
    int played = 0;
    for (const GameInfo& game : games) {
        favorites += game.favorite ? 1 : 0;
        played += (game.last_played > 0 || game.play_time_seconds > 0) ? 1 : 0;
    }
    quick_buttons[0]->setText(tr("All") + QStringLiteral("  %1").arg(games.size()));
    quick_buttons[1]->setText(tr("Favorites") + QStringLiteral("  %1").arg(favorites));
    quick_buttons[2]->setText(tr("Played") + QStringLiteral("  %1").arg(played));
}

void HomeView::PickRandomGame() {
    std::vector<int> rows;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!grid->isRowHidden(row)) {
            rows.push_back(row);
        }
    }
    if (rows.empty()) {
        return;
    }
    const int row = rows[QRandomGenerator::global()->bounded(static_cast<int>(rows.size()))];
    grid->setCurrentIndex(model->index(row, 0));
    grid->scrollTo(model->index(row, 0), QAbstractItemView::PositionAtCenter);
}

void HomeView::SetPreferences(int mode, int sort, int quick) {
    sort_mode = static_cast<SortMode>(std::clamp(sort, 0, 3));
    quick_filter = std::clamp(quick, 0, 2);
    quick_buttons[quick_filter]->setChecked(true);
    SetViewMode(mode);
    ApplySort();
}

void HomeView::SetQuickFilter(int mode) {
    quick_filter = mode;
    SetFilter(filter_text);
}

void HomeView::SetViewMode(int mode) {
    view_mode = std::clamp(mode, 0, 2);
    list_mode = view_mode == 2;
    mode_buttons[view_mode]->setChecked(true);
    UpdateGridMetrics();
    ApplyViewMode();
    UpdateHints();
    UpdateHeroSize();
    carousel->Refresh(false);
    UpdateTint();
    FocusActiveView();
}

void HomeView::ApplyViewMode() {
    const bool empty = visible_count == 0;
    grid->setVisible(!empty && view_mode != 0);
    carousel->setVisible(!empty && view_mode == 0);
    list_header->setVisible(!empty && view_mode == 2);
    empty_state->setVisible(empty);
}

void HomeView::FocusActiveView() {
    if (view_mode == 0) {
        carousel->setFocus();
    } else {
        grid->setFocus();
    }
}

bool HomeView::UsePlaystationIcons() const {
    return pad_style == 2 || (pad_style == 0 && pad_layout == 1);
}

void HomeView::SetIconStyle(int style) {
    style = std::clamp(style, 0, 1);
    if (style == g_icon_style) {
        return;
    }
    g_icon_style = style;
    if (grid != nullptr) {
        grid->viewport()->update();
        carousel->update();
        hero_art->update();
    }
}

void HomeView::SetPadStyle(int style) {
    pad_style = std::clamp(style, 0, 2);
    UpdateHints();
}

/// O icone do Play acompanha o que o jogador usa: botao do controle ou um triangulo de "play".
void HomeView::UpdatePlayIcon() {
    const qreal dpr = devicePixelRatioF();
    constexpr int kBadge = 32;
    constexpr int kGap = 12; // espaco entre o icone e a palavra "Play"
    QPixmap icon(QSize(kBadge + kGap, kBadge) * dpr);
    icon.setDevicePixelRatio(dpr);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setRenderHint(QPainter::Antialiasing);
    if (pad_hints) {
        p.drawPixmap(0, 0, MakePadBadge(UsePlaystationIcons(), 0, kBadge, dpr));
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(0, 0, kBadge, kBadge));
        p.setBrush(theme.accent);
        p.drawPolygon(QPolygonF({QPointF(12.5, 9.5), QPointF(12.5, 22.5), QPointF(23, 16)}));
    }
    p.end();
    play_button->setIcon(QIcon(icon));
    play_button->setIconSize(QSize(kBadge + kGap, kBadge));
}

void HomeView::UpdateHints() {
    UpdatePlayIcon();
    QList<QPair<QString, QString>> list;
    if (pad_hints) {
        // Botoes do controle: simbolos coloridos (PlayStation) ou letras (Xbox).
        const bool ps = UsePlaystationIcons();
        const QString kind = ps ? QStringLiteral("@p") : QStringLiteral("@x");
        list.append({tr("D-Pad"), tr("Navigate")});
        list.append({kind + QLatin1Char('0'), tr("Play")});
        list.append({kind + QLatin1Char('1'), tr("Home")});
        list.append({kind + QLatin1Char('2'), tr("Favorite")});
        list.append({kind + QLatin1Char('3'), tr("Random")});
        list.append({ps ? QStringLiteral("L1  R1") : QStringLiteral("L  R"), tr("Filter")});
        list.append({ps ? QStringLiteral("L2  R2") : QStringLiteral("LT  RT"), tr("View")});
        list.append({ps ? tr("Create") : tr("Select"), tr("Sort")});
        list.append({ps ? tr("Options") : tr("Menu"), tr("Settings")});
        hints->SetHints(list);
        return;
    }
    if (view_mode == 0) {
        list.append({QStringLiteral("\u2190 \u2192"), tr("Navigate")});
    } else if (view_mode == 1) {
        list.append({QStringLiteral("\u2190 \u2191 \u2193 \u2192"), tr("Navigate")});
    } else {
        list.append({QStringLiteral("\u2191 \u2193"), tr("Navigate")});
    }
    list.append({QStringLiteral("Enter"), tr("Play")});
    list.append({QStringLiteral("Esc"), tr("Home")});
    list.append({QStringLiteral("F"), tr("Favorite")});
    list.append({QStringLiteral("Q  E"), tr("Filter")});
    list.append({QStringLiteral("R"), tr("Random")});
    list.append({QStringLiteral("/"), tr("Search")});
    hints->SetHints(list);
}

/// Controle: traduz o botao apertado em uma tecla e entrega para a vista ativa, assim o
/// controle e o teclado compartilham exatamente o mesmo comportamento.
void HomeView::HandlePadButton(int button) {
    if (!isVisible() || launching || curtain->isVisible() || !window()->isActiveWindow() ||
        QApplication::activeModalWidget() != nullptr ||
        QApplication::activePopupWidget() != nullptr) {
        return; // outra janela, menu ou jogo tem o foco: o controle nao navega aqui
    }
    if (!pad_hints || pad_layout != (gamepad != nullptr ? gamepad->Layout() : 0)) {
        pad_hints = true;
        pad_layout = gamepad != nullptr ? gamepad->Layout() : 0;
        UpdateHints();
    }
    if (button == GamepadNav::Start) {
        emit SettingsRequested();
        return;
    }
    if (button == GamepadNav::Select) {
        CycleSort();
        return;
    }
    if (button == GamepadNav::L3) {
        emit ConsoleModeRequested();
        return;
    }
    if (button == GamepadNav::L2 || button == GamepadNav::R2) {
        // Gatilhos trocam o modo de visualizacao: carrossel, grade e lista.
        PlaySound(static_cast<int>(SoundBank::Sound::Move));
        SetViewMode((view_mode + (button == GamepadNav::R2 ? 1 : 2)) % 3);
        emit PreferencesChanged();
        return;
    }
    struct KeyMap {
        int key;
        const char* text;
    };
    static const KeyMap kMap[GamepadNav::ButtonCount] = {
        {Qt::Key_Up, ""},     {Qt::Key_Down, ""},   {Qt::Key_Left, ""}, {Qt::Key_Right, ""},
        {Qt::Key_Return, ""}, {Qt::Key_Escape, ""}, {Qt::Key_F, "f"},   {Qt::Key_R, "r"},
        {Qt::Key_Q, "q"},     {Qt::Key_E, "e"},     {Qt::Key_unknown, ""},
        {Qt::Key_unknown, ""}, {Qt::Key_unknown, ""}, {Qt::Key_unknown, ""},
        {Qt::Key_unknown, ""},
    };
    if (button < 0 || button >= GamepadNav::ButtonCount || kMap[button].key == Qt::Key_unknown) {
        return;
    }
    QWidget* target = QApplication::focusWidget();
    if (target == nullptr || qobject_cast<QLineEdit*>(target) != nullptr || !isAncestorOf(target)) {
        FocusActiveView();
        target = QApplication::focusWidget();
    }
    if (target == nullptr) {
        target = view_mode == 0 ? static_cast<QWidget*>(carousel) : static_cast<QWidget*>(grid);
    }
    injecting = true;
    const QString text = QString::fromLatin1(kMap[button].text);
    QKeyEvent press(QEvent::KeyPress, kMap[button].key, Qt::NoModifier, text);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, kMap[button].key, Qt::NoModifier, text);
    QApplication::sendEvent(target, &release);
    injecting = false;
}

/// Atalhos de console: F favorita, Q/E trocam o filtro, R sorteia, / busca, Esc volta ao inicio.
bool HomeView::HandleShortcut(QKeyEvent* event) {
    if (qobject_cast<QLineEdit*>(focusWidget()) != nullptr ||
        (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))) {
        return false;
    }
    switch (event->key()) {
    case Qt::Key_F:
        if (const GameInfo* game = CurrentGame()) {
            PlaySound(static_cast<int>(SoundBank::Sound::Favorite));
            emit FavoriteToggleRequested(game->program_id);
        }
        return true;
    case Qt::Key_Q:
    case Qt::Key_E: {
        const int step = event->key() == Qt::Key_E ? 1 : 2;
        SetQuickFilter((quick_filter + step) % 3);
        quick_buttons[quick_filter]->setChecked(true);
        emit PreferencesChanged();
        return true;
    }
    case Qt::Key_R:
        PickRandomGame();
        return true;
    case Qt::Key_Slash:
        emit SearchRequested();
        return true;
    case Qt::Key_Escape:
    case Qt::Key_Backspace:
        PlaySound(static_cast<int>(SoundBank::Sound::Back));
        dock_home->click();
        return true;
    case Qt::Key_Space:
        if (const GameInfo* game = CurrentGame()) {
            RequestLaunch(game->path);
        }
        return true;
    default:
        return false;
    }
}

void HomeView::keyPressEvent(QKeyEvent* event) {
    if (HandleShortcut(event)) {
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void HomeView::SetSoundOptions(const QString& dir, bool enabled) {
    if (sounds == nullptr) {
        sounds = new SoundBank(dir, this);
    }
    sounds->SetEnabled(enabled);
}

void HomeView::SetConsoleMode(bool enabled) {
    console_button->setChecked(enabled);
}

void HomeView::PlaySound(int sound) {
    if (sounds != nullptr && !sound_mute) {
        sounds->Play(static_cast<SoundBank::Sound>(sound));
    }
}

void HomeView::RequestLaunch(const QString& path) {
    if (launching || path.isEmpty()) {
        return;
    }
    const GameInfo* found = nullptr;
    for (const GameInfo& game : games) {
        if (game.path == path) {
            found = &game;
            break;
        }
    }
    if (found == nullptr || !isVisible() || width() < 300 || height() < 300) {
        emit GameActivated(path);
        return;
    }
    if (sounds != nullptr) {
        sounds->Play(SoundBank::Sound::Launch);
    }
    // De onde a capa "sai": o centro do carrossel, o destaque ou a celula da grade.
    QRectF from(width() / 2.0 - 60, height() / 2.0 - 60, 120, 120);
    if (carousel->isVisible()) {
        const QPointF c = carousel->mapTo(this, QPoint(carousel->width() / 2, 18 + 78));
        from = QRectF(c.x() - 78, c.y() - 78, 156, 156);
    } else if (hero_art->isVisible() && hero_panel->isVisible()) {
        const QPointF c = hero_art->mapTo(this, hero_art->rect().center());
        from = QRectF(c.x() - 85, c.y() - 85, 170, 170);
    } else if (grid->currentIndex().isValid()) {
        const QRect cell = grid->visualRect(grid->currentIndex());
        const QPointF c = grid->viewport()->mapTo(this, cell.center());
        from = QRectF(c.x() - kIconSize / 2.0, c.y() - kIconSize / 2.0 - 12, kIconSize, kIconSize);
    }
    launching = true;
    curtain->StartLaunch(*found, from, [this, path] {
        emit GameActivated(path);
        // Se o jogo nao abrir (erro, cancelado), a tela volta ao normal.
        QTimer::singleShot(2500, this, [this] {
            curtain->FadeOut(450);
            launching = false;
        });
    });
}

void HomeView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this] { FocusActiveView(); });
    if (!curtain->IntroPlayed()) {
        curtain->StartIntro();
        if (sounds != nullptr) {
            sounds->Play(SoundBank::Sound::Startup);
        }
        QTimer::singleShot(1200, this, [this] { sound_mute = false; });
    }
}

void HomeView::UpdateGridMetrics() {
    if (list_mode) {
        grid->setFlow(QListView::TopToBottom);
        grid->setWrapping(false);
        grid->setGridSize(QSize(std::max(240, grid->viewport()->width()), kRowHeight));
    } else {
        grid->setFlow(QListView::LeftToRight);
        grid->setWrapping(true);
        grid->setGridSize(QSize(kCellWidth, kCellHeight));
    }
    grid->doItemsLayout();
    grid->viewport()->update();
    const QModelIndex current = grid->currentIndex();
    if (current.isValid()) {
        grid->scrollTo(current);
    }
}

bool HomeView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress && !injecting && pad_hints &&
        (watched == grid || watched == carousel)) {
        pad_hints = false; // o jogador voltou ao teclado
        UpdateHints();
    }
    if (watched == grid && event->type() == QEvent::KeyPress &&
        HandleShortcut(static_cast<QKeyEvent*>(event))) {
        return true;
    }
    if (list_mode && grid != nullptr && watched == grid->viewport() &&
        event->type() == QEvent::Resize) {
        const int width = std::max(240, grid->viewport()->width());
        if (grid->gridSize().width() != width) {
            grid->setGridSize(QSize(width, kRowHeight));
        }
    }
    return QWidget::eventFilter(watched, event);
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
    // A biblioteca mostra todos os jogos de uma vez: o carrossel vira grade enquanto ela esta aberta.
    if (enabled && view_mode == 0) {
        library_switched = true;
        SetViewMode(1);
    } else if (!enabled && library_switched) {
        library_switched = false;
        SetViewMode(0);
    }
    UpdateHeroSize();
    dock_home->setChecked(!enabled);
    dock_library->setChecked(enabled);
}

void HomeView::FocusGrid() {
    FocusActiveView();
}

void HomeView::ShowContextMenuForCurrent(const QPoint& global_pos) {
    if (const GameInfo* game = CurrentGame()) {
        const GameInfo copy = *game; // a lista pode ser recarregada enquanto o menu esta aberto
        emit GameContextMenuRequested(copy, global_pos);
    }
}

void HomeView::UpdateHero() {
    if (hero_timer != nullptr) {
        hero_timer->stop();
    }
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
        hero_eyebrow->clear();
        hero_desc->clear();
        hero_hours->clear();
        hero_art->SetGame(QPixmap(), QString());
        hero_panel->setProperty("tint", QColor());
        hero_panel->update();
        favorite_button->setIcon(MakeIcon(Glyph::Star, theme.text, 22, dpr));
        UpdateTint();
        return;
    }

    hero_title->setText(game->title.isEmpty() ? tr("Unknown title") : game->title);
    hero_desc->setText(game->subtitle);

    qint64 latest = 0;
    for (const GameInfo& g : games) {
        latest = std::max(latest, g.last_played);
    }
    hero_eyebrow->setText(latest > 0 && game->last_played == latest ? tr("LAST PLAYED")
                                                                       : tr("SELECTED GAME"));
    hero_eyebrow->setVisible(!compact);

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
    hero_art->SetGame(game->icon, game->title, game->custom_cover,
                      CartridgeFooter(game->product_id, game->region));
    hero_panel->setProperty("tint", hero_art->Base());
    hero_panel->update();
    UpdateTint();
}

/// A cor da capa selecionada tinge o painel (e de leve o fundo) no modo carrossel.
void HomeView::UpdateTint() {
    const GameInfo* game = CurrentGame();
    const QColor target = (view_mode == 0 && game != nullptr) ? hero_art->Base() : QColor();
    tint_anim->stop();
    if (!target.isValid()) {
        tint_now = QColor();
        ApplyTint();
        return;
    }
    if (!tint_now.isValid() || !isVisible()) {
        tint_now = target;
        ApplyTint();
        return;
    }
    tint_anim->setStartValue(tint_now);
    tint_anim->setEndValue(target);
    tint_anim->start();
}

void HomeView::ApplyTint() {
    grid_panel_widget->setProperty("tint", tint_now);
    grid_panel_widget->setProperty("tint_center", true);
    grid_panel_widget->update();
    update();
}

void HomeView::CycleSort() {
    PlaySound(static_cast<int>(SoundBank::Sound::Move));
    SetSortMode(static_cast<SortMode>((static_cast<int>(sort_mode) + 1) % 4));
    emit PreferencesChanged();
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
    QColor base = theme.bg;
    if (tint_now.isValid()) {
        base = Theme::Mix(theme.bg, tint_now, 0.10f); // o fundo acompanha a capa, bem de leve
    }
    p.fillRect(rect(), base);
}

void HomeView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (curtain != nullptr && curtain->isVisible()) {
        curtain->setGeometry(rect());
    }
    UpdateHeroSize();
}

void HomeView::RefreshBrand() {
    if (brand_badge != nullptr) {
        brand_badge->setPixmap(MakeBrandBadge(theme, 52, devicePixelRatioF()));
    }
}

void HomeView::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ApplicationWindowIconChange && brand_badge != nullptr) {
        brand_badge->setPixmap(MakeBrandBadge(theme, 52, devicePixelRatioF()));
    }
}

void HomeView::UpdateHeroSize() {
    // Em janelas estreitas a arte do jogo some para dar espaco ao texto.
    // O destaque ocupa cerca de 29% da altura, para sobrar espaco para a grade.
    // Em janelas baixas ele fica compacto: so titulo e botoes (o carrossel encolhe sozinho).
    // Grade e lista escondem o destaque em janelas muito baixas; biblioteca e lista vazia tambem.
    const bool was_compact = compact;
    compact = height() < 1000;
    const int hero_height =
        compact ? 168 : std::clamp(static_cast<int>(height() * 0.29), 232, 300);
    hero_panel->setFixedHeight(hero_height + kShadowTop + kShadowBottom);
    hero_art->setVisible(width() >= 900 && !compact);
    hero_panel->setVisible(!library_mode && visible_count > 0 &&
                           (view_mode == 0 || height() >= 760));
    if (compact != was_compact) {
        UpdateHero();
    }
}

void ListHeader::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::TextAntialiasing);
    QFont font = BodyFont(11, QFont::Bold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    p.setFont(font);
    QColor color = view->theme.text_muted;
    color.setAlphaF(0.9);
    p.setPen(color);
    // Mesma conta de CardDelegate::PaintRow: colunas encaixadas a partir da direita.
    const qreal row_left = 4.0;
    qreal right = view->grid->viewport()->width() - 4.0 - 18.0;
    const auto column = [&](qreal width, const QString& text, int flags) {
        if (right - width < row_left + 300) {
            return;
        }
        p.drawText(QRectF(right - width, 0, width, height()), flags | Qt::AlignVCenter,
                   text.toUpper());
        right -= width + 10;
    };
    column(110, tr("Play time"), Qt::AlignRight);
    column(80, tr("Size"), Qt::AlignRight);
    column(54, tr("Region"), Qt::AlignHCenter);
    if (right - 130 > row_left + 300) {
        p.drawText(QRectF(right - 130 + 18, 0, 112, height()), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("Compatibility").toUpper());
    }
}

} // namespace Home
