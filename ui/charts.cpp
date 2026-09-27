#include "charts.hpp"
#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace dake::ui {

// --- BarChart -------------------------------------------------------------

BarChart::BarChart(QWidget* parent) : QWidget(parent) {}

void BarChart::setData(std::vector<ChartPoint> points) {
    points_ = std::move(points);
    update();
}

void BarChart::setSeries(const QString& first, const QString& second) {
    firstName_ = first;
    secondName_ = second;
    update();
}

void BarChart::setSigned(bool value) {
    signed_ = value;
    update();
}

QSize BarChart::sizeHint() const {
    return QSize(320, 160);
}

void BarChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool hasData = false;
    double max_val = 0.0;
    for (const auto& p : points_) {
        if (p.primary != 0.0 || p.secondary != 0.0) hasData = true;
        max_val = std::max({max_val, std::abs(p.primary), std::abs(p.secondary)});
    }

    if (!hasData || points_.empty() || max_val == 0.0) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin datos todavia"));
        return;
    }

    QFont labelFont = theme::bodyFont(8);
    QFontMetrics fm(labelFont);
    int labelHeight = fm.height();

    int topMargin = 4;
    bool hasLegend = !secondName_.isEmpty();
    if (hasLegend) {
        topMargin = labelHeight + 16;
    }

    double chartH = height() - topMargin - labelHeight - 8;
    if (chartH <= 0) return;

    double zeroY;
    if (signed_) {
        zeroY = topMargin + chartH / 2.0;
    } else {
        zeroY = topMargin + chartH;
    }

    double scale = signed_ ? (chartH / 2.0) / max_val : chartH / max_val;

    if (hasLegend) {
        painter.setFont(labelFont);
        int w1 = fm.horizontalAdvance(firstName_);
        int w2 = fm.horizontalAdvance(secondName_);
        int spacing = 20;
        int totalW = 12 + w1 + spacing + 12 + w2;
        int startX = (width() - totalW) / 2;
        int legendY = 4;
        
        painter.fillRect(QRectF(startX, legendY + (labelHeight - 8) / 2.0, 8, 8), theme::color(theme::kPositive));
        painter.setPen(theme::color(theme::kTextMuted));
        painter.drawText(QRectF(startX + 12, legendY, w1, labelHeight), Qt::AlignLeft | Qt::AlignVCenter, firstName_);
        
        startX += 12 + w1 + spacing;
        painter.fillRect(QRectF(startX, legendY + (labelHeight - 8) / 2.0, 8, 8), theme::color(theme::kNegative));
        painter.drawText(QRectF(startX + 12, legendY, w2, labelHeight), Qt::AlignLeft | Qt::AlignVCenter, secondName_);
    }

    painter.setPen(theme::color(theme::kBorder));
    painter.drawLine(QPointF(0, zeroY), QPointF(width(), zeroY));

    int N = static_cast<int>(points_.size());
    double step = static_cast<double>(width()) / N;
    double barGroupWidth = step * 0.7;

    for (int i = 0; i < N; ++i) {
        const auto& p = points_[i];
        double cx = (i + 0.5) * step;

        if (hasLegend) {
            double barW = barGroupWidth / 2.0;
            
            double h1 = std::abs(p.primary) * scale;
            double y1 = p.primary >= 0 ? zeroY - h1 : zeroY;
            if (!signed_) y1 = zeroY - h1;
            painter.fillRect(QRectF(cx - barW, y1, barW - 1, h1), theme::color(theme::kPositive));

            double h2 = std::abs(p.secondary) * scale;
            double y2 = p.secondary >= 0 ? zeroY - h2 : zeroY;
            if (!signed_) y2 = zeroY - h2;
            painter.fillRect(QRectF(cx, y2, barW - 1, h2), theme::color(theme::kNegative));
        } else {
            double barW = barGroupWidth;
            double h = std::abs(p.primary) * scale;
            double y = p.primary >= 0 ? zeroY - h : zeroY;
            if (!signed_) y = zeroY - h;
            
            QColor c = theme::kSerie;
            if (signed_) {
                c = p.primary >= 0 ? theme::kPositive : theme::kNegative;
            }
            painter.fillRect(QRectF(cx - barW / 2.0, y, barW, h), c);
        }
    }

    painter.setFont(labelFont);
    painter.setPen(theme::color(theme::kTextFaint));
    
    int skip = 1;
    int maxW = 0;
    for (const auto& p : points_) {
        maxW = std::max(maxW, fm.horizontalAdvance(p.label));
    }
    if (step > 0 && maxW + 8 > step) {
        skip = static_cast<int>(std::ceil((maxW + 8) / step));
    }
    
    for (int i = 0; i < N; i += skip) {
        double cx = (i + 0.5) * step;
        QString l = points_[i].label;
        int lw = fm.horizontalAdvance(l);
        painter.drawText(QRectF(cx - lw / 2.0, height() - labelHeight - 2, lw, labelHeight), Qt::AlignCenter, l);
    }
}


