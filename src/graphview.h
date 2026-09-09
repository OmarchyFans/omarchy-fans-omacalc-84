#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

#include "backend.h"

// Draws the graph screen: grid, axes, the enabled equations, stat plots, the
// trace cursor and the shaded region left behind by ∫f(x)dx.
class GraphView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Backend *backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(QColor pageColor MEMBER m_pageColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor inkColor MEMBER m_inkColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentColor MEMBER m_accentColor NOTIFY appearanceChanged)
    Q_PROPERTY(qreal uiScale MEMBER m_uiScale NOTIFY appearanceChanged)

public:
    explicit GraphView(QQuickItem *parent = nullptr);

    Backend *backend() const { return m_backend; }
    void setBackend(Backend *backend);

    void paint(QPainter *painter) override;

    // Screen and graph coordinates, so QML can turn a tap into an x value.
    Q_INVOKABLE double toGraphX(double pixel) const;
    Q_INVOKABLE double toGraphY(double pixel) const;
    Q_INVOKABLE double toPixelX(double x) const;
    Q_INVOKABLE double toPixelY(double y) const;
    Q_INVOKABLE QColor curveColor(int index) const;

signals:
    void backendChanged();
    void appearanceChanged();

private:
    void invalidate();

    QPointer<Backend> m_backend;
    QColor m_pageColor = QColor(QStringLiteral("#101010"));
    QColor m_inkColor = QColor(QStringLiteral("#eeeeee"));
    QColor m_accentColor = QColor(QStringLiteral("#5584aa"));
    qreal m_uiScale = 1.0;

    QVector<Curve> m_curves;
    QVector<PlotPoints> m_plots;
    bool m_dirty = true;
};
