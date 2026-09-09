#include "backend.h"

#include "stats.h"

#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QRect>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const auto windowGeometrySetting = QStringLiteral("window/geometry");
constexpr double kPi = 3.14159265358979323846;

// The four graphing modes and the equations each one owns, in the order the
// Y= editor lists them.
const QStringList &functionNames() {
    static const QStringList names = {
        QStringLiteral("Y1"),  QStringLiteral("Y2"),  QStringLiteral("Y3"),
        QStringLiteral("Y4"),  QStringLiteral("Y5"),  QStringLiteral("Y6"),
        QStringLiteral("Y7"),  QStringLiteral("Y8"),  QStringLiteral("Y9"),
        QStringLiteral("Y0"),
        QStringLiteral("X1T"), QStringLiteral("Y1T"), QStringLiteral("X2T"),
        QStringLiteral("Y2T"), QStringLiteral("X3T"), QStringLiteral("Y3T"),
        QStringLiteral("X4T"), QStringLiteral("Y4T"), QStringLiteral("X5T"),
        QStringLiteral("Y5T"), QStringLiteral("X6T"), QStringLiteral("Y6T"),
        QStringLiteral("r1"),  QStringLiteral("r2"),  QStringLiteral("r3"),
        QStringLiteral("r4"),  QStringLiteral("r5"),  QStringLiteral("r6"),
        QStringLiteral("u"),   QStringLiteral("v"),   QStringLiteral("w"),
    };
    return names;
}

constexpr int kFunctionOffset = 0;
constexpr int kFunctionCount = 10;
constexpr int kParametricOffset = 10;
constexpr int kParametricCount = 12;
constexpr int kPolarOffset = 22;
constexpr int kPolarCount = 6;
constexpr int kSequenceOffset = 28;
constexpr int kSequenceCount = 3;

int offsetForMode(int mode) {
    switch (mode) {
    case 1:
        return kParametricOffset;
    case 2:
        return kPolarOffset;
    case 3:
        return kSequenceOffset;
    default:
        return kFunctionOffset;
    }
}

int countForMode(int mode) {
    switch (mode) {
    case 1:
        return kParametricCount;
    case 2:
        return kPolarCount;
    case 3:
        return kSequenceCount;
    default:
        return kFunctionCount;
    }
}

const QStringList &listNames() {
    static const QStringList names = {QStringLiteral("L1"), QStringLiteral("L2"),
                                      QStringLiteral("L3"), QStringLiteral("L4"),
                                      QStringLiteral("L5"), QStringLiteral("L6")};
    return names;
}
}  // namespace

Backend::Backend(QObject *parent) : QObject(parent) {
    for (const QString &name : functionNames()) {
        GraphFunction function;
        function.name = name;
        // Parametric and sequence equations start switched off so an empty
        // editor does not draw anything.
        function.enabled = true;
        m_functions.append(function);
    }
    m_plots.resize(3);
    m_previousWindow = m_window;

    QSettings settings;
    settings.beginGroup(QStringLiteral("state"));
    m_context.settings.angle = calc::AngleMode(settings.value(QStringLiteral("angle"), 0).toInt());
    m_context.settings.format =
        calc::NumberFormat(settings.value(QStringLiteral("format"), 0).toInt());
    m_context.settings.fixDigits = settings.value(QStringLiteral("fix"), -1).toInt();
    m_context.settings.complexMode =
        calc::ComplexMode(settings.value(QStringLiteral("complex"), 0).toInt());
    m_graphMode = settings.value(QStringLiteral("graphMode"), 0).toInt();
    m_connectedPlot = settings.value(QStringLiteral("connected"), true).toBool();
    m_showGrid = settings.value(QStringLiteral("grid"), false).toBool();
    m_showAxes = settings.value(QStringLiteral("axes"), true).toBool();
    m_window.xMin = settings.value(QStringLiteral("xMin"), -10.0).toDouble();
    m_window.xMax = settings.value(QStringLiteral("xMax"), 10.0).toDouble();
    m_window.xScale = settings.value(QStringLiteral("xScale"), 1.0).toDouble();
    m_window.yMin = settings.value(QStringLiteral("yMin"), -10.0).toDouble();
    m_window.yMax = settings.value(QStringLiteral("yMax"), 10.0).toDouble();
    m_window.yScale = settings.value(QStringLiteral("yScale"), 1.0).toDouble();
    const QStringList bodies = settings.value(QStringLiteral("functions")).toStringList();
    for (int i = 0; i < bodies.size() && i < m_functions.size(); ++i) {
        m_functions[i].body = bodies.at(i).trimmed();
        if (!m_functions[i].body.isEmpty())
            m_context.functions.insert(m_functions[i].name, m_functions[i].body);
    }
    settings.endGroup();

    // Anything that changes what a screen shows bumps one counter, so bindings
    // built on invokable methods know to re-read.
    const auto bump = [this]() {
        ++m_revision;
        emit revisionChanged();
    };
    connect(this, &Backend::windowChanged, this, bump);
    connect(this, &Backend::functionsChanged, this, bump);
    connect(this, &Backend::settingsChanged, this, bump);
    connect(this, &Backend::listsChanged, this, bump);
    connect(this, &Backend::plotsChanged, this, bump);
    connect(this, &Backend::matricesChanged, this, bump);

    loadOmarchyTheme();
    watchOmarchyTheme();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
}

QString Backend::formatValue(double value) const {
    return calc::formatReal(value, m_context.settings);
}

// --- Home screen -----------------------------------------------------------

QVariantMap Backend::submit(const QString &line) {
    QVariantMap entry;
    entry.insert(QStringLiteral("expression"), line);
    if (line.trimmed().isEmpty()) {
        entry.insert(QStringLiteral("ok"), false);
        entry.insert(QStringLiteral("answer"), QString());
        return entry;
    }

    try {
        const calc::Value value = calc::evaluate(line, m_context);
        const QString text = calc::formatWithHint(value, m_context.settings, m_context.displayHint);
        m_context.store(QStringLiteral("Ans"), value);
        m_lastAnswer = text;
        entry.insert(QStringLiteral("ok"), true);
        entry.insert(QStringLiteral("answer"), text);
    } catch (const calc::Error &error) {
        entry.insert(QStringLiteral("ok"), false);
        entry.insert(QStringLiteral("answer"), QStringLiteral("ERR:") + error.code);
    }

    m_history.append(entry);
    // The home screen only scrolls back so far; older lines fall off the top.
    while (m_history.size() > 200)
        m_history.removeFirst();
    emit historyChanged();
    emit listsChanged();
    emit matricesChanged();
    return entry;
}

QString Backend::evaluateToText(const QString &line) {
    try {
        const calc::Value value = calc::evaluate(line, m_context);
        return calc::formatWithHint(value, m_context.settings, m_context.displayHint);
    } catch (const calc::Error &error) {
        return QStringLiteral("ERR:") + error.code;
    }
}

void Backend::clearHistory() {
    m_history.clear();
    emit historyChanged();
}

void Backend::copyResult() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(m_lastAnswer);
}

QString Backend::clipboardText() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text().trimmed() : QString();
}

// --- Modes -----------------------------------------------------------------

namespace {
void writeSetting(const QString &key, const QVariant &value) {
    QSettings settings;
    settings.beginGroup(QStringLiteral("state"));
    settings.setValue(key, value);
    settings.endGroup();
}
}  // namespace

void Backend::setAngleMode(int mode) {
    const auto angle = calc::AngleMode(std::clamp(mode, 0, 1));
    if (m_context.settings.angle == angle)
        return;
    m_context.settings.angle = angle;
    writeSetting(QStringLiteral("angle"), mode);
    emit settingsChanged();
    emit windowChanged();
}

void Backend::setNumberFormat(int format) {
    const auto value = calc::NumberFormat(std::clamp(format, 0, 2));
    if (m_context.settings.format == value)
        return;
    m_context.settings.format = value;
    writeSetting(QStringLiteral("format"), format);
    emit settingsChanged();
}

