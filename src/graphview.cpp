#include "graphview.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {
// Ten curve colours, spaced around the wheel from the theme's accent so the
// equations stay apart from one another and still sit in the theme.
QColor colorForIndex(const QColor &accent, int index) {
    const int hue = (accent.hue() < 0 ? 210 : accent.hue()) + index * 36;
    QColor color = QColor::fromHsv(hue % 360, std::max(120, accent.saturation()),
                                   std::max(180, accent.value()));
    return color;
}

bool finitePoint(const QPointF &point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}
}  // namespace

GraphView::GraphView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setRenderTarget(QQuickPaintedItem::Image);
    setAntialiasing(true);
    connect(this, &QQuickItem::widthChanged, this, &GraphView::invalidate);
    connect(this, &QQuickItem::heightChanged, this, &GraphView::invalidate);
    connect(this, &GraphView::appearanceChanged, this, [this]() { update(); });
}

void GraphView::setBackend(Backend *backend) {
    if (m_backend == backend)
        return;
    if (m_backend)
        m_backend->disconnect(this);
    m_backend = backend;
    if (m_backend) {
        connect(m_backend, &Backend::windowChanged, this, &GraphView::invalidate);
        connect(m_backend, &Backend::functionsChanged, this, &GraphView::invalidate);
        connect(m_backend, &Backend::settingsChanged, this, &GraphView::invalidate);
        connect(m_backend, &Backend::listsChanged, this, &GraphView::invalidate);
        connect(m_backend, &Backend::plotsChanged, this, &GraphView::invalidate);
        connect(m_backend, &Backend::traceChanged, this, [this]() { update(); });
    }
    invalidate();
    emit backendChanged();
}

void GraphView::invalidate() {
    m_dirty = true;
    update();
}

double GraphView::toPixelX(double x) const {
    if (!m_backend)
        return 0;
    const QVariantMap window = m_backend->window();
    const double low = window.value(QStringLiteral("xMin")).toDouble();
    const double high = window.value(QStringLiteral("xMax")).toDouble();
    if (high == low)
        return 0;
    return (x - low) / (high - low) * width();
}

double GraphView::toPixelY(double y) const {
    if (!m_backend)
        return 0;
    const QVariantMap window = m_backend->window();
    const double low = window.value(QStringLiteral("yMin")).toDouble();
    const double high = window.value(QStringLiteral("yMax")).toDouble();
    if (high == low)
        return 0;
    return height() - (y - low) / (high - low) * height();
}

double GraphView::toGraphX(double pixel) const {
    if (!m_backend || width() <= 0)
        return 0;
    const QVariantMap window = m_backend->window();
    const double low = window.value(QStringLiteral("xMin")).toDouble();
    const double high = window.value(QStringLiteral("xMax")).toDouble();
    return low + pixel / width() * (high - low);
}

double GraphView::toGraphY(double pixel) const {
    if (!m_backend || height() <= 0)
        return 0;
    const QVariantMap window = m_backend->window();
    const double low = window.value(QStringLiteral("yMin")).toDouble();
    const double high = window.value(QStringLiteral("yMax")).toDouble();
    return low + (height() - pixel) / height() * (high - low);
}

QColor GraphView::curveColor(int index) const { return colorForIndex(m_accentColor, index); }