// --- LineChart ------------------------------------------------------------

LineChart::LineChart(QWidget* parent) : QWidget(parent) {}

void LineChart::setData(std::vector<ChartPoint> points) {
    points_ = std::move(points);
    update();
}

QSize LineChart::sizeHint() const {
    return QSize(320, 160);
}

void LineChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool hasData = false;
    double max_val = 0.0;
    for (const auto& p : points_) {
        if (p.primary != 0.0) hasData = true;
        max_val = std::max(max_val, std::abs(p.primary));
    }

    if (!hasData || points_.empty() || max_val == 0.0) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin datos todavia"));
        return;
    }

    QFont labelFont = theme::bodyFont(8);
    QFontMetrics fm(labelFont);
    int labelHeight = fm.height();

    double topMargin = 8.0;
    double chartH = height() - labelHeight - 8 - topMargin;
    if (chartH <= 0) return;
    
    double zeroY = topMargin + chartH;
    double scale = chartH / max_val;

    int N = static_cast<int>(points_.size());
    double step = static_cast<double>(width()) / N;

    QPainterPath linePath;
    QPainterPath fillPath;
    
    QVector<QPointF> poly;
    for (int i = 0; i < N; ++i) {
        double cx = (i + 0.5) * step;
        double cy = zeroY - std::abs(points_[i].primary) * scale;
        poly.push_back(QPointF(cx, cy));
    }

    if (!poly.isEmpty()) {
        linePath.moveTo(poly.first());
        for (int i = 1; i < poly.size(); ++i) {
            linePath.lineTo(poly[i]);
        }
        
        fillPath = linePath;
        fillPath.lineTo(poly.last().x(), zeroY);
        fillPath.lineTo(poly.first().x(), zeroY);
        fillPath.closeSubpath();
    }

    painter.setPen(theme::color(theme::kBorder));
    painter.drawLine(QPointF(0, zeroY), QPointF(width(), zeroY));

    QColor fillColor = theme::kSerie;
    fillColor.setAlpha(40);
    painter.fillPath(fillPath, fillColor);

    QPen linePen(theme::color(theme::kSerie), 2);
    painter.setPen(linePen);
    painter.drawPath(linePath);

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::color(theme::kSerie));
    for (const auto& pt : poly) {
        painter.drawEllipse(pt, 3, 3);
    }

    painter.setFont(labelFont);
    painter.setPen(theme::color(theme::kTextFaint));
    
    int skip = 1;
    int maxW = 0;
    for (const auto& p : points_) {
        maxW = std::max(maxW, fm.horizontalAdvance(p.label));
    }
    if (step > 0 && maxW + 8 > step) {
        skip = static_cast<int>(std::ceil((maxW + 8) / step));
    }
    
    for (int i = 0; i < N; i += skip) {
        double cx = (i + 0.5) * step;
        QString l = points_[i].label;
        int lw = fm.horizontalAdvance(l);
        painter.drawText(QRectF(cx - lw / 2.0, height() - labelHeight - 2, lw, labelHeight), Qt::AlignCenter, l);
    }
}