void Backend::setFixDigits(int digits) {
    const int value = digits < 0 ? -1 : std::clamp(digits, 0, 9);
    if (m_context.settings.fixDigits == value)
        return;
    m_context.settings.fixDigits = value;
    writeSetting(QStringLiteral("fix"), value);
    emit settingsChanged();
}

void Backend::setComplexMode(int mode) {
    const auto value = calc::ComplexMode(std::clamp(mode, 0, 2));
    if (m_context.settings.complexMode == value)
        return;
    m_context.settings.complexMode = value;
    writeSetting(QStringLiteral("complex"), mode);
    emit settingsChanged();
}

void Backend::setGraphMode(int mode) {
    const int value = std::clamp(mode, 0, 3);
    if (m_graphMode == value)
        return;
    m_graphMode = value;
    stopTrace();
    writeSetting(QStringLiteral("graphMode"), value);
    emit settingsChanged();
    emit functionsChanged();
    emit windowChanged();
}

void Backend::setConnectedPlot(bool connected) {
    if (m_connectedPlot == connected)
        return;
    m_connectedPlot = connected;
    writeSetting(QStringLiteral("connected"), connected);
    emit settingsChanged();
}

void Backend::setShowGrid(bool show) {
    if (m_showGrid == show)
        return;
    m_showGrid = show;
    writeSetting(QStringLiteral("grid"), show);
    emit settingsChanged();
}

void Backend::setShowAxes(bool show) {
    if (m_showAxes == show)
        return;
    m_showAxes = show;
    writeSetting(QStringLiteral("axes"), show);
    emit settingsChanged();
}

// --- Y= editor -------------------------------------------------------------

QVariantList Backend::functions() const {
    QVariantList rows;
    const int offset = offsetForMode(m_graphMode);
    const int count = countForMode(m_graphMode);
    for (int i = 0; i < count; ++i) {
        const GraphFunction &function = m_functions.at(offset + i);
        QVariantMap row;
        row.insert(QStringLiteral("index"), i);
        row.insert(QStringLiteral("name"), function.name);
        row.insert(QStringLiteral("body"), function.body);
        row.insert(QStringLiteral("enabled"), function.enabled);
        row.insert(QStringLiteral("style"), function.style);
        rows.append(row);
    }
    return rows;
}

QString Backend::functionName(int index) const {
    const int offset = offsetForMode(m_graphMode);
    if (index < 0 || index >= countForMode(m_graphMode))
        return QString();
    return m_functions.at(offset + index).name;
}

void Backend::setFunctionBody(int index, const QString &body) {
    const int offset = offsetForMode(m_graphMode);
    if (index < 0 || index >= countForMode(m_graphMode))
        return;
    GraphFunction &function = m_functions[offset + index];
    function.body = body.trimmed();
    if (function.body.isEmpty())
        m_context.functions.remove(function.name);
    else
        m_context.functions.insert(function.name, function.body);
    m_context.clearCache();

    QStringList bodies;
    for (const GraphFunction &item : m_functions)
        bodies << item.body;
    writeSetting(QStringLiteral("functions"), bodies);
    emit functionsChanged();
}

void Backend::setFunctionEnabled(int index, bool enabled) {
    const int offset = offsetForMode(m_graphMode);
    if (index < 0 || index >= countForMode(m_graphMode))
        return;
    m_functions[offset + index].enabled = enabled;
    // A parametric pair is switched on and off together.
    if (m_graphMode == 1) {
        const int partner = index % 2 == 0 ? index + 1 : index - 1;
        if (partner >= 0 && partner < countForMode(m_graphMode))
            m_functions[offset + partner].enabled = enabled;
    }
    emit functionsChanged();
}

void Backend::setFunctionStyle(int index, int style) {
    const int offset = offsetForMode(m_graphMode);
    if (index < 0 || index >= countForMode(m_graphMode))
        return;
    m_functions[offset + index].style = std::clamp(style, 0, 2);
    emit functionsChanged();
}

void Backend::clearFunctions() {
    const int offset = offsetForMode(m_graphMode);
    for (int i = 0; i < countForMode(m_graphMode); ++i) {
        m_functions[offset + i].body.clear();
        m_context.functions.remove(m_functions[offset + i].name);
    }
    m_context.clearCache();
    emit functionsChanged();
}

// --- Window and zoom -------------------------------------------------------

QVariantMap Backend::window() const {
    QVariantMap map;
    map.insert(QStringLiteral("xMin"), m_window.xMin);
    map.insert(QStringLiteral("xMax"), m_window.xMax);
    map.insert(QStringLiteral("xScale"), m_window.xScale);
    map.insert(QStringLiteral("yMin"), m_window.yMin);
    map.insert(QStringLiteral("yMax"), m_window.yMax);
    map.insert(QStringLiteral("yScale"), m_window.yScale);
    map.insert(QStringLiteral("xResolution"), m_window.xResolution);
    map.insert(QStringLiteral("tMin"), m_window.tMin);
    map.insert(QStringLiteral("tMax"), m_window.tMax);
    map.insert(QStringLiteral("tStep"), m_window.tStep);
    map.insert(QStringLiteral("thetaMin"), m_window.thetaMin);
    map.insert(QStringLiteral("thetaMax"), m_window.thetaMax);
    map.insert(QStringLiteral("thetaStep"), m_window.thetaStep);
    map.insert(QStringLiteral("nMin"), m_window.nMin);
    map.insert(QStringLiteral("nMax"), m_window.nMax);
    map.insert(QStringLiteral("tableStart"), m_tableStart);
    map.insert(QStringLiteral("tableStep"), m_tableStep);
    return map;
}

void Backend::rememberWindow() { m_previousWindow = m_window; }

bool Backend::setWindowValue(const QString &key, const QString &text) {
    double value = 0;
    try {
        value = calc::evaluate(text, m_context).real();
    } catch (const calc::Error &) {
        return false;
    }
    if (!std::isfinite(value))
        return false;

    rememberWindow();
    if (key == QStringLiteral("xMin"))
        m_window.xMin = value;
    else if (key == QStringLiteral("xMax"))
        m_window.xMax = value;
    else if (key == QStringLiteral("xScale"))
        m_window.xScale = value;
    else if (key == QStringLiteral("yMin"))
        m_window.yMin = value;
    else if (key == QStringLiteral("yMax"))
        m_window.yMax = value;
    else if (key == QStringLiteral("yScale"))
        m_window.yScale = value;
    else if (key == QStringLiteral("xResolution"))
        m_window.xResolution = std::clamp(int(std::round(value)), 1, 8);
    else if (key == QStringLiteral("tMin"))
        m_window.tMin = value;
    else if (key == QStringLiteral("tMax"))
        m_window.tMax = value;
    else if (key == QStringLiteral("tStep"))
        m_window.tStep = value;
    else if (key == QStringLiteral("thetaMin"))
        m_window.thetaMin = value;
    else if (key == QStringLiteral("thetaMax"))
        m_window.thetaMax = value;
    else if (key == QStringLiteral("thetaStep"))
        m_window.thetaStep = value;
    else if (key == QStringLiteral("nMin"))
        m_window.nMin = value;
    else if (key == QStringLiteral("nMax"))
        m_window.nMax = value;
    else if (key == QStringLiteral("tableStart"))
        m_tableStart = value;
    else if (key == QStringLiteral("tableStep"))
        m_tableStep = value;
    else
        return false;

    QSettings settings;
    settings.beginGroup(QStringLiteral("state"));
    settings.setValue(QStringLiteral("xMin"), m_window.xMin);
    settings.setValue(QStringLiteral("xMax"), m_window.xMax);
    settings.setValue(QStringLiteral("xScale"), m_window.xScale);
    settings.setValue(QStringLiteral("yMin"), m_window.yMin);
    settings.setValue(QStringLiteral("yMax"), m_window.yMax);
    settings.setValue(QStringLiteral("yScale"), m_window.yScale);
    settings.endGroup();

    emit windowChanged();
    return true;
}

void Backend::zoomStandard() {
    rememberWindow();
    m_window.xMin = -10;
    m_window.xMax = 10;
    m_window.xScale = 1;
    m_window.yMin = -10;
    m_window.yMax = 10;
    m_window.yScale = 1;
    emit windowChanged();
}

