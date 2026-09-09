#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>

#include "backend.h"
#include "engine.h"
#include "special.h"

namespace {
void press(Backend &calculator, const QString &keys) {
    const QStringList sequence = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &key : sequence)
        calculator.pressKey(key);
}

// Engine helpers: evaluate a line the way the home screen does and hand back
// either the formatted answer, the raw number, or the error name.
QString answer(const QString &source, calc::Context &context) {
    const calc::Value value = calc::evaluate(source, context);
    return calc::formatWithHint(value, context.settings, context.displayHint);
}

double number(const QString &source, calc::Context &context) {
    return calc::evaluate(source, context).number().real();
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

    void startsAtZero() {
        Backend calculator;
        QCOMPARE(calculator.display(), QStringLiteral("0"));
        QCOMPARE(calculator.expression(), QString());
    }

    void calculatesWithPrecedence() {
        Backend calculator;
        press(calculator, "4 2 × 3 + 7 =");
        QCOMPARE(calculator.display(), QStringLiteral("133"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 × 3 + 7"));

        press(calculator, "clear 2 + 3 × 4 =");
        QCOMPARE(calculator.display(), QStringLiteral("14"));

        press(calculator, "clear 1 0 - 4 ÷ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("8"));
    }

    void showsEntryWhileTyping() {
        Backend calculator;
        press(calculator, "4 2 ×");
        QCOMPARE(calculator.display(), QStringLiteral("42"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 ×"));

        press(calculator, "3");
        QCOMPARE(calculator.display(), QStringLiteral("3"));
    }

    void handlesDecimals() {
        Backend calculator;
        press(calculator, ". 5 + . 2 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.75"));

        // Binary-float noise stays out of the display.
        press(calculator, "clear 0 . 1 + 0 . 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.3"));

        // A second decimal point in one number is ignored.
        press(calculator, "clear 1 . 5 . 5");
        QCOMPARE(calculator.display(), QStringLiteral("1.55"));
    }

    void divisionByZeroErrors() {
        Backend calculator;
        press(calculator, "1 ÷ 0 =");
        QCOMPARE(calculator.display(), QStringLiteral("Error"));

        // Digits recover from an error without an explicit clear.
        press(calculator, "5");
        QCOMPARE(calculator.display(), QStringLiteral("5"));
    }

    void percentOfRunningTotal() {
        Backend calculator;

        // With a pending + or −, x% means x percent of the running total.
        press(calculator, "2 0 0 + 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("220"));

        press(calculator, "clear 2 0 0 - 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("180"));

        // With × or ÷, or standalone, x% is simply x ÷ 100.
        press(calculator, "clear 2 0 0 × 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("20"));

        press(calculator, "clear 5 0 %");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));

        // After equals, percent picks up from the result.
        press(calculator, "clear 4 0 + 1 0 = %");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));
    }

    void percentAndSign() {
        Backend calculator;
        press(calculator, "8 sign");
        QCOMPARE(calculator.display(), QStringLiteral("-8"));
        press(calculator, "sign");
        QCOMPARE(calculator.display(), QStringLiteral("8"));

        press(calculator, "clear 4 + 8 sign =");
        QCOMPARE(calculator.display(), QStringLiteral("-4"));
    }

    void signStartsNewOperand() {
        Backend calculator;

        // Sign with nothing typed starts a fresh negative operand rather than
        // negating the previous one: 4 + ± 2 = is 4 + (-2), not 4 + (-42).
        press(calculator, "4 + sign 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("2"));
        QCOMPARE(calculator.expression(), QStringLiteral("4 + -2"));

        press(calculator, "clear sign");
        QCOMPARE(calculator.display(), QStringLiteral("-0"));
        press(calculator, "5");
        QCOMPARE(calculator.display(), QStringLiteral("-5"));
    }

    void chainsWithFullPrecision() {
        Backend calculator;

        // Chaining continues from the exact value, not the rounded display.
        press(calculator, "1 ÷ 3 = × 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));

        // Integers within the 15-digit entry limit survive exactly.
        press(calculator, "clear 9 9 9 9 9 9 9 9 9 9 9 9 9 9 =");
        QCOMPARE(calculator.display(), QStringLiteral("99999999999999"));
    }

    void capsEntryAtFifteenDigits() {
        Backend calculator;
        press(calculator, "1 2 3 4 5 6 7 8 9 1 2 3 4 5 6 7 8");
        QCOMPARE(calculator.display(), QStringLiteral("123456789123456"));

        // The decimal point does not count against the digit cap.
        press(calculator, "clear . 1 2 3 4 5 6 7 8 9 1 2 3 4 5 6 7");
        QCOMPARE(calculator.display(), QStringLiteral("0.123456789123456"));
    }

    void backspaceEdits() {
        Backend calculator;
        press(calculator, "1 2 3 backspace");
        QCOMPARE(calculator.display(), QStringLiteral("12"));

        press(calculator, "backspace backspace backspace");
        QCOMPARE(calculator.display(), QStringLiteral("0"));
    }

    void chainsFromResult() {
        Backend calculator;
        press(calculator, "6 × 7 = × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("84"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 × 2"));

        // A digit after equals starts fresh instead of appending to the result.
        press(calculator, "9");
        QCOMPARE(calculator.display(), QStringLiteral("9"));
        QCOMPARE(calculator.expression(), QString());
    }

    void replacesDanglingOperator() {
        Backend calculator;
        press(calculator, "4 + × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("8"));

        // Equals with a trailing operator drops it.
        press(calculator, "clear 9 + =");
        QCOMPARE(calculator.display(), QStringLiteral("9"));
    }

    void pastesNumbers() {
        Backend calculator;
        QClipboard *clipboard = QGuiApplication::clipboard();

        clipboard->setText(QStringLiteral(" 42.5 "));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("42.5"));

        // A pasted number is a normal entry that calculates like any other.
        press(calculator, "+ . 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("43"));

        // Decimal commas are welcome; garbage is ignored.
        clipboard->setText(QStringLiteral("1,5"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("1.5"));

        clipboard->setText(QStringLiteral("not a number"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("1.5"));
    }

    void evaluatesTokens() {
        bool ok = false;
        QCOMPARE(Backend::evaluateTokens({"2", "+", "3", "×", "4"}, &ok), 14.0);
        QVERIFY(ok);

        Backend::evaluateTokens({"2", "+"}, &ok);
        QVERIFY(!ok);

        Backend::evaluateTokens({"1", "÷", "0"}, &ok);
        QVERIFY(!ok);
    }

    void formatsNumbers() {
        QCOMPARE(Backend::formatNumber(133), QStringLiteral("133"));
        QCOMPARE(Backend::formatNumber(0.1 + 0.2), QStringLiteral("0.3"));
        QCOMPARE(Backend::formatNumber(-0.0), QStringLiteral("0"));
        QCOMPARE(Backend::formatNumber(1e15), QStringLiteral("1e+15"));
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