// --- RankChart ------------------------------------------------------------

RankChart::RankChart(QWidget* parent) : QWidget(parent) {}

void RankChart::setData(std::vector<ChartPoint> points) {
    points_ = std::move(points);
    updateGeometry();
    update();
}

void RankChart::setLimit(int rows) {
    limit_ = rows;
    updateGeometry();
    update();
}

QSize RankChart::sizeHint() const {
    if (points_.empty()) return QSize(320, 40);
    int count = std::min(static_cast<int>(points_.size()), std::max(1, limit_));
    int h = count * 30 - 6;
    return QSize(320, std::max(40, h));
}

void RankChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool hasData = false;
    for (const auto& p : points_) {
        if (p.primary != 0.0) { hasData = true; break; }
    }

    if (!hasData || points_.empty()) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin datos todavia"));
        return;
    }

    // Se ordena aca y no en cada pantalla que la usa. charts.hpp promete
    // "ordenadas de mayor a menor", y una promesa que cada usuario tiene que
    // cumplir por su cuenta se rompe en el segundo usuario: costByCategory ya
    // viene ordenado, pero jobResults viene por fecha de apertura.
    std::vector<ChartPoint> ordenados = points_;
    std::stable_sort(ordenados.begin(), ordenados.end(),
                     [](const ChartPoint& a, const ChartPoint& b) {
                         return std::abs(a.primary) > std::abs(b.primary);
                     });

    std::vector<ChartPoint> rows;
    int safeLimit = std::max(1, limit_);
    if (ordenados.size() > static_cast<size_t>(safeLimit)) {
        for (int i = 0; i < safeLimit - 1; ++i) {
            rows.push_back(ordenados[i]);
        }
        ChartPoint otros;
        otros.label = QStringLiteral("Otros");
        for (size_t i = safeLimit - 1; i < ordenados.size(); ++i) {
            otros.primary += ordenados[i].primary;
        }
        rows.push_back(otros);
    } else {
        rows = ordenados;
    }

    double max_val = 0.0;
    for (const auto& r : rows) {
        max_val = std::max(max_val, std::abs(r.primary));
    }

    if (max_val == 0.0) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin datos todavia"));
        return;
    }

    QFont font = theme::bodyFont(9);
    painter.setFont(font);
    
    int y = 0;
    // Se reserva una franja a la derecha para el valor, y la barra NO entra ahi.
    // Sin eso, la fila mas larga pinta la barra debajo de su propio numero y el
    // numero queda ilegible justo en la fila que mas importa, que es la de
    // arriba. El ancho sale de medir el texto mas largo, no de un numero magico.
    const QFontMetrics metrics(theme::bodyFont(9));
    int gutter = 0;
    for (const auto& r : rows) {
        gutter = std::max(gutter, metrics.horizontalAdvance(r.primaryText));
    }
    gutter += 12;
    const int barArea = std::max(20, width() - gutter);

    for (const auto& r : rows) {
        double ratio = std::abs(r.primary) / max_val;
        int barW = static_cast<int>(barArea * ratio);

        QColor barColor = theme::kSerie;
        barColor.setAlphaF(0.60F);   // el sufijo F: setAlphaF toma float, y sin el /W4 avisa
        painter.fillRect(QRectF(0, y, barW, 24), barColor);
        
        painter.setPen(theme::color(theme::kText));
        painter.drawText(QRectF(6, y, barArea - 12, 24), Qt::AlignLeft | Qt::AlignVCenter, r.label);
        
        if (!r.primaryText.isEmpty()) {
            painter.setPen(theme::color(theme::kTextMuted));
            painter.drawText(QRectF(6, y, width() - 12, 24), Qt::AlignRight | Qt::AlignVCenter, r.primaryText);
        }
        
        y += 30;
    }
}

// --- CompareChart ---------------------------------------------------------