void Backend::zoomTrig() {
    rememberWindow();
    const bool degrees = m_context.settings.angle == calc::AngleMode::Degree;
    m_window.xMin = degrees ? -360 : -2 * kPi;
    m_window.xMax = degrees ? 360 : 2 * kPi;
    m_window.xScale = degrees ? 90 : kPi / 2;
    m_window.yMin = -4;
    m_window.yMax = 4;
    m_window.yScale = 1;
    emit windowChanged();
}

void Backend::zoomDecimal() {
    rememberWindow();
    m_window.xMin = -4.7;
    m_window.xMax = 4.7;
    m_window.xScale = 1;
    m_window.yMin = -3.1;
    m_window.yMax = 3.1;
    m_window.yScale = 1;
    emit windowChanged();
}

// Makes one unit the same length on both axes for the current view shape.
void Backend::zoomSquare(double aspect) {
    if (aspect <= 0)
        return;
    rememberWindow();
    const double centreY = 0.5 * (m_window.yMin + m_window.yMax);
    const double halfHeight = 0.5 * (m_window.xMax - m_window.xMin) / aspect;
    m_window.yMin = centreY - halfHeight;
    m_window.yMax = centreY + halfHeight;
    emit windowChanged();
}

namespace {
void scaleAbout(double &low, double &high, double factor) {
    const double centre = 0.5 * (low + high);
    const double half = 0.5 * (high - low) * factor;
    low = centre - half;
    high = centre + half;
}
}  // namespace

void Backend::zoomIn() {
    rememberWindow();
    scaleAbout(m_window.xMin, m_window.xMax, 0.25);
    scaleAbout(m_window.yMin, m_window.yMax, 0.25);
    emit windowChanged();
}

void Backend::zoomOut() {
    rememberWindow();
    scaleAbout(m_window.xMin, m_window.xMax, 4.0);
    scaleAbout(m_window.yMin, m_window.yMax, 4.0);
    emit windowChanged();
}

void Backend::zoomInteger() {
    rememberWindow();
    const double centreX = std::round(0.5 * (m_window.xMin + m_window.xMax));
    const double centreY = std::round(0.5 * (m_window.yMin + m_window.yMax));
    m_window.xMin = centreX - 47;
    m_window.xMax = centreX + 47;
    m_window.yMin = centreY - 31;
    m_window.yMax = centreY + 31;
    m_window.xScale = 10;
    m_window.yScale = 10;
    emit windowChanged();
}

// Fits the vertical range to whatever the enabled equations actually reach
// across the current horizontal range.
void Backend::zoomFit() {
    double low = std::numeric_limits<double>::infinity();
    double high = -std::numeric_limits<double>::infinity();
    const QVector<Curve> curves = sampleCurves(200);
    for (const Curve &curve : curves) {
        for (const QPointF &point : curve.points) {
            if (!std::isfinite(point.y()))
                continue;
            low = std::min(low, point.y());
            high = std::max(high, point.y());
        }
    }
    if (!std::isfinite(low) || !std::isfinite(high) || low > high)
        return;
    rememberWindow();
    if (low == high) {
        low -= 1;
        high += 1;
    }
    const double margin = 0.1 * (high - low);
    m_window.yMin = low - margin;
    m_window.yMax = high + margin;
    emit windowChanged();
}

void Backend::zoomStatistics() {
    double lowX = std::numeric_limits<double>::infinity();
    double highX = -std::numeric_limits<double>::infinity();
    double lowY = std::numeric_limits<double>::infinity();
    double highY = -std::numeric_limits<double>::infinity();
    for (const PlotPoints &plot : samplePlots()) {
        for (const QPointF &point : plot.points) {
            lowX = std::min(lowX, point.x());
            highX = std::max(highX, point.x());
            lowY = std::min(lowY, point.y());
            highY = std::max(highY, point.y());
        }
    }
    if (!std::isfinite(lowX) || !std::isfinite(lowY) || lowX > highX)
        return;
    rememberWindow();
    const double marginX = std::max(1e-6, 0.1 * (highX - lowX));
    const double marginY = std::max(1e-6, 0.1 * (highY - lowY));
    m_window.xMin = lowX - marginX;
    m_window.xMax = highX + marginX;
    m_window.yMin = lowY - marginY;
    m_window.yMax = highY + marginY;
    emit windowChanged();
}

void Backend::zoomBox(double x1, double y1, double x2, double y2) {
    if (x1 == x2 || y1 == y2)
        return;
    rememberWindow();
    m_window.xMin = std::min(x1, x2);
    m_window.xMax = std::max(x1, x2);
    m_window.yMin = std::min(y1, y2);
    m_window.yMax = std::max(y1, y2);
    emit windowChanged();
}

void Backend::zoomPrevious() {
    const GraphWindow saved = m_previousWindow;
    m_previousWindow = m_window;
    m_window = saved;
    emit windowChanged();
}

void Backend::panBy(double dx, double dy) {
    rememberWindow();
    m_window.xMin += dx;
    m_window.xMax += dx;
    m_window.yMin += dy;
    m_window.yMax += dy;
    emit windowChanged();
}

// --- Sampling --------------------------------------------------------------

namespace {
QString parameterForMode(int mode) {
    switch (mode) {
    case 1:
        return QStringLiteral("T");
    case 2:
        return QStringLiteral("θ");
    case 3:
        return QStringLiteral("n");
    default:
        return QStringLiteral("X");
    }
}
}  // namespace

double Backend::evaluateFunction(int index, double parameter, bool *ok) {
    bool localOk = false;
    if (!ok)
        ok = &localOk;
    *ok = false;

    const int offset = offsetForMode(m_graphMode);
    if (index < 0 || index >= countForMode(m_graphMode))
        return 0;
    const GraphFunction &function = m_functions.at(offset + index);
    if (function.body.trimmed().isEmpty())
        return 0;

    try {
        const calc::Value value = m_context.callWith(function.body, parameterForMode(m_graphMode),
                                                     calc::Value(calc::Complex(parameter, 0)));
        if (!value.isNumber())
            return 0;
        const calc::Complex number = value.number();
        if (number.imag() != 0 || !std::isfinite(number.real()))
            return 0;
        *ok = true;
        return number.real();
    } catch (const calc::Error &) {
        return 0;
    }
}

// Points where the equation has no value come back as a break in the line.
std::function<double(double)> Backend::functionSampler(int index) {
    return [this, index](double x) {
        bool ok = false;
        const double y = evaluateFunction(index, x, &ok);
        return ok ? y : std::numeric_limits<double>::quiet_NaN();
    };
}

int Backend::firstEnabledFunction() const {
    const int offset = offsetForMode(m_graphMode);
    for (int i = 0; i < countForMode(m_graphMode); ++i) {
        const GraphFunction &function = m_functions.at(offset + i);
        if (function.enabled && !function.body.trimmed().isEmpty())
            return i;
    }
    return -1;
}

