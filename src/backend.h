#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "engine.h"

// One sampled graph, in graph coordinates. A point with a non-finite y marks a
// break in the curve so asymptotes are not joined up across the screen.
struct Curve {
    QString name;
    QVector<QPointF> points;
    int style = 0;  // 0 line, 1 thick, 2 dotted
};

struct PlotPoints {
    int type = 0;  // 0 scatter, 1 xy line, 2 histogram, 3 box plot, 4 normal probability
    QVector<QPointF> points;
    QVector<QPointF> bars;  // histogram bins as (left, height) pairs
    double binWidth = 0;
    double minimum = 0;
    double lowerQuartile = 0;
    double median = 0;
    double upperQuartile = 0;
    double maximum = 0;
    int mark = 0;
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString lastAnswer READ lastAnswer NOTIFY historyChanged)

    Q_PROPERTY(int angleMode READ angleMode WRITE setAngleMode NOTIFY settingsChanged)
    Q_PROPERTY(int numberFormat READ numberFormat WRITE setNumberFormat NOTIFY settingsChanged)
    Q_PROPERTY(int fixDigits READ fixDigits WRITE setFixDigits NOTIFY settingsChanged)
    Q_PROPERTY(int complexMode READ complexMode WRITE setComplexMode NOTIFY settingsChanged)
    Q_PROPERTY(int graphMode READ graphMode WRITE setGraphMode NOTIFY settingsChanged)
    Q_PROPERTY(bool connectedPlot READ connectedPlot WRITE setConnectedPlot NOTIFY settingsChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY settingsChanged)
    Q_PROPERTY(bool showAxes READ showAxes WRITE setShowAxes NOTIFY settingsChanged)

    Q_PROPERTY(QVariantList functions READ functions NOTIFY functionsChanged)
    Q_PROPERTY(QVariantMap window READ window NOTIFY windowChanged)
    Q_PROPERTY(QVariantList lists READ lists NOTIFY listsChanged)
    Q_PROPERTY(QVariantList plots READ plots NOTIFY plotsChanged)
    Q_PROPERTY(QVariantList matrixNames READ matrixNames CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

    Q_PROPERTY(bool tracing READ tracing NOTIFY traceChanged)
    Q_PROPERTY(int traceFunction READ traceFunction NOTIFY traceChanged)
    Q_PROPERTY(double traceX READ traceX NOTIFY traceChanged)
    Q_PROPERTY(double traceY READ traceY NOTIFY traceChanged)
    Q_PROPERTY(double traceParameter READ traceParameter NOTIFY traceChanged)
    Q_PROPERTY(QString traceLabel READ traceLabel NOTIFY traceChanged)
    Q_PROPERTY(QVariantMap shading READ shading NOTIFY traceChanged)

    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    // --- Home screen -------------------------------------------------------
    QVariantList history() const { return m_history; }
    QString lastAnswer() const { return m_lastAnswer; }
    // Evaluates one line, appends it to the history and updates Ans.
    Q_INVOKABLE QVariantMap submit(const QString &line);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE QString evaluateToText(const QString &line);
    Q_INVOKABLE void copyResult() const;
    Q_INVOKABLE QString clipboardText() const;

    // --- Modes -------------------------------------------------------------
    int angleMode() const { return int(m_context.settings.angle); }
    void setAngleMode(int mode);
    int numberFormat() const { return int(m_context.settings.format); }
    void setNumberFormat(int format);
    int fixDigits() const { return m_context.settings.fixDigits; }
    void setFixDigits(int digits);
    int complexMode() const { return int(m_context.settings.complexMode); }
    void setComplexMode(int mode);
    int graphMode() const { return m_graphMode; }
    void setGraphMode(int mode);
    bool connectedPlot() const { return m_connectedPlot; }
    void setConnectedPlot(bool connected);
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);
    bool showAxes() const { return m_showAxes; }
    void setShowAxes(bool show);

    // --- Y= editor ---------------------------------------------------------
    QVariantList functions() const;
    Q_INVOKABLE void setFunctionBody(int index, const QString &body);
    Q_INVOKABLE void setFunctionEnabled(int index, bool enabled);
    Q_INVOKABLE void setFunctionStyle(int index, int style);
    Q_INVOKABLE QString functionName(int index) const;
    Q_INVOKABLE void clearFunctions();

    // --- Window and zoom ---------------------------------------------------
    QVariantMap window() const;
    Q_INVOKABLE bool setWindowValue(const QString &key, const QString &text);
    Q_INVOKABLE void zoomStandard();
    Q_INVOKABLE void zoomTrig();
    Q_INVOKABLE void zoomDecimal();
    Q_INVOKABLE void zoomSquare(double aspect);
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomFit();
    Q_INVOKABLE void zoomInteger();
    Q_INVOKABLE void zoomStatistics();
    Q_INVOKABLE void zoomBox(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void zoomPrevious();
    Q_INVOKABLE void panBy(double dx, double dy);

    // --- Table -------------------------------------------------------------
    Q_INVOKABLE QVariantList tableRows(int count) const;
    Q_INVOKABLE void setTableStart(double start);
    Q_INVOKABLE void setTableStep(double step);
    Q_INVOKABLE void scrollTable(int rows);

    // --- Trace and CALC ----------------------------------------------------
    bool tracing() const { return m_tracing; }
    int traceFunction() const { return m_traceFunction; }
    double traceX() const { return m_traceX; }
    double traceY() const { return m_traceY; }
    double traceParameter() const { return m_traceParameter; }
    QString traceLabel() const { return m_traceLabel; }
    QVariantMap shading() const { return m_shading; }
    Q_INVOKABLE void startTrace();
    Q_INVOKABLE void stopTrace();
    Q_INVOKABLE void traceStep(int direction, int pixels);
    Q_INVOKABLE void traceSelect(int delta);
    Q_INVOKABLE void traceTo(double x);
    Q_INVOKABLE QVariantMap calcValue(int function, double x);
    Q_INVOKABLE QVariantMap calcZero(int function, double left, double right);
    Q_INVOKABLE QVariantMap calcExtremum(int function, double left, double right, bool maximum);
    Q_INVOKABLE QVariantMap calcIntersect(int first, int second, double left, double right);
    Q_INVOKABLE QVariantMap calcDerivative(int function, double x);
    Q_INVOKABLE QVariantMap calcIntegral(int function, double left, double right);

    // --- Statistics --------------------------------------------------------
    QVariantList lists() const;
    Q_INVOKABLE QVariantList listColumn(const QString &name) const;
    Q_INVOKABLE void setListCell(const QString &name, int row, const QString &text);
    Q_INVOKABLE void clearList(const QString &name);
    Q_INVOKABLE QVariantList oneVariableStats(const QString &listName, const QString &frequency);
    Q_INVOKABLE QVariantList twoVariableStats(const QString &xList, const QString &yList);
    Q_INVOKABLE QVariantList regression(int model, const QString &xList, const QString &yList,
                                        int storeToFunction);
    QVariantList plots() const;
    Q_INVOKABLE void setPlot(int index, bool enabled, int type, const QString &xList,
                             const QString &yList, int mark);

    // --- Matrices ----------------------------------------------------------
    QVariantList matrixNames() const;
    int revision() const { return m_revision; }
    Q_INVOKABLE QVariantMap matrixData(const QString &name) const;
    Q_INVOKABLE void resizeMatrix(const QString &name, int rows, int columns);
    Q_INVOKABLE bool setMatrixCell(const QString &name, int row, int column, const QString &text);

    // --- Drawing data for the graph view -----------------------------------
    QVector<Curve> sampleCurves(int pixelWidth);
    QVector<PlotPoints> samplePlots();

    // --- Desktop integration (unchanged from the original calculator) ------
    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);

