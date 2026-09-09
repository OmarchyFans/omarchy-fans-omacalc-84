#pragma once

// A TI-style expression engine: tokenizer, recursive-descent parser and
// evaluator. Values are complex scalars, lists or matrices, mirroring the
// three data types a TI-84 works with. Nothing here touches the GUI, so the
// whole engine is exercised directly from the test suite.

#include <QHash>
#include <QString>
#include <QStringList>

#include <complex>
#include <memory>
#include <vector>

namespace calc {

using Complex = std::complex<double>;

// Errors carry a TI error name ("SYNTAX", "DIVIDE BY 0", ...) so the display
// can show the same "ERR:" line the calculator it imitates would.
struct Error {
    QString code;
    explicit Error(QString code) : code(std::move(code)) {}
};

[[noreturn]] void raise(const char *code);

struct Matrix {
    int rows = 0;
    int cols = 0;
    std::vector<Complex> cells;  // row-major

    Matrix() = default;
    Matrix(int rows, int cols) : rows(rows), cols(cols), cells(size_t(rows) * cols, Complex(0, 0)) {}

    Complex &at(int r, int c) { return cells[size_t(r) * cols + c]; }
    const Complex &at(int r, int c) const { return cells[size_t(r) * cols + c]; }
    bool isEmpty() const { return rows == 0 || cols == 0; }
};

class Value {
public:
    enum Kind { Number, List, MatrixValue };

    Value() = default;
    Value(Complex number) : m_kind(Number), m_number(number) {}
    Value(double number) : m_kind(Number), m_number(number, 0) {}
    static Value fromList(std::vector<Complex> items);
    static Value fromMatrix(Matrix matrix);

    Kind kind() const { return m_kind; }
    bool isNumber() const { return m_kind == Number; }
    bool isList() const { return m_kind == List; }
    bool isMatrix() const { return m_kind == MatrixValue; }

    // Accessors raise ERR:DATA TYPE rather than returning something bogus, so
    // a wrong-typed argument surfaces at the point of use.
    Complex number() const;
    double real() const;
    const std::vector<Complex> &list() const;
    const Matrix &matrix() const;

private:
    Kind m_kind = Number;
    Complex m_number;
    std::vector<Complex> m_list;
    Matrix m_matrix;
};

enum class AngleMode { Radian, Degree };
enum class NumberFormat { Normal, Scientific, Engineering };
enum class ComplexMode { Real, Rectangular, Polar };

struct Settings {
    AngleMode angle = AngleMode::Radian;
    NumberFormat format = NumberFormat::Normal;
    int fixDigits = -1;  // -1 is Float; 0-9 fix that many decimals
    ComplexMode complexMode = ComplexMode::Real;
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

// ▶Frac, ▶Polar and friends do not change a value, they change how the answer
// is shown; evaluation records the request here for the display to pick up.
enum class DisplayHint { None, Fraction, Decimal, Rectangular, Polar, DegreeMinuteSecond };

class Context {
public:
    Settings settings;
    DisplayHint displayHint = DisplayHint::None;

    // Scalars (A-Z, theta, Ans), lists (L1-L6 and named lists) and matrices
    // ([A]-[J]) all live in one map keyed by the name as typed.
    QHash<QString, Value> variables;
    // Y1-Y0, parametric X1T/Y1T, polar r1-r6 and sequence definitions are kept
    // as source text so the editor round-trips exactly what was typed.
    QHash<QString, QString> functions;

    bool has(const QString &name) const;
    Value value(const QString &name) const;
    void store(const QString &name, const Value &value);

    // Evaluates a stored definition (Y1, r1, ...) with `parameter` bound to
    // `argument` for the duration of the call.
    Value callDefinition(const QString &name, const Value &argument);
    NodePtr parseCached(const QString &source);
    // Evaluates `source` with `parameter` bound to `argument`, used by the
    // grapher, the table and every CALC-menu routine.
    Value callWith(const QString &source, const QString &parameter, const Value &argument);

    void clearCache() { m_cache.clear(); }

private:
    QHash<QString, NodePtr> m_cache;
    int m_depth = 0;
};

// Parses and evaluates. Both raise calc::Error on bad input.
NodePtr parse(const QString &source);
Value evaluate(const NodePtr &program, Context &context);
Value evaluate(const QString &source, Context &context);

// Formatting follows the MODE screen: Normal/Sci/Eng and Float/Fix.
QString formatNumber(Complex value, const Settings &settings);
QString formatValue(const Value &value, const Settings &settings);
QString formatReal(double value, const Settings &settings);
QString formatWithHint(const Value &value, const Settings &settings, DisplayHint hint);
// Exact-looking fractions for ▶Frac, with the same 4-digit denominator ceiling
// the calculator uses before it gives up and shows a decimal.
bool toFraction(double value, long long *numerator, long long *denominator);

// Shared numeric helpers, also used by the grapher and the CALC menu.
double toRadians(double angle, AngleMode mode);
double fromRadians(double radians, AngleMode mode);

}  // namespace calc