QVector<Curve> Backend::sampleCurves(int pixelWidth) {
    QVector<Curve> curves;
    const int offset = offsetForMode(m_graphMode);
    const int count = countForMode(m_graphMode);
    const int steps = std::clamp(pixelWidth / std::max(1, m_window.xResolution), 30, 2000);

    if (m_graphMode == 1) {
        // Parametric: each pair of equations traces one curve in t.
        for (int pair = 0; pair * 2 + 1 < count; ++pair) {
            const GraphFunction &xEquation = m_functions.at(offset + pair * 2);
            const GraphFunction &yEquation = m_functions.at(offset + pair * 2 + 1);
            if (!xEquation.enabled || xEquation.body.trimmed().isEmpty()
                || yEquation.body.trimmed().isEmpty())
                continue;
            Curve curve;
            curve.name = xEquation.name + QStringLiteral("/") + yEquation.name;
            curve.style = xEquation.style;
            const double step = m_window.tStep != 0 ? std::abs(m_window.tStep) : 0.1;
            for (double t = m_window.tMin; t <= m_window.tMax + 1e-12; t += step) {
                bool okX = false;
                bool okY = false;
                const double x = evaluateFunction(pair * 2, t, &okX);
                const double y = evaluateFunction(pair * 2 + 1, t, &okY);
                curve.points.append(okX && okY
                                        ? QPointF(x, y)
                                        : QPointF(x, std::numeric_limits<double>::quiet_NaN()));
                if (curve.points.size() > 20000)
                    break;
            }
            curves.append(curve);
        }
        return curves;
    }

    if (m_graphMode == 2) {
        for (int i = 0; i < count; ++i) {
            const GraphFunction &function = m_functions.at(offset + i);
            if (!function.enabled || function.body.trimmed().isEmpty())
                continue;
            Curve curve;
            curve.name = function.name;
            curve.style = function.style;
            const double step = m_window.thetaStep != 0 ? std::abs(m_window.thetaStep) : 0.1;
            for (double angle = m_window.thetaMin; angle <= m_window.thetaMax + 1e-12;
                 angle += step) {
                bool ok = false;
                const double radius = evaluateFunction(i, angle, &ok);
                const double radians = calc::toRadians(angle, m_context.settings.angle);
                curve.points.append(ok ? QPointF(radius * std::cos(radians),
                                                 radius * std::sin(radians))
                                       : QPointF(0, std::numeric_limits<double>::quiet_NaN()));
                if (curve.points.size() > 20000)
                    break;
            }
            curves.append(curve);
        }
        return curves;
    }

    if (m_graphMode == 3) {
        for (int i = 0; i < count; ++i) {
            const GraphFunction &function = m_functions.at(offset + i);
            if (!function.enabled || function.body.trimmed().isEmpty())
                continue;
            Curve curve;
            curve.name = function.name;
            curve.style = function.style;
            for (double n = m_window.nMin; n <= m_window.nMax + 1e-12; n += 1.0) {
                bool ok = false;
                const double value = evaluateFunction(i, n, &ok);
                curve.points.append(ok ? QPointF(n, value)
                                       : QPointF(n, std::numeric_limits<double>::quiet_NaN()));
                if (curve.points.size() > 5000)
                    break;
            }
            curves.append(curve);
        }
        return curves;
    }

    const double span = m_window.xMax - m_window.xMin;
    if (span <= 0)
        return curves;
    for (int i = 0; i < count; ++i) {
        const GraphFunction &function = m_functions.at(offset + i);
        if (!function.enabled || function.body.trimmed().isEmpty())
            continue;
        Curve curve;
        curve.name = function.name;
        curve.style = function.style;
        curve.points.reserve(steps + 1);
        for (int step = 0; step <= steps; ++step) {
            const double x = m_window.xMin + span * double(step) / double(steps);
            bool ok = false;
            const double y = evaluateFunction(i, x, &ok);
            curve.points.append(QPointF(x, ok ? y : std::numeric_limits<double>::quiet_NaN()));
        }
        curves.append(curve);
    }
    return curves;
}

std::vector<double> Backend::listValues(const QString &name) const {
    std::vector<double> values;
    if (!m_context.has(name))
        return values;
    const calc::Value value = m_context.value(name);
    if (!value.isList())
        return values;
    for (const calc::Complex &item : value.list()) {
        if (item.imag() == 0)
            values.push_back(item.real());
    }
    return values;
}

QVector<PlotPoints> Backend::samplePlots() {
    QVector<PlotPoints> result;
    for (const StatPlot &plot : m_plots) {
        if (!plot.enabled)
            continue;
        const std::vector<double> xs = listValues(plot.xList);
        if (xs.empty())
            continue;
        PlotPoints points;
        points.type = plot.type;
        points.mark = plot.mark;

        if (plot.type == 0 || plot.type == 1) {
            const std::vector<double> ys = listValues(plot.yList);
            const size_t n = std::min(xs.size(), ys.size());
            for (size_t i = 0; i < n; ++i)
                points.points.append(QPointF(xs[i], ys[i]));
        } else if (plot.type == 2) {
            // Histogram bins follow Xscl, exactly as the calculator does.
            const double width = m_window.xScale > 0 ? m_window.xScale : 1.0;
            const double start = m_window.xMin;
            std::vector<int> counts;
            for (double value : xs) {
                const int bin = int(std::floor((value - start) / width));
                if (bin < 0)
                    continue;
                if (bin >= int(counts.size()))
                    counts.resize(size_t(bin) + 1, 0);
                counts[size_t(bin)] += 1;
            }
            points.binWidth = width;
            for (size_t bin = 0; bin < counts.size(); ++bin) {
                if (counts[bin] == 0)
                    continue;
                points.bars.append(QPointF(start + double(bin) * width, counts[bin]));
            }
            for (double value : xs)
                points.points.append(QPointF(value, 0));
        } else if (plot.type == 3) {
            const stats::OneVariable summary = stats::oneVariable(xs, {});
            points.minimum = summary.minimum;
            points.lowerQuartile = summary.lowerQuartile;
            points.median = summary.median;
            points.upperQuartile = summary.upperQuartile;
            points.maximum = summary.maximum;
            for (double value : xs)
                points.points.append(QPointF(value, 0));
        } else {
            // Normal probability plot: each value against its expected z score.
            std::vector<double> sorted = xs;
            std::sort(sorted.begin(), sorted.end());
            const double n = double(sorted.size());
            for (size_t i = 0; i < sorted.size(); ++i) {
                const double area = (double(i) + 0.5) / n;
                const double z = calc::evaluate(QStringLiteral("invNorm(")
                                                    + QString::number(area, 'g', 12)
                                                    + QStringLiteral(")"),
                                                m_context)
                                     .real();
                points.points.append(QPointF(sorted[i], z));
            }
        }
        result.append(points);
    }
    return result;
}

// --- Table -----------------------------------------------------------------

QVariantList Backend::tableRows(int count) const {
    QVariantList rows;
    Backend *self = const_cast<Backend *>(this);
    const int offset = offsetForMode(m_graphMode);
    const int functionCount = countForMode(m_graphMode);

    QStringList headers;
    QList<int> columns;
    for (int i = 0; i < functionCount; ++i) {
        const GraphFunction &function = m_functions.at(offset + i);
        if (function.body.trimmed().isEmpty() || !function.enabled)
            continue;
        headers << function.name;
        columns << i;
    }

    QVariantMap header;
    header.insert(QStringLiteral("header"), true);
    header.insert(QStringLiteral("parameter"), parameterForMode(m_graphMode));
    header.insert(QStringLiteral("columns"), headers);
    rows.append(header);

    for (int r = 0; r < count; ++r) {
        const double x = m_tableStart + m_tableStep * double(r);
        QVariantMap row;
        row.insert(QStringLiteral("header"), false);
        row.insert(QStringLiteral("parameter"), formatValue(x));
        QStringList cells;
        for (int index : columns) {
            bool ok = false;
            const double y = self->evaluateFunction(index, x, &ok);
            cells << (ok ? formatValue(y) : QStringLiteral("ERROR"));
        }
        row.insert(QStringLiteral("columns"), cells);
        rows.append(row);
    }
    return rows;
}

void Backend::setTableStart(double start) {
    m_tableStart = start;
    emit windowChanged();
}

void Backend::setTableStep(double step) {
    if (step == 0)
        return;
    m_tableStep = step;
    emit windowChanged();
}

void Backend::scrollTable(int rows) {
    m_tableStart += m_tableStep * double(rows);
    emit windowChanged();
}

// --- Trace and CALC --------------------------------------------------------

QVariantMap Backend::resultMap(bool ok, double x, double y, const QString &label) const {
    QVariantMap map;
    map.insert(QStringLiteral("ok"), ok);
    map.insert(QStringLiteral("x"), x);
    map.insert(QStringLiteral("y"), y);
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("xText"), formatValue(x));
    map.insert(QStringLiteral("yText"), formatValue(y));
    return map;
}

void Backend::startTrace() {
    const int index = firstEnabledFunction();
    if (index < 0)
        return;
    m_tracing = true;
    m_traceFunction = index;
    if (m_graphMode == 0)
        m_traceParameter = 0.5 * (m_window.xMin + m_window.xMax);
    else if (m_graphMode == 1)
        m_traceParameter = m_window.tMin;
    else if (m_graphMode == 2)
        m_traceParameter = m_window.thetaMin;
    else
        m_traceParameter = m_window.nMin;
    traceTo(m_traceParameter);
}

