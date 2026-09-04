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
        painter.setPen(theme::kTextFaint);
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
        
        painter.fillRect(QRectF(startX, legendY + (labelHeight - 8) / 2.0, 8, 8), theme::kPositive);
        painter.setPen(theme::kTextMuted);
        painter.drawText(QRectF(startX + 12, legendY, w1, labelHeight), Qt::AlignLeft | Qt::AlignVCenter, firstName_);
        
        startX += 12 + w1 + spacing;
        painter.fillRect(QRectF(startX, legendY + (labelHeight - 8) / 2.0, 8, 8), theme::kNegative);
        painter.drawText(QRectF(startX + 12, legendY, w2, labelHeight), Qt::AlignLeft | Qt::AlignVCenter, secondName_);
    }

    painter.setPen(theme::kBorder);
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
            painter.fillRect(QRectF(cx - barW, y1, barW - 1, h1), theme::kPositive);

            double h2 = std::abs(p.secondary) * scale;
            double y2 = p.secondary >= 0 ? zeroY - h2 : zeroY;
            if (!signed_) y2 = zeroY - h2;
            painter.fillRect(QRectF(cx, y2, barW - 1, h2), theme::kNegative);
        } else {
            double barW = barGroupWidth;
            double h = std::abs(p.primary) * scale;
            double y = p.primary >= 0 ? zeroY - h : zeroY;
            if (!signed_) y = zeroY - h;
            
            QColor c = theme::kAccent;
            if (signed_) {
                c = p.primary >= 0 ? theme::kPositive : theme::kNegative;
            }
            painter.fillRect(QRectF(cx - barW / 2.0, y, barW, h), c);
        }
    }

    painter.setFont(labelFont);
    painter.setPen(theme::kTextFaint);
    
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
        painter.setPen(theme::kTextFaint);
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

    painter.setPen(theme::kBorder);
    painter.drawLine(QPointF(0, zeroY), QPointF(width(), zeroY));

    QColor fillColor = theme::kAccent;
    fillColor.setAlpha(40);
    painter.fillPath(fillPath, fillColor);

    QPen linePen(theme::kAccent, 2);
    painter.setPen(linePen);
    painter.drawPath(linePath);

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::kAccent);
    for (const auto& pt : poly) {
        painter.drawEllipse(pt, 3, 3);
    }

    painter.setFont(labelFont);
    painter.setPen(theme::kTextFaint);
    
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
    update();
}

void RankChart::setLimit(int rows) {
    limit_ = rows;
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
        painter.setPen(theme::kTextFaint);
        painter.setFont(theme::bodyFont(9));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("sin datos todavia"));
        return;
    }

    std::vector<ChartPoint> rows;
    int safeLimit = std::max(1, limit_);
    if (points_.size() > static_cast<size_t>(safeLimit)) {
        for (int i = 0; i < safeLimit - 1; ++i) {
            rows.push_back(points_[i]);
        }
        ChartPoint otros;
        otros.label = QStringLiteral("Otros");
        for (size_t i = safeLimit - 1; i < points_.size(); ++i) {
            otros.primary += points_[i].primary;
        }
        rows.push_back(otros);
    } else {
        rows = points_;
    }

    double max_val = 0.0;
    for (const auto& r : rows) {
        max_val = std::max(max_val, std::abs(r.primary));
    }

    if (max_val == 0.0) {
        painter.setPen(theme::kTextFaint);
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

        QColor barColor = theme::kAccent;
        barColor.setAlphaF(0.60F);   // el sufijo F: setAlphaF toma float, y sin el /W4 avisa
        painter.fillRect(QRectF(0, y, barW, 24), barColor);
        
        painter.setPen(theme::kText);
        painter.drawText(QRectF(6, y, barArea - 12, 24), Qt::AlignLeft | Qt::AlignVCenter, r.label);
        
        if (!r.primaryText.isEmpty()) {
            painter.setPen(theme::kTextMuted);
            painter.drawText(QRectF(6, y, width() - 12, 24), Qt::AlignRight | Qt::AlignVCenter, r.primaryText);
        }
        
        y += 30;
    }
}

} // namespace dake::ui