void GraphView::paint(QPainter *painter) {
    painter->fillRect(0, 0, int(width()), int(height()), m_pageColor);
    if (!m_backend || width() <= 0 || height() <= 0)
        return;

    if (m_dirty) {
        m_curves = m_backend->sampleCurves(int(width()));
        m_plots = m_backend->samplePlots();
        m_dirty = false;
    }

    const QVariantMap window = m_backend->window();
    const double xMin = window.value(QStringLiteral("xMin")).toDouble();
    const double xMax = window.value(QStringLiteral("xMax")).toDouble();
    const double yMin = window.value(QStringLiteral("yMin")).toDouble();
    const double yMax = window.value(QStringLiteral("yMax")).toDouble();
    const double xScale = window.value(QStringLiteral("xScale")).toDouble();
    const double yScale = window.value(QStringLiteral("yScale")).toDouble();
    const double line = std::max(1.0, m_uiScale);

    QColor faint = m_inkColor;
    faint.setAlphaF(0.18f);
    QColor axisColor = m_inkColor;
    axisColor.setAlphaF(0.65f);

    // Tick marks and, when asked for, the grid behind them.
    auto drawVerticalMarks = [&](bool grid) {
        if (xScale <= 0 || (xMax - xMin) / xScale > 400)
            return;
        const double start = std::ceil(xMin / xScale) * xScale;
        for (double x = start; x <= xMax; x += xScale) {
            const double px = toPixelX(x);
            if (grid) {
                painter->setPen(QPen(faint, line));
                painter->drawLine(QPointF(px, 0), QPointF(px, height()));
            } else {
                painter->setPen(QPen(axisColor, line));
                const double py = toPixelY(0);
                painter->drawLine(QPointF(px, py - 3 * line), QPointF(px, py + 3 * line));
            }
        }
    };
    auto drawHorizontalMarks = [&](bool grid) {
        if (yScale <= 0 || (yMax - yMin) / yScale > 400)
            return;
        const double start = std::ceil(yMin / yScale) * yScale;
        for (double y = start; y <= yMax; y += yScale) {
            const double py = toPixelY(y);
            if (grid) {
                painter->setPen(QPen(faint, line));
                painter->drawLine(QPointF(0, py), QPointF(width(), py));
            } else {
                painter->setPen(QPen(axisColor, line));
                const double px = toPixelX(0);
                painter->drawLine(QPointF(px - 3 * line, py), QPointF(px + 3 * line, py));
            }
        }
    };

    if (m_backend->showGrid()) {
        drawVerticalMarks(true);
        drawHorizontalMarks(true);
    }

    if (m_backend->showAxes()) {
        painter->setPen(QPen(axisColor, line));
        if (yMin <= 0 && yMax >= 0)
            painter->drawLine(QPointF(0, toPixelY(0)), QPointF(width(), toPixelY(0)));
        if (xMin <= 0 && xMax >= 0)
            painter->drawLine(QPointF(toPixelX(0), 0), QPointF(toPixelX(0), height()));
        drawVerticalMarks(false);
        drawHorizontalMarks(false);
    }

    // The area measured by ∫f(x)dx, drawn under the curves.
    const QVariantMap shading = m_backend->shading();
    if (!shading.isEmpty()) {
        const int index = shading.value(QStringLiteral("function")).toInt();
        const double from = shading.value(QStringLiteral("from")).toDouble();
        const double to = shading.value(QStringLiteral("to")).toDouble();
        int position = -1;
        for (int i = 0; i < m_curves.size(); ++i) {
            if (m_curves.at(i).index == index)
                position = i;
        }
        if (position >= 0) {
            QPainterPath path;
            bool started = false;
            const Curve &curve = m_curves.at(position);
            for (const QPointF &point : curve.points) {
                if (point.x() < from || point.x() > to || !finitePoint(point))
                    continue;
                const QPointF pixel(toPixelX(point.x()), toPixelY(point.y()));
                if (!started) {
                    path.moveTo(toPixelX(point.x()), toPixelY(0));
                    started = true;
                }
                path.lineTo(pixel);
            }
            if (started) {
                path.lineTo(toPixelX(to), toPixelY(0));
                path.closeSubpath();
                QColor fill = colorForIndex(m_accentColor, curve.index);
                fill.setAlphaF(0.28f);
                painter->fillPath(path, fill);
            }
        }
    }

    // Curves. A break in the samples ends the current polyline, so vertical
    // asymptotes are not joined across the screen.
    for (int index = 0; index < m_curves.size(); ++index) {
        const Curve &curve = m_curves.at(index);
        QPen pen(colorForIndex(m_accentColor, curve.index));
        pen.setWidthF(curve.style == 1 ? 3.0 * line : 1.8 * line);
        if (curve.style == 2)
            pen.setStyle(Qt::DotLine);
        painter->setPen(pen);

        const bool connected = m_backend->connectedPlot();
        QPolygonF segment;
        const double jumpLimit = (yMax - yMin) * 4.0;
        QPointF previous;
        bool hasPrevious = false;
        for (const QPointF &point : curve.points) {
            if (!finitePoint(point)) {
                if (segment.size() > 1)
                    painter->drawPolyline(segment);
                segment.clear();
                hasPrevious = false;
                continue;
            }
            if (hasPrevious && std::abs(point.y() - previous.y()) > jumpLimit) {
                if (segment.size() > 1)
                    painter->drawPolyline(segment);
                segment.clear();
            }
            segment.append(QPointF(toPixelX(point.x()), toPixelY(point.y())));
            previous = point;
            hasPrevious = true;
            if (!connected) {
                painter->drawPoint(segment.last());
                segment.clear();
            }
        }
        if (segment.size() > 1)
            painter->drawPolyline(segment);
    }

    // Stat plots.
    for (int index = 0; index < m_plots.size(); ++index) {
        const PlotPoints &plot = m_plots.at(index);
        // Stat plots take colours past the ten equations so a plot keeps its
        // colour whatever is switched on in the Y= editor.
        const QColor color = colorForIndex(m_accentColor, 10 + index);
        painter->setPen(QPen(color, 1.6 * line));
        const double markSize = 3.0 * line;

        if (plot.type == 0 || plot.type == 1) {
            QPolygonF polyline;
            for (const QPointF &point : plot.points) {
                const QPointF pixel(toPixelX(point.x()), toPixelY(point.y()));
                polyline.append(pixel);
                if (plot.mark == 0)
                    painter->drawRect(QRectF(pixel.x() - markSize / 2, pixel.y() - markSize / 2,
                                             markSize, markSize));
                else if (plot.mark == 1)
                    painter->drawLine(QPointF(pixel.x() - markSize, pixel.y()),
                                      QPointF(pixel.x() + markSize, pixel.y()));
                else
                    painter->drawPoint(pixel);
            }
            if (plot.type == 1 && polyline.size() > 1)
                painter->drawPolyline(polyline);
        } else if (plot.type == 2) {
            QColor fill = color;
            fill.setAlphaF(0.45f);
            for (const QPointF &bar : plot.bars) {
                const double left = toPixelX(bar.x());
                const double right = toPixelX(bar.x() + plot.binWidth);
                const double top = toPixelY(bar.y());
                const double base = toPixelY(0);
                painter->fillRect(QRectF(left, top, right - left, base - top), fill);
                painter->drawRect(QRectF(left, top, right - left, base - top));
            }
        } else if (plot.type == 3) {
            const double centre = height() * (0.25 + 0.12 * index);
            const double boxHeight = std::max(8.0, height() * 0.08);
            const double left = toPixelX(plot.lowerQuartile);
            const double right = toPixelX(plot.upperQuartile);
            painter->drawRect(QRectF(left, centre - boxHeight / 2, right - left, boxHeight));
            painter->drawLine(QPointF(toPixelX(plot.median), centre - boxHeight / 2),
                              QPointF(toPixelX(plot.median), centre + boxHeight / 2));
            painter->drawLine(QPointF(toPixelX(plot.minimum), centre), QPointF(left, centre));
            painter->drawLine(QPointF(right, centre), QPointF(toPixelX(plot.maximum), centre));
        } else {
            for (const QPointF &point : plot.points) {
                const QPointF pixel(toPixelX(point.x()), toPixelY(point.y()));
                painter->drawRect(QRectF(pixel.x() - markSize / 2, pixel.y() - markSize / 2,
                                         markSize, markSize));
            }
        }
    }

    // Trace cursor.
    if (m_backend->tracing()) {
        const double px = toPixelX(m_backend->traceX());
        const double py = toPixelY(m_backend->traceY());
        if (std::isfinite(px) && std::isfinite(py)) {
            painter->setPen(QPen(m_inkColor, line));
            const double arm = 7.0 * line;
            painter->drawLine(QPointF(px - arm, py), QPointF(px + arm, py));
            painter->drawLine(QPointF(px, py - arm), QPointF(px, py + arm));
        }
    }
}