void Backend::stopTrace() {
    if (!m_tracing && m_shading.isEmpty())
        return;
    m_tracing = false;
    m_shading.clear();
    emit traceChanged();
}

void Backend::traceTo(double parameter) {
    m_traceParameter = parameter;
    bool ok = false;

    if (m_graphMode == 1) {
        const int pair = m_traceFunction - (m_traceFunction % 2);
        bool okX = false;
        bool okY = false;
        const double x = evaluateFunction(pair, parameter, &okX);
        const double y = evaluateFunction(pair + 1, parameter, &okY);
        ok = okX && okY;
        m_traceX = x;
        m_traceY = y;
        m_traceLabel = QStringLiteral("T=") + formatValue(parameter)
            + QStringLiteral("  X=") + formatValue(x) + QStringLiteral("  Y=") + formatValue(y);
    } else if (m_graphMode == 2) {
        const double radius = evaluateFunction(m_traceFunction, parameter, &ok);
        const double radians = calc::toRadians(parameter, m_context.settings.angle);
        m_traceX = radius * std::cos(radians);
        m_traceY = radius * std::sin(radians);
        m_traceLabel = QStringLiteral("θ=") + formatValue(parameter) + QStringLiteral("  r=")
            + formatValue(radius);
    } else {
        const double y = evaluateFunction(m_traceFunction, parameter, &ok);
        m_traceX = parameter;
        m_traceY = y;
        m_traceLabel = functionName(m_traceFunction) + QStringLiteral("   X=")
            + formatValue(parameter) + QStringLiteral("   Y=")
            + (ok ? formatValue(y) : QStringLiteral("undefined"));
    }
    m_tracing = true;
    emit traceChanged();
}

void Backend::traceStep(int direction, int pixels) {
    if (!m_tracing)
        startTrace();
    if (!m_tracing)
        return;
    const int steps = std::max(10, pixels);
    double step = 0;
    switch (m_graphMode) {
    case 1:
        step = m_window.tStep;
        break;
    case 2:
        step = m_window.thetaStep;
        break;
    case 3:
        step = 1;
        break;
    default:
        step = (m_window.xMax - m_window.xMin) / double(steps);
        break;
    }
    double next = m_traceParameter + double(direction) * step;
    if (m_graphMode == 0)
        next = std::clamp(next, m_window.xMin, m_window.xMax);
    traceTo(next);
}

void Backend::traceSelect(int delta) {
    const int offset = offsetForMode(m_graphMode);
    const int count = countForMode(m_graphMode);
    const int stride = m_graphMode == 1 ? 2 : 1;
    for (int attempt = 1; attempt <= count; ++attempt) {
        int candidate = m_traceFunction + delta * attempt * stride;
        candidate = ((candidate % count) + count) % count;
        const GraphFunction &function = m_functions.at(offset + candidate);
        if (function.enabled && !function.body.trimmed().isEmpty()) {
            m_traceFunction = candidate;
            traceTo(m_traceParameter);
            return;
        }
    }
}

QVariantMap Backend::calcValue(int function, double x) {
    bool ok = false;
    const double y = evaluateFunction(function, x, &ok);
    if (ok) {
        m_traceFunction = function;
        traceTo(x);
    }
    return resultMap(ok, x, y, QStringLiteral("value"));
}

QVariantMap Backend::calcZero(int function, double left, double right) {
    const auto sampler = functionSampler(function);
    double root = 0;
    bool ok = calc::findRoot(sampler, std::min(left, right), std::max(left, right), &root);
    if (!ok) {
        // Sweep the bracket for a sign change the user's rough guess missed.
        const double low = std::min(left, right);
        const double high = std::max(left, right);
        const int slices = 200;
        for (int i = 0; i < slices && !ok; ++i) {
            const double a = low + (high - low) * double(i) / slices;
            const double b = low + (high - low) * double(i + 1) / slices;
            ok = calc::findRoot(sampler, a, b, &root);
        }
    }
    if (ok) {
        m_traceFunction = function;
        traceTo(root);
    }
    return resultMap(ok, root, 0, QStringLiteral("zero"));
}

QVariantMap Backend::calcExtremum(int function, double left, double right, bool maximum) {
    const auto sampler = functionSampler(function);
    const double x = calc::findExtremum(sampler, std::min(left, right), std::max(left, right),
                                        maximum);
    bool ok = false;
    const double y = evaluateFunction(function, x, &ok);
    if (ok) {
        m_traceFunction = function;
        traceTo(x);
    }
    return resultMap(ok, x, y, maximum ? QStringLiteral("maximum") : QStringLiteral("minimum"));
}

QVariantMap Backend::calcIntersect(int first, int second, double left, double right) {
    const auto a = functionSampler(first);
    const auto b = functionSampler(second);
    const calc::RealFunction difference = [a, b](double x) { return a(x) - b(x); };
    const double low = std::min(left, right);
    const double high = std::max(left, right);
    double root = 0;
    bool ok = calc::findRoot(difference, low, high, &root);
    for (int i = 0; i < 200 && !ok; ++i) {
        const double from = low + (high - low) * double(i) / 200.0;
        const double to = low + (high - low) * double(i + 1) / 200.0;
        ok = calc::findRoot(difference, from, to, &root);
    }
    bool valueOk = false;
    const double y = evaluateFunction(first, root, &valueOk);
    if (ok && valueOk) {
        m_traceFunction = first;
        traceTo(root);
    }
    return resultMap(ok && valueOk, root, y, QStringLiteral("intersect"));
}

QVariantMap Backend::calcDerivative(int function, double x) {
    const auto sampler = functionSampler(function);
    const double slope = calc::derivativeAt(sampler, x, 0);
    const bool ok = std::isfinite(slope);
    if (ok) {
        m_traceFunction = function;
        traceTo(x);
    }
    QVariantMap map = resultMap(ok, x, slope, QStringLiteral("dy/dx"));
    map.insert(QStringLiteral("yText"), formatValue(slope));
    return map;
}

QVariantMap Backend::calcIntegral(int function, double left, double right) {
    const auto sampler = functionSampler(function);
    const double area = calc::integrateFunction(sampler, left, right);
    const bool ok = std::isfinite(area);
    if (ok) {
        // The view shades the region that was measured.
        m_shading.insert(QStringLiteral("function"), function);
        m_shading.insert(QStringLiteral("from"), std::min(left, right));
        m_shading.insert(QStringLiteral("to"), std::max(left, right));
        emit traceChanged();
    }
    return resultMap(ok, left, area, QStringLiteral("∫f(x)dx"));
}

// --- Lists and statistics --------------------------------------------------

QVariantList Backend::lists() const {
    QVariantList result;
    for (const QString &name : listNames()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), name);
        entry.insert(QStringLiteral("values"), listColumn(name));
        result.append(entry);
    }
    return result;
}

QVariantList Backend::listColumn(const QString &name) const {
    QVariantList column;
    for (double value : listValues(name))
        column.append(formatValue(value));
    return column;
}

void Backend::setListCell(const QString &name, int row, const QString &text) {
    if (row < 0 || row > 998)
        return;
    std::vector<calc::Complex> items;
    if (m_context.has(name)) {
        const calc::Value existing = m_context.value(name);
        if (existing.isList())
            items = existing.list();
    }

    if (text.trimmed().isEmpty()) {
        // Clearing the last cell shortens the list, the way DEL does.
        if (row < int(items.size()))
            items.erase(items.begin() + row);
    } else {
        double value = 0;
        try {
            value = calc::evaluate(text, m_context).real();
        } catch (const calc::Error &) {
            return;
        }
        while (int(items.size()) <= row)
            items.push_back(calc::Complex(0, 0));
        items[size_t(row)] = calc::Complex(value, 0);
    }

    m_context.store(name, calc::Value::fromList(items));
    emit listsChanged();
}

void Backend::clearList(const QString &name) {
    m_context.store(name, calc::Value::fromList({}));
    emit listsChanged();
}