signals:
    void historyChanged();
    void settingsChanged();
    void functionsChanged();
    void windowChanged();
    void listsChanged();
    void plotsChanged();
    void traceChanged();
    void matricesChanged();
    void revisionChanged();
    void darkModeChanged();
    void textScaleChanged();
    void themeColorsChanged();

private:
    struct GraphFunction {
        QString name;
        QString body;
        bool enabled = true;
        int style = 0;
    };
    struct StatPlot {
        bool enabled = false;
        int type = 0;
        QString xList = QStringLiteral("L1");
        QString yList = QStringLiteral("L2");
        int mark = 0;
    };
    struct GraphWindow {
        double xMin = -10, xMax = 10, xScale = 1;
        double yMin = -10, yMax = 10, yScale = 1;
        int xResolution = 1;
        double tMin = 0, tMax = 6.283185307179586, tStep = 0.1308996938995747;
        double thetaMin = 0, thetaMax = 6.283185307179586, thetaStep = 0.1308996938995747;
        double nMin = 1, nMax = 10;
    };

    void rememberWindow();
    double evaluateFunction(int index, double parameter, bool *ok);
    std::function<double(double)> functionSampler(int index);
    int firstEnabledFunction() const;
    QVariantMap resultMap(bool ok, double x, double y, const QString &label) const;
    QString formatValue(double value) const;
    std::vector<double> listValues(const QString &name) const;
    void loadOmarchyTheme();
    void watchOmarchyTheme();

    int m_revision = 0;
    calc::Context m_context;
    QVariantList m_history;
    QString m_lastAnswer = QStringLiteral("0");

    QVector<GraphFunction> m_functions;
    QVector<StatPlot> m_plots;
    GraphWindow m_window;
    GraphWindow m_previousWindow;
    int m_graphMode = 0;  // 0 function, 1 parametric, 2 polar, 3 sequence
    bool m_connectedPlot = true;
    bool m_showGrid = false;
    bool m_showAxes = true;

    double m_tableStart = 0;
    double m_tableStep = 1;

    bool m_tracing = false;
    int m_traceFunction = 0;
    double m_traceX = 0;
    double m_traceY = 0;
    double m_traceParameter = 0;
    QString m_traceLabel;
    QVariantMap m_shading;

    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QFileSystemWatcher m_themeWatcher;
};