namespace {
constexpr int kCompareRowHeight = 30;
constexpr int kCompareLabelWidth = 150;
} // namespace

CompareChart::CompareChart(QWidget* parent) : QWidget(parent) {}

void CompareChart::setData(std::vector<CompareRow> rows, theme::Tono barColor) {
    rows_ = std::move(rows);
    barColor_ = barColor;
    updateGeometry();
    update();
}

QSize CompareChart::sizeHint() const {
    return {420, std::max(60, static_cast<int>(rows_.size()) * kCompareRowHeight)};
}

QSize CompareChart::minimumSizeHint() const {
    return {280, std::max(60, static_cast<int>(rows_.size()) * kCompareRowHeight)};
}

void CompareChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (rows_.empty()) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin gastos en estos dos meses"));
        return;
    }

    double maxValue = 0.0;
    for (const CompareRow& row : rows_) {
        maxValue = std::max({maxValue, row.current, row.previous});
    }
    if (maxValue <= 0.0) {
        maxValue = 1.0;
    }

    // La franja de la derecha se mide con el texto mas largo, igual que en
    // RankChart: la barra nunca pasa por debajo de su propio numero.
    const QFont font = theme::bodyFont(9);
    const QFont numbers = theme::numericFont(9);
    const QFontMetrics metrics(numbers);
    int gutter = 0;
    for (const CompareRow& row : rows_) {
        gutter = std::max(gutter, metrics.horizontalAdvance(row.currentText + QStringLiteral("  ") +
                                                            row.changeText));
    }
    gutter += 16;
    const int barLeft = kCompareLabelWidth;
    const int barArea = std::max(40, width() - barLeft - gutter);

    int y = 0;
    for (const CompareRow& row : rows_) {
        const int mid = y + kCompareRowHeight / 2;

        painter.setFont(font);
        painter.setPen(theme::color(theme::kText));
        const QString label = QFontMetrics(font).elidedText(row.label, Qt::ElideRight,
                                                            kCompareLabelWidth - 10);
        painter.drawText(QRect(0, y, kCompareLabelWidth - 10, kCompareRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, label);

        const int barWidth = static_cast<int>(barArea * (row.current / maxValue));
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::color(theme::kSurfaceRaised));
        painter.drawRoundedRect(QRectF(barLeft, mid - 7, barArea, 14), 3, 3);
        painter.setBrush(theme::color(barColor_));
        if (barWidth > 0) {
            painter.drawRoundedRect(QRectF(barLeft, mid - 7, barWidth, 14), 3, 3);
        }

        // El mes anterior: una raya, no una segunda barra. Dos barras por fila
        // obligan a leer la leyenda; una raya se entiende como "antes estaba aca".
        if (row.previous > 0.0) {
            const int x = barLeft + static_cast<int>(barArea * (row.previous / maxValue));
            painter.setPen(QPen(theme::color(theme::kText), 2));
            painter.drawLine(x, mid - 10, x, mid + 10);
        }

        painter.setFont(numbers);
        painter.setPen(theme::color(theme::kText));
        const QRect textRect(barLeft + barArea + 12, y, gutter, kCompareRowHeight);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, row.currentText);
        painter.setPen(theme::color(row.rose ? theme::kNegative : theme::kPositive));
        const int offset = metrics.horizontalAdvance(row.currentText + QStringLiteral("  "));
        painter.drawText(textRect.adjusted(offset, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                         row.changeText);

        y += kCompareRowHeight;
    }
}

// --- TypeChart ------------------------------------------------------------

namespace {
constexpr int kTypeRowHeight = 44;
constexpr int kTypeLabelWidth = 110;
constexpr int kTypeTop = 22;
} // namespace

TypeChart::TypeChart(QWidget* parent) : QWidget(parent) {}

void TypeChart::setData(std::vector<TypeRow> rows, double targetPercent) {
    rows_ = std::move(rows);
    target_ = targetPercent;
    updateGeometry();
    update();
}