QVariantList Backend::oneVariableStats(const QString &listName, const QString &frequency) {
    QVariantList rows;
    const std::vector<double> values = listValues(listName);
    if (values.empty())
        return rows;
    std::vector<double> weights;
    if (!frequency.trimmed().isEmpty())
        weights = listValues(frequency);
    if (!weights.empty() && weights.size() != values.size())
        return rows;

    const stats::OneVariable summary = stats::oneVariable(values, weights);
    auto add = [&](const QString &label, double value) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("value"), formatValue(value));
        rows.append(row);
    };
    add(QStringLiteral("x̄"), summary.mean);
    add(QStringLiteral("Σx"), summary.sum);
    add(QStringLiteral("Σx²"), summary.sumSquares);
    add(QStringLiteral("Sx"), summary.sampleDeviation);
    add(QStringLiteral("σx"), summary.populationDeviation);
    add(QStringLiteral("n"), summary.count);
    add(QStringLiteral("minX"), summary.minimum);
    add(QStringLiteral("Q1"), summary.lowerQuartile);
    add(QStringLiteral("Med"), summary.median);
    add(QStringLiteral("Q3"), summary.upperQuartile);
    add(QStringLiteral("maxX"), summary.maximum);
    return rows;
}

QVariantList Backend::twoVariableStats(const QString &xList, const QString &yList) {
    QVariantList rows;
    const std::vector<double> xs = listValues(xList);
    const std::vector<double> ys = listValues(yList);
    if (xs.empty() || ys.size() != xs.size())
        return rows;

    const stats::TwoVariable summary = stats::twoVariable(xs, ys);
    auto add = [&](const QString &label, double value) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("value"), formatValue(value));
        rows.append(row);
    };
    add(QStringLiteral("x̄"), summary.meanX);
    add(QStringLiteral("Σx"), summary.sumX);
    add(QStringLiteral("Σx²"), summary.sumXSquares);
    add(QStringLiteral("Sx"), summary.sampleDeviationX);
    add(QStringLiteral("σx"), summary.populationDeviationX);
    add(QStringLiteral("n"), summary.count);
    add(QStringLiteral("ȳ"), summary.meanY);
    add(QStringLiteral("Σy"), summary.sumY);
    add(QStringLiteral("Σy²"), summary.sumYSquares);
    add(QStringLiteral("Sy"), summary.sampleDeviationY);
    add(QStringLiteral("σy"), summary.populationDeviationY);
    add(QStringLiteral("Σxy"), summary.sumXY);
    add(QStringLiteral("minX"), summary.minimumX);
    add(QStringLiteral("maxX"), summary.maximumX);
    add(QStringLiteral("minY"), summary.minimumY);
    add(QStringLiteral("maxY"), summary.maximumY);
    return rows;
}

QVariantList Backend::regression(int model, const QString &xList, const QString &yList,
                                 int storeToFunction) {
    QVariantList rows;
    const std::vector<double> xs = listValues(xList);
    const std::vector<double> ys = listValues(yList);
    if (xs.empty() || ys.size() != xs.size())
        return rows;

    const stats::Regression result =
        stats::fit(stats::Model(std::clamp(model, 0, 10)), xs, ys);
    if (!result.valid) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), QStringLiteral("ERR"));
        row.insert(QStringLiteral("value"), QStringLiteral("no fit"));
        rows.append(row);
        return rows;
    }

    auto add = [&](const QString &label, const QString &value) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("value"), value);
        rows.append(row);
    };
    add(result.name, result.equation);

    static const QStringList letters = {QStringLiteral("a"), QStringLiteral("b"),
                                        QStringLiteral("c"), QStringLiteral("d"),
                                        QStringLiteral("e")};
    for (size_t i = 0; i < result.coefficients.size() && i < size_t(letters.size()); ++i)
        add(letters.at(int(i)), formatValue(result.coefficients[i]));
    add(QStringLiteral("r²"), formatValue(result.determination));
    if (result.hasCorrelation)
        add(QStringLiteral("r"), formatValue(result.correlation));

    // Building the fitted equation as text lets it go straight into Y= and be
    // graphed over the scatter plot.
    QString equation;
    const std::vector<double> &c = result.coefficients;
    auto term = [&](double value) { return QString::number(value, 'g', 12); };
    switch (stats::Model(model)) {
    case stats::Model::Linear:
    case stats::Model::MedianMedian:
        equation = term(c[0]) + QStringLiteral("X+") + term(c[1]);
        break;
    case stats::Model::LinearAB:
        equation = term(c[0]) + QStringLiteral("+") + term(c[1]) + QStringLiteral("X");
        break;
    case stats::Model::Quadratic:
        equation = term(c[0]) + QStringLiteral("X²+") + term(c[1]) + QStringLiteral("X+")
            + term(c[2]);
        break;
    case stats::Model::Cubic:
        equation = term(c[0]) + QStringLiteral("X³+") + term(c[1]) + QStringLiteral("X²+")
            + term(c[2]) + QStringLiteral("X+") + term(c[3]);
        break;
    case stats::Model::Quartic:
        equation = term(c[0]) + QStringLiteral("X⁴+") + term(c[1]) + QStringLiteral("X³+")
            + term(c[2]) + QStringLiteral("X²+") + term(c[3]) + QStringLiteral("X+")
            + term(c[4]);
        break;
    case stats::Model::Logarithmic:
        equation = term(c[0]) + QStringLiteral("+") + term(c[1]) + QStringLiteral("ln(X)");
        break;
    case stats::Model::Exponential:
        equation = term(c[0]) + QStringLiteral("*") + term(c[1]) + QStringLiteral("^X");
        break;
    case stats::Model::Power:
        equation = term(c[0]) + QStringLiteral("*X^") + term(c[1]);
        break;
    case stats::Model::Logistic:
        equation = term(c[2]) + QStringLiteral("/(1+") + term(c[0])
            + QStringLiteral("*e^(-") + term(c[1]) + QStringLiteral("X))");
        break;
    case stats::Model::Sinusoidal:
        equation = term(c[0]) + QStringLiteral("*sin(") + term(c[1]) + QStringLiteral("X+")
            + term(c[2]) + QStringLiteral(")+") + term(c[3]);
        break;
    }
    add(QStringLiteral("RegEQ"), equation);

    if (storeToFunction >= 0) {
        const int savedMode = m_graphMode;
        m_graphMode = 0;
        setFunctionBody(storeToFunction, equation);
        m_graphMode = savedMode;
    }
    return rows;
}

QVariantList Backend::plots() const {
    QVariantList result;
    for (int i = 0; i < m_plots.size(); ++i) {
        const StatPlot &plot = m_plots.at(i);
        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("enabled"), plot.enabled);
        map.insert(QStringLiteral("type"), plot.type);
        map.insert(QStringLiteral("xList"), plot.xList);
        map.insert(QStringLiteral("yList"), plot.yList);
        map.insert(QStringLiteral("mark"), plot.mark);
        result.append(map);
    }
    return result;
}

void Backend::setPlot(int index, bool enabled, int type, const QString &xList,
                      const QString &yList, int mark) {
    if (index < 0 || index >= m_plots.size())
        return;
    StatPlot &plot = m_plots[index];
    plot.enabled = enabled;
    plot.type = std::clamp(type, 0, 4);
    plot.xList = xList;
    plot.yList = yList;
    plot.mark = std::clamp(mark, 0, 2);
    emit plotsChanged();
}

// --- Matrices --------------------------------------------------------------

QVariantList Backend::matrixNames() const {
    QVariantList names;
    for (char letter = 'A'; letter <= 'J'; ++letter)
        names.append(QStringLiteral("[") + QChar(letter) + QStringLiteral("]"));
    return names;
}

