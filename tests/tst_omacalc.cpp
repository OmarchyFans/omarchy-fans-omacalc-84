#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>

#include "backend.h"
#include "engine.h"
#include "special.h"
#include "stats.h"

namespace {

// Engine helpers: evaluate a line the way the home screen does and hand back
// either the formatted answer, the raw number, or the error name.
QString answer(const QString &source, calc::Context &context) {
    const calc::Value value = calc::evaluate(source, context);
    return calc::formatWithHint(value, context.settings, context.displayHint);
}

double number(const QString &source, calc::Context &context) {
    return calc::evaluate(source, context).number().real();
}

// Equations persist in the settings, so a test that graphs starts from a
// clean editor in every mode.
void clearAllEquations(Backend &calculator) {
    const int mode = calculator.graphMode();
    for (int each = 0; each < 4; ++each) {
        calculator.setGraphMode(each);
        calculator.clearFunctions();
    }
    calculator.setGraphMode(mode);
}

QString errorFor(const QString &source, calc::Context &context) {
    try {
        calc::evaluate(source, context);
    } catch (const calc::Error &error) {
        return error.code;
    }
    return QString();
}
}

class OmacalcTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }


    // --- Expression engine -------------------------------------------------

    void followsOperatingSystemPrecedence() {
        calc::Context context;
        QCOMPARE(answer("3+4*2", context), QStringLiteral("11"));
        // Squaring happens before the sign is applied.
        QCOMPARE(answer("-2^2", context), QStringLiteral("-4"));
        QCOMPARE(answer("(-2)^2", context), QStringLiteral("4"));
        // Powers chain left to right on this calculator.
        QCOMPARE(answer("2^3^2", context), QStringLiteral("64"));
        QCOMPARE(answer("2^-3", context), QStringLiteral("0.125"));
        QCOMPARE(answer("2+3*4-6/2", context), QStringLiteral("11"));
    }

    void multipliesWithoutAnOperator() {
        calc::Context context;
        QCOMPARE(answer("2(3)", context), QStringLiteral("6"));
        QCOMPARE(answer("(2)(3)", context), QStringLiteral("6"));
        QCOMPARE(answer("3->X:2X", context), QStringLiteral("6"));
        // Implied multiplication ranks with division, so this is (1/2)X.
        QCOMPARE(answer("3->X:1/2X", context), QStringLiteral("1.5"));
        QCOMPARE(answer("2pi", context), QStringLiteral("6.283185307"));
    }

    void closesParenthesesAtTheEndOfALine() {
        calc::Context context;
        QCOMPARE(answer("2*(3+4", context), QStringLiteral("14"));
        QCOMPARE(answer("sqrt(16", context), QStringLiteral("4"));
    }

    void evaluatesRootsAndPowers() {
        calc::Context context;
        QCOMPARE(answer("√(16)", context), QStringLiteral("4"));
        QCOMPARE(answer("√16", context), QStringLiteral("4"));
        // A prefix root without a parenthesis takes just the next operand.
        QCOMPARE(answer("√9+1", context), QStringLiteral("4"));
        QCOMPARE(answer("∛(27)", context), QStringLiteral("3"));
        QCOMPARE(answer("3 ˣ√ 8", context), QStringLiteral("2"));
        QCOMPARE(answer("5²", context), QStringLiteral("25"));
        QCOMPARE(answer("4⁻¹", context), QStringLiteral("0.25"));
    }

    void appliesTheAngleMode() {
        calc::Context context;
        context.settings.angle = calc::AngleMode::Degree;
        QCOMPARE(answer("sin(30)", context), QStringLiteral("0.5"));
        QCOMPARE(answer("cos(60)", context), QStringLiteral("0.5"));
        QCOMPARE(answer("tan⁻¹(1)", context), QStringLiteral("45"));

        context.settings.angle = calc::AngleMode::Radian;
        QCOMPARE(answer("sin(pi/6)", context), QStringLiteral("0.5"));
        // The degree mark overrides the mode for one number.
        QVERIFY(std::abs(number("sin(30°)", context) - 0.5) < 1e-12);
    }

    void evaluatesLogarithmsAndProbability() {
        calc::Context context;
        QCOMPARE(answer("ln(e)", context), QStringLiteral("1"));
        QCOMPARE(answer("log(100)", context), QStringLiteral("2"));
        QCOMPARE(answer("logBASE(8,2)", context), QStringLiteral("3"));
        QCOMPARE(answer("5!", context), QStringLiteral("120"));
        QCOMPARE(answer("5 nCr 2", context), QStringLiteral("10"));
        QCOMPARE(answer("5 nPr 2", context), QStringLiteral("20"));
        QCOMPARE(answer("gcd(12,18)", context), QStringLiteral("6"));
        QCOMPARE(answer("lcm(4,6)", context), QStringLiteral("12"));
    }

    void showsTenSignificantDigits() {
        calc::Context context;
        QCOMPARE(answer("pi", context), QStringLiteral("3.141592654"));
        QCOMPARE(answer("1/3", context), QStringLiteral("0.3333333333"));
        QCOMPARE(answer("0.1+0.2", context), QStringLiteral("0.3"));
        QCOMPARE(answer("2^40", context), QStringLiteral("1.099511628E12"));
        QCOMPARE(answer("2^30", context), QStringLiteral("1073741824"));
        QCOMPARE(answer("1/8000", context), QStringLiteral("1.25E-4"));
    }

    void honoursTheDisplayModes() {
        calc::Context context;
        context.settings.format = calc::NumberFormat::Scientific;
        QCOMPARE(answer("12345", context), QStringLiteral("1.2345E4"));
        context.settings.format = calc::NumberFormat::Engineering;
        QCOMPARE(answer("12345", context), QStringLiteral("12.345E3"));
        context.settings.format = calc::NumberFormat::Normal;
        context.settings.fixDigits = 2;
        QCOMPARE(answer("2/3", context), QStringLiteral("0.67"));
    }

    void convertsToFractionsAndDegrees() {
        calc::Context context;
        QCOMPARE(answer("0.75▶Frac", context), QStringLiteral("3/4"));
        QCOMPARE(answer("(1/3)▶Frac", context), QStringLiteral("1/3"));
        QCOMPARE(answer("1.5▶DMS", context), QStringLiteral("1°30'0.000\""));
    }

    void storesAndRecallsVariables() {
        calc::Context context;
        QCOMPARE(answer("5->A", context), QStringLiteral("5"));
        QCOMPARE(answer("A*3", context), QStringLiteral("15"));
        QCOMPARE(answer("A+1->A:A", context), QStringLiteral("6"));
        // Untouched letters read as zero.
        QCOMPARE(answer("Z", context), QStringLiteral("0"));
    }

    void worksWithLists() {
        calc::Context context;
        QCOMPARE(answer("{1,2,3}+1", context), QStringLiteral("{2, 3, 4}"));
        QCOMPARE(answer("{1,2,3}*{2,2,2}", context), QStringLiteral("{2, 4, 6}"));
        QCOMPARE(answer("sum({1,2,3,4})", context), QStringLiteral("10"));
        QCOMPARE(answer("mean({2,4,6})", context), QStringLiteral("4"));
        QCOMPARE(answer("median({5,1,3})", context), QStringLiteral("3"));
        QCOMPARE(answer("stdDev({2,4,4,4,5,5,7,9})", context), QStringLiteral("2.138089935"));
        QCOMPARE(answer("cumSum({1,2,3})", context), QStringLiteral("{1, 3, 6}"));
        QCOMPARE(answer("ΔList({1,4,9})", context), QStringLiteral("{3, 5}"));
        QCOMPARE(answer("sortA({3,1,2})", context), QStringLiteral("{1, 2, 3}"));
        QCOMPARE(answer("seq(X²,X,1,4)", context), QStringLiteral("{1, 4, 9, 16}"));
        QCOMPARE(answer("{1,2,3}->L1:L1(2)", context), QStringLiteral("2"));
        QCOMPARE(answer("dim({1,2,3})", context), QStringLiteral("3"));
    }

    void worksWithMatrices() {
        calc::Context context;
        QCOMPARE(answer("[[1,2],[3,4]]->[A]:det([A])", context), QStringLiteral("-2"));
        QCOMPARE(answer("[A]ᵀ", context), QStringLiteral("[[1, 3][2, 4]]"));
        QCOMPARE(answer("[A]*[A]", context), QStringLiteral("[[7, 10][15, 22]]"));
        QCOMPARE(answer("[A]⁻¹", context), QStringLiteral("[[-2, 1][1.5, -0.5]]"));
        QCOMPARE(answer("identity(2)", context), QStringLiteral("[[1, 0][0, 1]]"));
        QCOMPARE(answer("rref([[1,2,3],[4,5,6]])", context), QStringLiteral("[[1, 0, -1][0, 1, 2]]"));
        QCOMPARE(answer("[A](2,1)", context), QStringLiteral("3"));
        QCOMPARE(answer("dim([A])", context), QStringLiteral("{2, 2}"));
    }

    void handlesComplexNumbersOnlyWhenAsked() {
        calc::Context context;
        QCOMPARE(errorFor("sqrt(-4)", context), QStringLiteral("NONREAL ANS"));

        context.settings.complexMode = calc::ComplexMode::Rectangular;
        QCOMPARE(answer("sqrt(-4)", context), QStringLiteral("2i"));
        QCOMPARE(answer("i^2", context), QStringLiteral("-1"));
        QCOMPARE(answer("(3+4i)", context), QStringLiteral("3+4i"));
        QCOMPARE(answer("abs(3+4i)", context), QStringLiteral("5"));
        QCOMPARE(answer("conj(3+4i)", context), QStringLiteral("3-4i"));
    }

    void runsTheCalculusRoutines() {
        calc::Context context;
        QVERIFY(std::abs(number("nDeriv(X²,X,3)", context) - 6.0) < 1e-6);
        QVERIFY(std::abs(number("fnInt(X²,X,0,1)", context) - 1.0 / 3.0) < 1e-9);
        QVERIFY(std::abs(number("fnInt(sin(X),X,0,pi)", context) - 2.0) < 1e-9);
        QVERIFY(std::abs(number("solve(X²-2,X,1)", context) - std::sqrt(2.0)) < 1e-9);
        QVERIFY(std::abs(number("fMin(X²-4X,X,0,5)", context) - 2.0) < 1e-6);
        QVERIFY(std::abs(number("fMax(-X²+4X,X,0,5)", context) - 2.0) < 1e-6);
    }

    void evaluatesStoredFunctions() {
        calc::Context context;
        context.functions.insert(QStringLiteral("Y1"), QStringLiteral("X²+1"));
        QCOMPARE(answer("Y1(3)", context), QStringLiteral("10"));
        QVERIFY(std::abs(number("nDeriv(Y1(X),X,2)", context) - 4.0) < 1e-6);
    }

    void computesDistributions() {
        calc::Context context;
        QVERIFY(std::abs(number("normalcdf(-1,1)", context) - 0.682689492137) < 1e-9);
        QVERIFY(std::abs(number("invNorm(0.975)", context) - 1.959963984540) < 1e-8);
        QVERIFY(std::abs(number("normalpdf(0)", context) - 0.398942280401) < 1e-10);
        QVERIFY(std::abs(number("binompdf(10,0.5,5)", context) - 0.24609375) < 1e-12);
        QVERIFY(std::abs(number("binomcdf(10,0.5,5)", context) - 0.623046875) < 1e-12);
        QVERIFY(std::abs(number("poissonpdf(2,3)", context) - 0.180447044315) < 1e-10);
        QVERIFY(std::abs(number("geometpdf(0.25,3)", context) - 0.140625) < 1e-12);
        QVERIFY(std::abs(number("tcdf(-2,2,10)", context) - 0.926611965229) < 1e-9);
        QVERIFY(std::abs(number("invT(0.975,10)", context) - 2.228138851986) < 1e-7);
        QVERIFY(std::abs(number("chi2cdf(0,3.84,1)", context) - 0.949956478751) < 1e-9);
        QVERIFY(std::abs(number("Fcdf(0,4.1028,2,10)", context) - 0.949999422835) < 1e-9);
    }

    void reportsTheSameErrorsTheCalculatorDoes() {
        calc::Context context;
        QCOMPARE(errorFor("1/0", context), QStringLiteral("DIVIDE BY 0"));
        QCOMPARE(errorFor("ln(0)", context), QStringLiteral("DOMAIN"));
        QCOMPARE(errorFor("sin⁻¹(2)", context), QStringLiteral("DOMAIN"));
        QCOMPARE(errorFor("2+", context), QStringLiteral("SYNTAX"));
        QCOMPARE(errorFor("[[1,2],[3,4]]+[[1,2,3],[4,5,6]]", context),
                 QStringLiteral("DIM MISMATCH"));
        QCOMPARE(errorFor("[[1,2],[2,4]]⁻¹", context), QStringLiteral("SINGULAR MAT"));
        QCOMPARE(errorFor("L3", context), QStringLiteral("UNDEFINED"));
    }

    void comparesAndCombinesLogically() {
        calc::Context context;
        QCOMPARE(answer("3>2", context), QStringLiteral("1"));
        QCOMPARE(answer("3<2", context), QStringLiteral("0"));
        QCOMPARE(answer("1 and 0", context), QStringLiteral("0"));
        QCOMPARE(answer("1 or 0", context), QStringLiteral("1"));
        QCOMPARE(answer("not(0)", context), QStringLiteral("1"));
    }


    // --- Backend -----------------------------------------------------------

    void keepsAnswersAndHistory() {
        Backend calculator;
        calculator.clearHistory();
        QCOMPARE(calculator.submit(QStringLiteral("2+3")).value("answer"), QStringLiteral("5"));
        QCOMPARE(calculator.submit(QStringLiteral("Ans*2")).value("answer"), QStringLiteral("10"));
        QCOMPARE(calculator.lastAnswer(), QStringLiteral("10"));
        QCOMPARE(calculator.history().size(), 2);
    }

    void reportsErrorsLikeTheCalculator() {
        Backend calculator;
        calculator.clearHistory();
        const QVariantMap result = calculator.submit(QStringLiteral("1/0"));
        QCOMPARE(result.value("ok").toBool(), false);
        QCOMPARE(result.value("answer").toString(), QStringLiteral("ERR:DIVIDE BY 0"));
    }

    void samplesEquationsForTheGraph() {
        Backend calculator;
        clearAllEquations(calculator);
        calculator.setFunctionBody(0, QStringLiteral("X²"));
        calculator.zoomStandard();

        const QVector<Curve> curves = calculator.sampleCurves(100);
        QCOMPARE(curves.size(), 1);
        QVERIFY(curves.first().points.size() > 50);
        for (const QPointF &point : curves.first().points)
            QVERIFY(std::abs(point.y() - point.x() * point.x()) < 1e-9);
    }

    void switchesEquationSetWithTheMode() {
        Backend calculator;
        calculator.setGraphMode(2);
        const QVariantList polar = calculator.functions();
        QCOMPARE(polar.size(), 6);
        QCOMPARE(polar.first().toMap().value("name").toString(), QStringLiteral("r1"));
        calculator.setGraphMode(0);
        QCOMPARE(calculator.functions().size(), 10);
    }

    void graphsPolarAndParametricCurves() {
        Backend calculator;
        clearAllEquations(calculator);
        calculator.setGraphMode(2);
        calculator.setFunctionBody(0, QStringLiteral("2"));
        const QVector<Curve> circle = calculator.sampleCurves(100);
        QCOMPARE(circle.size(), 1);
        for (const QPointF &point : circle.first().points) {
            if (std::isfinite(point.y()))
                QVERIFY(std::abs(std::hypot(point.x(), point.y()) - 2.0) < 1e-9);
        }

        calculator.setGraphMode(1);
        calculator.setFunctionBody(0, QStringLiteral("cos(T)"));
        calculator.setFunctionBody(1, QStringLiteral("sin(T)"));
        const QVector<Curve> unitCircle = calculator.sampleCurves(100);
        QCOMPARE(unitCircle.size(), 1);
        for (const QPointF &point : unitCircle.first().points) {
            if (std::isfinite(point.y()))
                QVERIFY(std::abs(std::hypot(point.x(), point.y()) - 1.0) < 1e-9);
        }
        calculator.setGraphMode(0);
    }

    void zoomsAndRemembersThePreviousWindow() {
        Backend calculator;
        calculator.zoomStandard();
        QCOMPARE(calculator.window().value("xMin").toDouble(), -10.0);
        calculator.zoomIn();
        QCOMPARE(calculator.window().value("xMin").toDouble(), -2.5);
        calculator.zoomPrevious();
        QCOMPARE(calculator.window().value("xMin").toDouble(), -10.0);

        calculator.zoomDecimal();
        QCOMPARE(calculator.window().value("xMax").toDouble(), 4.7);
        calculator.zoomBox(-1, -2, 3, 4);
        QCOMPARE(calculator.window().value("xMin").toDouble(), -1.0);
        QCOMPARE(calculator.window().value("yMax").toDouble(), 4.0);
    }

    void fillsTheTable() {
        Backend calculator;
        clearAllEquations(calculator);
        calculator.setFunctionBody(0, QStringLiteral("2X"));
        calculator.setTableStart(0);
        calculator.setTableStep(1);

        const QVariantList rows = calculator.tableRows(3);
        QCOMPARE(rows.size(), 4);  // one header plus three rows
        QCOMPARE(rows.at(0).toMap().value("header").toBool(), true);
        QCOMPARE(rows.at(1).toMap().value("parameter").toString(), QStringLiteral("0"));
        QCOMPARE(rows.at(1).toMap().value("columns").toStringList().first(), QStringLiteral("0"));
        QCOMPARE(rows.at(3).toMap().value("columns").toStringList().first(), QStringLiteral("4"));
    }

    void tracesAlongTheCurve() {
        Backend calculator;
        clearAllEquations(calculator);
        calculator.setFunctionBody(0, QStringLiteral("X²"));
        calculator.zoomStandard();
        calculator.startTrace();
        QVERIFY(calculator.tracing());
        calculator.traceTo(3);
        QCOMPARE(calculator.traceX(), 3.0);
        QCOMPARE(calculator.traceY(), 9.0);
        calculator.stopTrace();
        QVERIFY(!calculator.tracing());
    }

    void runsTheCalcMenu() {
        Backend calculator;
        clearAllEquations(calculator);
        calculator.setFunctionBody(0, QStringLiteral("X²-4"));
        calculator.setFunctionBody(1, QStringLiteral("X"));

        const QVariantMap zero = calculator.calcZero(0, 0, 5);
        QVERIFY(zero.value("ok").toBool());
        QVERIFY(std::abs(zero.value("x").toDouble() - 2.0) < 1e-6);

        const QVariantMap minimum = calculator.calcExtremum(0, -5, 5, false);
        QVERIFY(std::abs(minimum.value("x").toDouble()) < 1e-5);

        const QVariantMap slope = calculator.calcDerivative(0, 3);
        QVERIFY(std::abs(slope.value("y").toDouble() - 6.0) < 1e-6);

        const QVariantMap area = calculator.calcIntegral(0, 0, 3);
        QVERIFY(std::abs(area.value("y").toDouble() - (9.0 - 12.0)) < 1e-8);

        // X² - 4 meets X where x² - x - 4 = 0, at (1 + √17) / 2.
        const QVariantMap crossing = calculator.calcIntersect(0, 1, 0, 5);
        QVERIFY(crossing.value("ok").toBool());
        QVERIFY(std::abs(crossing.value("x").toDouble() - (1.0 + std::sqrt(17.0)) / 2.0) < 1e-6);
    }

    void editsListsAndComputesStatistics() {
        Backend calculator;
        calculator.clearList(QStringLiteral("L1"));
        calculator.clearList(QStringLiteral("L2"));
        const double xs[] = {1, 2, 3, 4};
        const double ys[] = {2, 4, 6, 8};
        for (int i = 0; i < 4; ++i) {
            calculator.setListCell(QStringLiteral("L1"), i, QString::number(xs[i]));
            calculator.setListCell(QStringLiteral("L2"), i, QString::number(ys[i]));
        }
        QCOMPARE(calculator.listColumn(QStringLiteral("L1")).size(), 4);

        const QVariantList summary =
            calculator.oneVariableStats(QStringLiteral("L1"), QString());
        QCOMPARE(summary.at(0).toMap().value("label").toString(), QStringLiteral("x̄"));
        QCOMPARE(summary.at(0).toMap().value("value").toString(), QStringLiteral("2.5"));

        const QVariantList both =
            calculator.twoVariableStats(QStringLiteral("L1"), QStringLiteral("L2"));
        QVERIFY(!both.isEmpty());

        const QVariantList fit = calculator.regression(0, QStringLiteral("L1"),
                                                       QStringLiteral("L2"), -1);
        QCOMPARE(fit.at(1).toMap().value("label").toString(), QStringLiteral("a"));
        QCOMPARE(fit.at(1).toMap().value("value").toString(), QStringLiteral("2"));
        QCOMPARE(fit.at(3).toMap().value("value").toString(), QStringLiteral("1"));  // r²
    }

    void plotsStatisticalData() {
        Backend calculator;
        calculator.clearList(QStringLiteral("L1"));
        calculator.clearList(QStringLiteral("L2"));
        for (int i = 0; i < 4; ++i) {
            calculator.setListCell(QStringLiteral("L1"), i, QString::number(i + 1));
            calculator.setListCell(QStringLiteral("L2"), i, QString::number((i + 1) * 2));
        }
        calculator.setPlot(0, true, 0, QStringLiteral("L1"), QStringLiteral("L2"), 0);
        const QVector<PlotPoints> plots = calculator.samplePlots();
        QCOMPARE(plots.size(), 1);
        QCOMPARE(plots.first().points.size(), 4);
        QCOMPARE(plots.first().points.at(2), QPointF(3, 6));
        calculator.setPlot(0, false, 0, QStringLiteral("L1"), QStringLiteral("L2"), 0);
    }

    void editsMatrices() {
        Backend calculator;
        calculator.resizeMatrix(QStringLiteral("[A]"), 2, 2);
        QVERIFY(calculator.setMatrixCell(QStringLiteral("[A]"), 0, 0, QStringLiteral("1")));
        QVERIFY(calculator.setMatrixCell(QStringLiteral("[A]"), 0, 1, QStringLiteral("2")));
        QVERIFY(calculator.setMatrixCell(QStringLiteral("[A]"), 1, 0, QStringLiteral("3")));
        QVERIFY(calculator.setMatrixCell(QStringLiteral("[A]"), 1, 1, QStringLiteral("4")));

        const QVariantMap data = calculator.matrixData(QStringLiteral("[A]"));
        QCOMPARE(data.value("rows").toInt(), 2);
        QCOMPARE(calculator.submit(QStringLiteral("det([A])")).value("answer"),
                 QStringLiteral("-2"));
    }

    void remembersModesBetweenRuns() {
        {
            Backend calculator;
            calculator.setAngleMode(1);
            calculator.setNumberFormat(1);
        }
        Backend reopened;
        QCOMPARE(reopened.angleMode(), 1);
        QCOMPARE(reopened.numberFormat(), 1);
        reopened.setAngleMode(0);
        reopened.setNumberFormat(0);
    }

    // --- Statistics --------------------------------------------------------

    void summarisesOneVariable() {
        const stats::OneVariable summary = stats::oneVariable({1, 2, 3, 4, 5}, {});
        QCOMPARE(summary.count, 5.0);
        QCOMPARE(summary.mean, 3.0);
        QCOMPARE(summary.median, 3.0);
        QCOMPARE(summary.lowerQuartile, 1.5);
        QCOMPARE(summary.upperQuartile, 4.5);
        QVERIFY(std::abs(summary.sampleDeviation - std::sqrt(2.5)) < 1e-12);
        QVERIFY(std::abs(summary.populationDeviation - std::sqrt(2.0)) < 1e-12);
    }

    void fitsEveryRegressionModel() {
        const std::vector<double> x = {1, 2, 3, 4, 5};

        const stats::Regression line = stats::fit(stats::Model::Linear, x, {3, 5, 7, 9, 11});
        QVERIFY(line.valid);
        QVERIFY(std::abs(line.coefficients[0] - 2.0) < 1e-9);
        QVERIFY(std::abs(line.coefficients[1] - 1.0) < 1e-9);
        QVERIFY(std::abs(line.correlation - 1.0) < 1e-9);

        const stats::Regression quadratic =
            stats::fit(stats::Model::Quadratic, x, {1, 4, 9, 16, 25});
        QVERIFY(std::abs(quadratic.coefficients[0] - 1.0) < 1e-9);

        const stats::Regression exponential =
            stats::fit(stats::Model::Exponential, x, {6, 18, 54, 162, 486});
        QVERIFY(std::abs(exponential.coefficients[0] - 2.0) < 1e-8);
        QVERIFY(std::abs(exponential.coefficients[1] - 3.0) < 1e-8);

        const stats::Regression power = stats::fit(stats::Model::Power, x, {2, 8, 18, 32, 50});
        QVERIFY(std::abs(power.coefficients[0] - 2.0) < 1e-8);
        QVERIFY(std::abs(power.coefficients[1] - 2.0) < 1e-8);

        const stats::Regression logarithmic =
            stats::fit(stats::Model::Logarithmic, x,
                       {1, 1 + 2 * std::log(2.0), 1 + 2 * std::log(3.0), 1 + 2 * std::log(4.0),
                        1 + 2 * std::log(5.0)});
        QVERIFY(std::abs(logarithmic.coefficients[0] - 1.0) < 1e-8);
        QVERIFY(std::abs(logarithmic.coefficients[1] - 2.0) < 1e-8);

        const stats::Regression medianMedian =
            stats::fit(stats::Model::MedianMedian, x, {3, 5, 7, 9, 11});
        QVERIFY(medianMedian.valid);
        QVERIFY(std::abs(medianMedian.coefficients[0] - 2.0) < 1e-9);
    }

    void fitsTheIterativeModels() {
        std::vector<double> x;
        std::vector<double> sine;
        for (int i = 0; i < 24; ++i) {
            const double value = double(i) * 0.25;
            x.push_back(value);
            sine.push_back(3.0 * std::sin(1.5 * value + 0.4) + 2.0);
        }
        const stats::Regression wave = stats::fit(stats::Model::Sinusoidal, x, sine);
        QVERIFY(wave.valid);
        QVERIFY(wave.determination > 0.999);

        std::vector<double> logistic;
        for (double value : x)
            logistic.push_back(8.0 / (1.0 + 4.0 * std::exp(-1.2 * value)));
        const stats::Regression curve = stats::fit(stats::Model::Logistic, x, logistic);
        QVERIFY(curve.valid);
        QVERIFY(curve.determination > 0.999);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend calculator;
        QCOMPARE(calculator.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(calculator.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(calculator.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(calculator.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!calculator.darkMode());
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmacalcTest)
#include "tst_omacalc.moc"