QSize TypeChart::sizeHint() const {
    return {760, kTypeTop + std::max(1, static_cast<int>(rows_.size())) * kTypeRowHeight};
}

QSize TypeChart::minimumSizeHint() const {
    return {480, kTypeTop + std::max(1, static_cast<int>(rows_.size())) * kTypeRowHeight};
}

void TypeChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (rows_.empty()) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("todavía no hay reparaciones entregadas en el periodo"));
        return;
    }

    // La escala va de 0 a lo que haga falta para que entren el objetivo con
    // aire y el mejor margen. Un margen negativo se pinta como barra cero y el
    // numero lo dice.
    double top = target_ * 1.5;
    for (const TypeRow& row : rows_) {
        top = std::max(top, row.marginPercent * 1.1);
    }
    const int barLeft = kTypeLabelWidth;
    const int textWidth = std::max(260, width() / 3);
    const int barWidth = std::max(80, width() - barLeft - textWidth - 16);
    auto x = [&](double percent) {
        const double clamped = std::clamp(percent, 0.0, top);
        return barLeft + static_cast<int>(barWidth * clamped / top);
    };

    const int targetX = x(target_);
    painter.setPen(theme::color(theme::kTextMuted));
    painter.setFont(theme::bodyFont(8));
    painter.drawText(QRect(targetX - 60, 0, 120, kTypeTop - 4), Qt::AlignCenter,
                     QStringLiteral("objetivo %1%").arg(target_, 0, 'f', 0));

    int y = kTypeTop;
    for (const TypeRow& row : rows_) {
        const int mid = y + kTypeRowHeight / 2;
        painter.setFont(theme::bodyFont(10, QFont::DemiBold));
        painter.setPen(theme::color(theme::kText));
        painter.drawText(QRect(0, y, kTypeLabelWidth - 10, kTypeRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, row.label);

        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::color(theme::kSurfaceRaised));
        painter.drawRoundedRect(QRectF(barLeft, mid - 9, barWidth, 18), 4, 4);
        if (row.hasMargin && row.marginPercent > 0) {
            painter.setBrush(theme::color(row.color));
            painter.drawRoundedRect(QRectF(barLeft, mid - 9, x(row.marginPercent) - barLeft, 18), 4, 4);
        }

        const QRect textRect(barLeft + barWidth + 16, y, textWidth, kTypeRowHeight);
        painter.setFont(theme::numericFont(9));
        painter.setPen(theme::color(theme::kText));
        painter.drawText(textRect.adjusted(0, 3, 0, -kTypeRowHeight / 2), Qt::AlignLeft | Qt::AlignVCenter,
                         row.detail);
        painter.setFont(theme::bodyFont(9, QFont::DemiBold));
        painter.setPen(theme::color(row.color));
        painter.drawText(textRect.adjusted(0, kTypeRowHeight / 2 - 3, 0, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, row.action);
        y += kTypeRowHeight;
    }

    // La raya del objetivo va encima de las barras: es contra lo que se mide.
    painter.setPen(QPen(theme::color(theme::kText), 2, Qt::DashLine));
    painter.drawLine(targetX, kTypeTop, targetX, y);
}

// --- SalaryScale ------------------------------------------------------------

SalaryScale::SalaryScale(QWidget* parent) : QWidget(parent) {}

void SalaryScale::setValues(double salary, double personal, double paid, const QString& salaryText,
                            const QString& personalText, const QString& paidText) {
    salary_ = salary;
    personal_ = personal;
    paid_ = paid;
    salaryText_ = salaryText;
    personalText_ = personalText;
    paidText_ = paidText;
    update();
}

QSize SalaryScale::sizeHint() const {
    return {760, 150};
}