QVariantMap Backend::matrixData(const QString &name) const {
    QVariantMap map;
    int rows = 0;
    int columns = 0;
    QVariantList cells;
    if (m_context.has(name)) {
        const calc::Value value = m_context.value(name);
        if (value.isMatrix()) {
            const calc::Matrix &matrix = value.matrix();
            rows = matrix.rows;
            columns = matrix.cols;
            for (int r = 0; r < rows; ++r) {
                QVariantList row;
                for (int c = 0; c < columns; ++c)
                    row.append(calc::formatNumber(matrix.at(r, c), m_context.settings));
                cells.append(QVariant(row));
            }
        }
    }
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("rows"), rows);
    map.insert(QStringLiteral("columns"), columns);
    map.insert(QStringLiteral("cells"), cells);
    return map;
}

void Backend::resizeMatrix(const QString &name, int rows, int columns) {
    rows = std::clamp(rows, 0, 99);
    columns = std::clamp(columns, 0, 99);
    calc::Matrix resized(rows, columns);
    if (m_context.has(name)) {
        const calc::Value existing = m_context.value(name);
        if (existing.isMatrix()) {
            const calc::Matrix &old = existing.matrix();
            for (int r = 0; r < std::min(rows, old.rows); ++r)
                for (int c = 0; c < std::min(columns, old.cols); ++c)
                    resized.at(r, c) = old.at(r, c);
        }
    }
    m_context.store(name, calc::Value::fromMatrix(resized));
    emit matricesChanged();
}

bool Backend::setMatrixCell(const QString &name, int row, int column, const QString &text) {
    if (!m_context.has(name))
        return false;
    const calc::Value existing = m_context.value(name);
    if (!existing.isMatrix())
        return false;
    calc::Matrix matrix = existing.matrix();
    if (row < 0 || row >= matrix.rows || column < 0 || column >= matrix.cols)
        return false;
    try {
        matrix.at(row, column) = calc::evaluate(text, m_context).number();
    } catch (const calc::Error &) {
        return false;
    }
    m_context.store(name, calc::Value::fromMatrix(matrix));
    emit matricesChanged();
    return true;
}

// --- Desktop integration ---------------------------------------------------

QVariantMap Backend::windowGeometry() const {
    const QSettings settings;
    const QRect geometry = settings.value(windowGeometrySetting).toRect();
    QVariantMap map;
    // Positions can legitimately be negative on monitors left of or above the
    // primary, so validity travels separately instead of being encoded as -1.
    map.insert(QStringLiteral("valid"), geometry.isValid());
    map.insert(QStringLiteral("x"), geometry.x());
    map.insert(QStringLiteral("y"), geometry.y());
    map.insert(QStringLiteral("width"), geometry.width());
    map.insert(QStringLiteral("height"), geometry.height());
    map.insert(QStringLiteral("maximized"),
               settings.value(QStringLiteral("window/maximized"), false).toBool());
    return map;
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    settings.setValue(windowGeometrySetting, QRect(x, y, width, height));
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;

    m_textScale = textScale;
    emit textScaleChanged();
}

void Backend::loadOmarchyTheme() {
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}

// --- Inferential statistics ------------------------------------------------

namespace {
QVariantMap field(const QString &key, const QString &label, const QString &type,
                  const QVariant &fallback, const QStringList &options = {}) {
    QVariantMap map;
    map.insert(QStringLiteral("key"), key);
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("type"), type);
    map.insert(QStringLiteral("default"), fallback);
    if (!options.isEmpty())
        map.insert(QStringLiteral("options"), options);
    return map;
}

QVariantMap procedure(const QString &key, const QString &name, const QVariantList &fields) {
    QVariantMap map;
    map.insert(QStringLiteral("key"), key);
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("fields"), fields);
    return map;
}

const QStringList &tailOptions() {
    static const QStringList options = {QStringLiteral("≠"), QStringLiteral("<"),
                                        QStringLiteral(">")};
    return options;
}

const QStringList &yesNo() {
    static const QStringList options = {QStringLiteral("No"), QStringLiteral("Yes")};
    return options;
}
}  // namespace

QVariantList Backend::inferenceProcedures() const {
    const QVariant one = QStringLiteral("L1");
    const QVariant two = QStringLiteral("L2");
    QVariantList list;

    list << procedure(QStringLiteral("ZTest"), QStringLiteral("Z-Test"),
                      {field(QStringLiteral("mu"), QStringLiteral("μ₀"), QStringLiteral("number"), 0),
                       field(QStringLiteral("sigma"), QStringLiteral("σ"), QStringLiteral("number"), 1),
                       field(QStringLiteral("list"), QStringLiteral("List"), QStringLiteral("list"), one),
                       field(QStringLiteral("tail"), QStringLiteral("μ"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("TTest"), QStringLiteral("T-Test"),
                      {field(QStringLiteral("mu"), QStringLiteral("μ₀"), QStringLiteral("number"), 0),
                       field(QStringLiteral("list"), QStringLiteral("List"), QStringLiteral("list"), one),
                       field(QStringLiteral("tail"), QStringLiteral("μ"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("2SampTTest"), QStringLiteral("2-SampTTest"),
                      {field(QStringLiteral("list1"), QStringLiteral("List1"), QStringLiteral("list"), one),
                       field(QStringLiteral("list2"), QStringLiteral("List2"), QStringLiteral("list"), two),
                       field(QStringLiteral("tail"), QStringLiteral("μ1"), QStringLiteral("choice"), 0, tailOptions()),
                       field(QStringLiteral("pooled"), QStringLiteral("Pooled"), QStringLiteral("choice"), 0, yesNo())});

    list << procedure(QStringLiteral("2SampZTest"), QStringLiteral("2-SampZTest"),
                      {field(QStringLiteral("sigma1"), QStringLiteral("σ1"), QStringLiteral("number"), 1),
                       field(QStringLiteral("sigma2"), QStringLiteral("σ2"), QStringLiteral("number"), 1),
                       field(QStringLiteral("list1"), QStringLiteral("List1"), QStringLiteral("list"), one),
                       field(QStringLiteral("list2"), QStringLiteral("List2"), QStringLiteral("list"), two),
                       field(QStringLiteral("tail"), QStringLiteral("μ1"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("1PropZTest"), QStringLiteral("1-PropZTest"),
                      {field(QStringLiteral("p0"), QStringLiteral("p₀"), QStringLiteral("number"), 0.5),
                       field(QStringLiteral("x"), QStringLiteral("x"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n"), QStringLiteral("n"), QStringLiteral("number"), 0),
                       field(QStringLiteral("tail"), QStringLiteral("prop"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("2PropZTest"), QStringLiteral("2-PropZTest"),
                      {field(QStringLiteral("x1"), QStringLiteral("x1"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n1"), QStringLiteral("n1"), QStringLiteral("number"), 0),
                       field(QStringLiteral("x2"), QStringLiteral("x2"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n2"), QStringLiteral("n2"), QStringLiteral("number"), 0),
                       field(QStringLiteral("tail"), QStringLiteral("p1"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("GOF"), QStringLiteral("χ²GOF-Test"),
                      {field(QStringLiteral("observed"), QStringLiteral("Observed"), QStringLiteral("list"), one),
                       field(QStringLiteral("expected"), QStringLiteral("Expected"), QStringLiteral("list"), two),
                       field(QStringLiteral("df"), QStringLiteral("df"), QStringLiteral("number"), 0)});

    list << procedure(QStringLiteral("ChiSquare"), QStringLiteral("χ²-Test"),
                      {field(QStringLiteral("matrix"), QStringLiteral("Observed"), QStringLiteral("matrix"),
                             QStringLiteral("[A]"))});

    list << procedure(QStringLiteral("LinRegTTest"), QStringLiteral("LinRegTTest"),
                      {field(QStringLiteral("list1"), QStringLiteral("Xlist"), QStringLiteral("list"), one),
                       field(QStringLiteral("list2"), QStringLiteral("Ylist"), QStringLiteral("list"), two),
                       field(QStringLiteral("tail"), QStringLiteral("β"), QStringLiteral("choice"), 0, tailOptions())});

    list << procedure(QStringLiteral("ANOVA"), QStringLiteral("ANOVA"),
                      {field(QStringLiteral("list1"), QStringLiteral("List1"), QStringLiteral("list"), one),
                       field(QStringLiteral("list2"), QStringLiteral("List2"), QStringLiteral("list"), two),
                       field(QStringLiteral("list3"), QStringLiteral("List3"), QStringLiteral("list"),
                             QString())});

    list << procedure(QStringLiteral("ZInterval"), QStringLiteral("ZInterval"),
                      {field(QStringLiteral("sigma"), QStringLiteral("σ"), QStringLiteral("number"), 1),
                       field(QStringLiteral("list"), QStringLiteral("List"), QStringLiteral("list"), one),
                       field(QStringLiteral("level"), QStringLiteral("C-Level"), QStringLiteral("number"), 0.95)});

    list << procedure(QStringLiteral("TInterval"), QStringLiteral("TInterval"),
                      {field(QStringLiteral("list"), QStringLiteral("List"), QStringLiteral("list"), one),
                       field(QStringLiteral("level"), QStringLiteral("C-Level"), QStringLiteral("number"), 0.95)});

    list << procedure(QStringLiteral("2SampTInt"), QStringLiteral("2-SampTInt"),
                      {field(QStringLiteral("list1"), QStringLiteral("List1"), QStringLiteral("list"), one),
                       field(QStringLiteral("list2"), QStringLiteral("List2"), QStringLiteral("list"), two),
                       field(QStringLiteral("level"), QStringLiteral("C-Level"), QStringLiteral("number"), 0.95),
                       field(QStringLiteral("pooled"), QStringLiteral("Pooled"), QStringLiteral("choice"), 0, yesNo())});

    list << procedure(QStringLiteral("1PropZInt"), QStringLiteral("1-PropZInt"),
                      {field(QStringLiteral("x"), QStringLiteral("x"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n"), QStringLiteral("n"), QStringLiteral("number"), 0),
                       field(QStringLiteral("level"), QStringLiteral("C-Level"), QStringLiteral("number"), 0.95)});

    list << procedure(QStringLiteral("2PropZInt"), QStringLiteral("2-PropZInt"),
                      {field(QStringLiteral("x1"), QStringLiteral("x1"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n1"), QStringLiteral("n1"), QStringLiteral("number"), 0),
                       field(QStringLiteral("x2"), QStringLiteral("x2"), QStringLiteral("number"), 0),
                       field(QStringLiteral("n2"), QStringLiteral("n2"), QStringLiteral("number"), 0),
                       field(QStringLiteral("level"), QStringLiteral("C-Level"), QStringLiteral("number"), 0.95)});

    return list;
}

QVariantList Backend::runInference(const QString &key, const QVariantMap &values) {
    auto number = [&](const QString &name, double fallback = 0.0) {
        bool ok = false;
        const double value = values.value(name).toDouble(&ok);
        return ok ? value : fallback;
    };
    auto listOf = [&](const QString &name) {
        return listValues(values.value(name).toString());
    };
    auto tailOf = [&](const QString &name) {
        return stats::Tail(std::clamp(values.value(name).toInt(), 0, 2));
    };
    auto summaryOf = [&](const QString &name) {
        return stats::oneVariable(listOf(name), {});
    };

    stats::Inference result;
    if (key == QStringLiteral("ZTest")) {
        const stats::OneVariable summary = summaryOf(QStringLiteral("list"));
        result = stats::zTest(number(QStringLiteral("mu")), number(QStringLiteral("sigma"), 1),
                              summary.mean, summary.count, tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("TTest")) {
        const stats::OneVariable summary = summaryOf(QStringLiteral("list"));
        result = stats::tTest(number(QStringLiteral("mu")), summary.mean,
                              summary.sampleDeviation, summary.count,
                              tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("2SampTTest")) {
        const stats::OneVariable a = summaryOf(QStringLiteral("list1"));
        const stats::OneVariable b = summaryOf(QStringLiteral("list2"));
        result = stats::twoSampleTTest(a.mean, a.sampleDeviation, a.count, b.mean,
                                       b.sampleDeviation, b.count, tailOf(QStringLiteral("tail")),
                                       values.value(QStringLiteral("pooled")).toInt() == 1);
    } else if (key == QStringLiteral("2SampZTest")) {
        const stats::OneVariable a = summaryOf(QStringLiteral("list1"));
        const stats::OneVariable b = summaryOf(QStringLiteral("list2"));
        result = stats::twoSampleZTest(number(QStringLiteral("sigma1"), 1),
                                       number(QStringLiteral("sigma2"), 1), a.mean, a.count,
                                       b.mean, b.count, tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("1PropZTest")) {
        result = stats::onePropZTest(number(QStringLiteral("p0"), 0.5), number(QStringLiteral("x")),
                                     number(QStringLiteral("n")), tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("2PropZTest")) {
        result = stats::twoPropZTest(number(QStringLiteral("x1")), number(QStringLiteral("n1")),
                                     number(QStringLiteral("x2")), number(QStringLiteral("n2")),
                                     tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("GOF")) {
        result = stats::goodnessOfFit(listOf(QStringLiteral("observed")),
                                      listOf(QStringLiteral("expected")),
                                      number(QStringLiteral("df")));
    } else if (key == QStringLiteral("ChiSquare")) {
        std::vector<std::vector<double>> table;
        const QString name = values.value(QStringLiteral("matrix")).toString();
        if (m_context.has(name)) {
            const calc::Value value = m_context.value(name);
            if (value.isMatrix()) {
                const calc::Matrix &matrix = value.matrix();
                for (int r = 0; r < matrix.rows; ++r) {
                    std::vector<double> row;
                    for (int c = 0; c < matrix.cols; ++c)
                        row.push_back(matrix.at(r, c).real());
                    table.push_back(row);
                }
            }
        }
        result = stats::independenceTest(table);
    } else if (key == QStringLiteral("LinRegTTest")) {
        result = stats::linearRegressionTTest(listOf(QStringLiteral("list1")),
                                              listOf(QStringLiteral("list2")),
                                              tailOf(QStringLiteral("tail")));
    } else if (key == QStringLiteral("ANOVA")) {
        std::vector<std::vector<double>> groups;
        for (const QString &name : {QStringLiteral("list1"), QStringLiteral("list2"),
                                    QStringLiteral("list3")}) {
            const std::vector<double> group = listOf(name);
            if (!group.empty())
                groups.push_back(group);
        }
        result = stats::analysisOfVariance(groups);
    } else if (key == QStringLiteral("ZInterval")) {
        const stats::OneVariable summary = summaryOf(QStringLiteral("list"));
        result = stats::zInterval(number(QStringLiteral("sigma"), 1), summary.mean, summary.count,
                                  number(QStringLiteral("level"), 0.95));
    } else if (key == QStringLiteral("TInterval")) {
        const stats::OneVariable summary = summaryOf(QStringLiteral("list"));
        result = stats::tInterval(summary.mean, summary.sampleDeviation, summary.count,
                                  number(QStringLiteral("level"), 0.95));
    } else if (key == QStringLiteral("2SampTInt")) {
        const stats::OneVariable a = summaryOf(QStringLiteral("list1"));
        const stats::OneVariable b = summaryOf(QStringLiteral("list2"));
        result = stats::twoSampleTInterval(a.mean, a.sampleDeviation, a.count, b.mean,
                                           b.sampleDeviation, b.count,
                                           number(QStringLiteral("level"), 0.95),
                                           values.value(QStringLiteral("pooled")).toInt() == 1);
    } else if (key == QStringLiteral("1PropZInt")) {
        result = stats::onePropZInterval(number(QStringLiteral("x")), number(QStringLiteral("n")),
                                         number(QStringLiteral("level"), 0.95));
    } else if (key == QStringLiteral("2PropZInt")) {
        result = stats::twoPropZInterval(number(QStringLiteral("x1")), number(QStringLiteral("n1")),
                                         number(QStringLiteral("x2")), number(QStringLiteral("n2")),
                                         number(QStringLiteral("level"), 0.95));
    }

    QVariantList rows;
    if (!result.valid) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), QStringLiteral("ERR"));
        row.insert(QStringLiteral("value"), QStringLiteral("check the inputs"));
        rows.append(row);
        return rows;
    }
    for (const auto &pair : result.values) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), pair.first);
        row.insert(QStringLiteral("value"), formatValue(pair.second));
        rows.append(row);
    }
    return rows;
}