void SalaryScale::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const double top = std::max({salary_, personal_, paid_, 1.0}) * 1.2;
    const int left = 12;
    const int right = width() - 12;
    const int span = std::max(40, right - left);
    auto x = [&](double value) { return left + static_cast<int>(span * std::clamp(value, 0.0, top) / top); };
    const int barY = 40;

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::color(theme::kSurfaceRaised));
    painter.drawRoundedRect(QRectF(left, barY, span, 22), 5, 5);
    const bool enough = personal_ < 0 || salary_ >= personal_;
    painter.setBrush(theme::color(enough ? theme::kPositive : theme::kNegative));
    if (salary_ > 0) {
        painter.drawRoundedRect(QRectF(left, barY, x(salary_) - left, 22), 5, 5);
    }
    painter.setFont(theme::bodyFont(9, QFont::DemiBold));
    painter.setPen(theme::color(theme::kText));
    painter.drawText(QRect(left, barY - 26, span, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Sueldo sostenible: ") + salaryText_);

    // Gasto personal: triangulo debajo de la barra.
    painter.setFont(theme::bodyFont(9));
    if (personal_ >= 0) {
        const int px = x(personal_);
        QPainterPath triangle;
        triangle.moveTo(px, barY + 26);
        triangle.lineTo(px - 7, barY + 38);
        triangle.lineTo(px + 7, barY + 38);
        triangle.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::color(theme::kAviso));
        painter.drawPath(triangle);
        painter.setPen(QPen(theme::color(theme::kAviso), 2));
        painter.drawLine(px, barY - 4, px, barY + 26);
        painter.setPen(theme::color(theme::kAviso));
        const QString text = QStringLiteral("gasto personal promedio: ") + personalText_;
        const int w = QFontMetrics(painter.font()).horizontalAdvance(text) + 8;
        const int tx = std::clamp(px - w / 2, left, right - w);
        painter.drawText(QRect(tx, barY + 40, w, 18), Qt::AlignCenter, text);
    }
    // Lo que te pagaste: rombo.
    if (paid_ >= 0) {
        const int dx = x(paid_);
        QPainterPath diamond;
        diamond.moveTo(dx, barY + 60);
        diamond.lineTo(dx + 7, barY + 67);
        diamond.lineTo(dx, barY + 74);
        diamond.lineTo(dx - 7, barY + 67);
        diamond.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::color(theme::kSerie));
        painter.drawPath(diamond);
        painter.setPen(theme::color(theme::kSerie));
        const QString text = QStringLiteral("te pagaste por mes: ") + paidText_;
        const int w = QFontMetrics(painter.font()).horizontalAdvance(text) + 8;
        const int tx = std::clamp(dx - w / 2, left, right - w);
        painter.drawText(QRect(tx, barY + 76, w, 18), Qt::AlignCenter, text);
    }
}

// --- SplitBar ---------------------------------------------------------------

SplitBar::SplitBar(QWidget* parent) : QWidget(parent) {}

void SplitBar::setSegments(std::vector<SplitSegment> segments) {
    segments_ = std::move(segments);
    update();
}

QSize SplitBar::sizeHint() const {
    return {760, 64};
}

void SplitBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    double total = 0;
    for (const SplitSegment& s : segments_) total += std::max(0.0, s.value);
    if (total <= 0) {
        painter.setPen(theme::color(theme::kTextFaint));
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin utilidad para repartir"));
        return;
    }
    const int left = 12;
    const int span = std::max(40, width() - 24);
    double x = left;
    for (const SplitSegment& s : segments_) {
        const double w = span * std::max(0.0, s.value) / total;
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::color(s.color));
        painter.drawRect(QRectF(x, 6, std::max(0.0, w - 2), 18));
        painter.setPen(theme::color(theme::kText));
        painter.setFont(theme::bodyFont(8, QFont::DemiBold));
        painter.drawText(QRectF(x, 28, std::max(60.0, w), 14), Qt::AlignLeft | Qt::AlignVCenter, s.label);
        painter.setFont(theme::numericFont(8));
        painter.setPen(theme::color(theme::kTextMuted));
        painter.drawText(QRectF(x, 43, std::max(60.0, w), 14), Qt::AlignLeft | Qt::AlignVCenter, s.amount);
        x += w;
    }
}

} // namespace dake::ui
