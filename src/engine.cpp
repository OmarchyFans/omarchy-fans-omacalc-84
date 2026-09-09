#include "engine.h"

#include "special.h"

#include <QLocale>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace calc {

void raise(const char *code) { throw Error(QString::fromUtf8(code)); }

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

bool isReal(const Complex &c) { return c.imag() == 0.0; }

double requireReal(const Complex &c) {
    if (!isReal(c))
        raise("DATA TYPE");
    return c.real();
}

// Integers arrive as doubles; anything within a whisker of a whole number is
// treated as that number so 3! survives a round trip through the parser.
bool wholeNumber(double v, long long *out) {
    if (!std::isfinite(v))
        return false;
    const double rounded = std::round(v);
    if (std::abs(v - rounded) > 1e-9)
        return false;
    if (std::abs(rounded) > 9.0e15)
        return false;
    *out = static_cast<long long>(rounded);
    return true;
}
}  // namespace

double toRadians(double angle, AngleMode mode) {
    return mode == AngleMode::Degree ? angle * kPi / 180.0 : angle;
}

double fromRadians(double radians, AngleMode mode) {
    return mode == AngleMode::Degree ? radians * 180.0 / kPi : radians;
}

// --- Value -----------------------------------------------------------------

Value Value::fromList(std::vector<Complex> items) {
    Value v;
    v.m_kind = List;
    v.m_list = std::move(items);
    return v;
}

Value Value::fromMatrix(Matrix matrix) {
    Value v;
    v.m_kind = MatrixValue;
    v.m_matrix = std::move(matrix);
    return v;
}

Complex Value::number() const {
    if (m_kind != Number)
        raise("DATA TYPE");
    return m_number;
}

double Value::real() const { return requireReal(number()); }

const std::vector<Complex> &Value::list() const {
    if (m_kind != List)
        raise("DATA TYPE");
    return m_list;
}

const Matrix &Value::matrix() const {
    if (m_kind != MatrixValue)
        raise("DATA TYPE");
    return m_matrix;
}

// --- Tokenizer -------------------------------------------------------------

namespace {

struct Token {
    enum Type { Number, Name, Operator, End };
    Type type = End;
    QString text;
    double number = 0;
    int position = 0;
};

// Names are matched against a fixed table, longest first, exactly the way a
// TI keyboard produces whole tokens: "AB" is A times B, but "abs" is one name.
const QStringList &nameTable() {
    static const QStringList names = [] {
        QStringList list;
        list << QStringLiteral("normalpdf") << QStringLiteral("normalcdf")
             << QStringLiteral("invNorm") << QStringLiteral("invT")
             << QStringLiteral("binompdf") << QStringLiteral("binomcdf")
             << QStringLiteral("poissonpdf") << QStringLiteral("poissoncdf")
             << QStringLiteral("geometpdf") << QStringLiteral("geometcdf")
             << QStringLiteral("tpdf") << QStringLiteral("tcdf")
             << QStringLiteral("Fpdf") << QStringLiteral("Fcdf")
             << QStringLiteral("χ²pdf") << QStringLiteral("χ²cdf")
             << QStringLiteral("chi2pdf") << QStringLiteral("chi2cdf")
             << QStringLiteral("randInt") << QStringLiteral("randNorm")
             << QStringLiteral("randBin") << QStringLiteral("randM")
             << QStringLiteral("rand")
             << QStringLiteral("nDeriv") << QStringLiteral("fnInt")
             << QStringLiteral("fMin") << QStringLiteral("fMax")
             << QStringLiteral("solve")
             << QStringLiteral("abs") << QStringLiteral("round")
             << QStringLiteral("iPart") << QStringLiteral("fPart")
             << QStringLiteral("int") << QStringLiteral("min")
             << QStringLiteral("max") << QStringLiteral("lcm")
             << QStringLiteral("gcd") << QStringLiteral("remainder")
             << QStringLiteral("sign") << QStringLiteral("floor")
             << QStringLiteral("ceiling") << QStringLiteral("ceil")
             << QStringLiteral("conj") << QStringLiteral("real")
             << QStringLiteral("imag") << QStringLiteral("angle")
             << QStringLiteral("sinh⁻¹") << QStringLiteral("cosh⁻¹")
             << QStringLiteral("tanh⁻¹") << QStringLiteral("asinh")
             << QStringLiteral("acosh") << QStringLiteral("atanh")
             << QStringLiteral("sinh") << QStringLiteral("cosh")
             << QStringLiteral("tanh")
             << QStringLiteral("sin⁻¹") << QStringLiteral("cos⁻¹")
             << QStringLiteral("tan⁻¹") << QStringLiteral("arcsin")
             << QStringLiteral("arccos") << QStringLiteral("arctan")
             << QStringLiteral("asin") << QStringLiteral("acos")
             << QStringLiteral("atan") << QStringLiteral("sin")
             << QStringLiteral("cos") << QStringLiteral("tan")
             << QStringLiteral("logBASE") << QStringLiteral("log")
             << QStringLiteral("ln") << QStringLiteral("exp")
             << QStringLiteral("sqrt") << QStringLiteral("cbrt")
             << QStringLiteral("xroot")
             << QStringLiteral("sum") << QStringLiteral("prod")
             << QStringLiteral("mean") << QStringLiteral("median")
             << QStringLiteral("stdDev") << QStringLiteral("variance")
             << QStringLiteral("cumSum") << QStringLiteral("ΔList")
             << QStringLiteral("augment") << QStringLiteral("sortA")
             << QStringLiteral("sortD") << QStringLiteral("seq")
             << QStringLiteral("dim") << QStringLiteral("fill")
             << QStringLiteral("det") << QStringLiteral("identity")
             << QStringLiteral("rref") << QStringLiteral("ref")
             << QStringLiteral("transpose")
             << QStringLiteral("nPr") << QStringLiteral("nCr")
             << QStringLiteral("and") << QStringLiteral("xor")
             << QStringLiteral("or") << QStringLiteral("not")
             << QStringLiteral("Ans") << QStringLiteral("pi")
             << QStringLiteral("theta");
        std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
            return a.size() > b.size();
        });
        return list;
    }();
    return names;
}

bool startsWithName(const QString &source, int index, QString *name) {
    for (const QString &candidate : nameTable()) {
        if (source.mid(index, candidate.size()) == candidate) {
            // A name only ends where a longer identifier could not continue,
            // so "sinh" never tokenizes as "sin" followed by "h".
            *name = candidate;
            return true;
        }
    }
    return false;
}

bool isNameChar(QChar c) { return c.isLetterOrNumber() || c == u'⁻' || c == u'¹'; }

QList<Token> tokenize(const QString &source) {
    QList<Token> tokens;
    int i = 0;
    const int n = source.size();

    auto push = [&](Token::Type type, const QString &text, double number, int pos) {
        Token t;
        t.type = type;
        t.text = text;
        t.number = number;
        t.position = pos;
        tokens.append(t);
    };

    while (i < n) {
        const QChar c = source.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }

        if (c.isDigit() || (c == u'.' && i + 1 < n && source.at(i + 1).isDigit())) {
            const int start = i;
            while (i < n && source.at(i).isDigit())
                ++i;
            if (i < n && source.at(i) == u'.') {
                ++i;
                while (i < n && source.at(i).isDigit())
                    ++i;
            }
            // An exponent only counts when digits actually follow, so "2E"
            // stays 2 times the variable E.
            if (i < n && (source.at(i) == u'e' || source.at(i) == u'E' || source.at(i) == u'ᴇ')) {
                int probe = i + 1;
                if (probe < n && (source.at(probe) == u'-' || source.at(probe) == u'+'
                                  || source.at(probe) == u'−'))
                    ++probe;
                if (probe < n && source.at(probe).isDigit()) {
                    while (probe < n && source.at(probe).isDigit())
                        ++probe;
                    i = probe;
                }
            }
            QString text = source.mid(start, i - start);
            text.replace(u'ᴇ', u'e');
            text.replace(u'−', u'-');
            bool ok = false;
            const double value = QLocale::c().toDouble(text, &ok);
            if (!ok)
                raise("SYNTAX");
            push(Token::Number, text, value, start);
            continue;
        }

        // Matrix variables are written [A] through [J]; a bare [ opens a
        // matrix literal instead.
        if (c == u'[' && i + 2 < n && source.at(i + 1).isLetter() && source.at(i + 2) == u']') {
            push(Token::Name, source.mid(i, 3), 0, i);
            i += 3;
            continue;
        }

        QString name;
        if (startsWithName(source, i, &name)) {
            const int end = i + name.size();
            // Only accept the table match if the source does not continue with
            // more identifier characters that would make it a different word.
            const bool tableIsWord = name.at(0).isLetter();
            const bool continues = end < n && isNameChar(source.at(end)) && tableIsWord
                && source.at(end).isLetter() && source.at(end).isLower();
            if (!continues) {
                push(Token::Name, name, 0, i);
                i = end;
                continue;
            }
        }

        // ˣ√, ᵀ and ʳ are letters as far as Unicode is concerned, but on the
        // keypad they are operators, so they must not become variable names.
        const bool operatorGlyph = c == u'ˣ' || c == u'ᵀ' || c == u'ʳ';
        if (!operatorGlyph && (c.isLetter() || c == u'θ' || c == u'π')) {
            // Single letters are variables: A-Z, plus theta and the list and
            // function names that carry a digit suffix (L1, Y1, r1, X1T).
            const int start = i;
            QString ident(c);
            ++i;
            if ((c == u'L' || c == u'Y' || c == u'r' || c == u'X' || c == u'u' || c == u'v'
                 || c == u'w')
                && i < n && source.at(i).isDigit()) {
                ident += source.at(i);
                ++i;
                if (i < n && (source.at(i) == u'T' || source.at(i) == u't')) {
                    ident += u'T';
                    ++i;
                }
            }
            push(Token::Name, ident, 0, start);
            continue;
        }

        static const QStringList multiCharOperators = {
            QStringLiteral("⁻¹"), QStringLiteral("▶Frac"), QStringLiteral("▶Dec"),
            QStringLiteral("▶Rect"), QStringLiteral("▶Polar"), QStringLiteral("▶DMS"),
            QStringLiteral("ˣ√"),  QStringLiteral("->"),     QStringLiteral("<="),
            QStringLiteral(">="),  QStringLiteral("!="),     QStringLiteral("^-1"),
        };
        bool matchedOperator = false;
        for (const QString &op : multiCharOperators) {
            if (source.mid(i, op.size()) == op) {
                push(Token::Operator, op, 0, i);
                i += op.size();
                matchedOperator = true;
                break;
            }
        }
        if (matchedOperator)
            continue;

        push(Token::Operator, QString(c), 0, i);
        ++i;
    }

    Token end;
    end.type = Token::End;
    end.position = n;
    tokens.append(end);
    return tokens;
}

}  // namespace

// --- Syntax tree -----------------------------------------------------------

struct Node {
    enum Type { Num, Ident, Call, Index, Binary, Unary, Postfix, ListLit, MatLit, Store, Seq };
    Type type = Num;
    Complex value;
    QString name;  // identifier, function name or operator symbol
    std::vector<NodePtr> kids;
};

namespace {

NodePtr makeNode(Node::Type type, const QString &name = QString()) {
    auto node = std::make_shared<Node>();
    node->type = type;
    node->name = name;
    return node;
}

bool isConstantName(const QString &name) {
    return name == QStringLiteral("Ans") || name == QStringLiteral("pi")
        || name == QStringLiteral("theta");
}

bool isFunctionName(const QString &name) {
    if (isConstantName(name))
        return false;
    if (name == QStringLiteral("and") || name == QStringLiteral("or")
        || name == QStringLiteral("xor") || name == QStringLiteral("nPr")
        || name == QStringLiteral("nCr"))
        return false;
    return nameTable().contains(name);
}

// The parser. Precedence follows the TI Equation Operating System: postfix
// operators, then powers, then nPr/nCr, then multiplication (including implied
// multiplication), then addition, relationals, and finally the logic operators.
class Parser {
public:
    explicit Parser(const QString &source) : m_tokens(tokenize(source)) {}

    NodePtr parseProgram() {
        auto seq = makeNode(Node::Seq);
        seq->kids.push_back(parseStatement());
        while (match(QStringLiteral(":"))) {
            if (peek().type == Token::End)
                break;
            seq->kids.push_back(parseStatement());
        }
        if (peek().type != Token::End)
            raise("SYNTAX");
        if (seq->kids.size() == 1)
            return seq->kids.front();
        return seq;
    }

private:
    const Token &peek(int ahead = 0) const {
        const int index = std::min(m_index + ahead, int(m_tokens.size()) - 1);
        return m_tokens.at(index);
    }

    bool checkOperator(const QString &text) const {
        return peek().type == Token::Operator && peek().text == text;
    }

    bool checkName(const QString &text) const {
        return peek().type == Token::Name && peek().text == text;
    }

    bool match(const QString &text) {
        if (checkOperator(text)) {
            ++m_index;
            return true;
        }
        return false;
    }

    bool matchName(const QString &text) {
        if (checkName(text)) {
            ++m_index;
            return true;
        }
        return false;
    }

    // A closing parenthesis may be left off at the end of a line, the way the
    // calculator closes them for you when you press ENTER.
    void expectClose() {
        if (match(QStringLiteral(")")))
            return;
        if (peek().type == Token::End || checkOperator(QStringLiteral(":")))
            return;
        raise("SYNTAX");
    }

    NodePtr parseStatement() {
        NodePtr value = parseLogicOr();
        if (match(QStringLiteral("→")) || match(QStringLiteral("->"))) {
            NodePtr target = parsePostfix();
            if (target->type != Node::Ident && target->type != Node::Index)
                raise("SYNTAX");
            auto store = makeNode(Node::Store);
            store->kids.push_back(value);
            store->kids.push_back(target);
            return store;
        }
        return value;
    }

    NodePtr parseLogicOr() {
        NodePtr left = parseLogicAnd();
        while (checkName(QStringLiteral("or")) || checkName(QStringLiteral("xor"))) {
            const QString op = peek().text;
            ++m_index;
            left = binary(op, left, parseLogicAnd());
        }
        return left;
    }

    NodePtr parseLogicAnd() {
        NodePtr left = parseRelational();
        while (checkName(QStringLiteral("and"))) {
            ++m_index;
            left = binary(QStringLiteral("and"), left, parseRelational());
        }
        return left;
    }

    NodePtr parseRelational() {
        NodePtr left = parseAdditive();
        while (true) {
            QString op;
            for (const QString &candidate :
                 {QStringLiteral("="), QStringLiteral("≠"), QStringLiteral("!="),
                  QStringLiteral("≤"), QStringLiteral("<="), QStringLiteral("≥"),
                  QStringLiteral(">="), QStringLiteral("<"), QStringLiteral(">")}) {
                if (checkOperator(candidate)) {
                    op = candidate;
                    break;
                }
            }
            if (op.isEmpty())
                return left;
            ++m_index;
            left = binary(op, left, parseAdditive());
        }
    }

    NodePtr parseAdditive() {
        NodePtr left = parseMultiplicative();
        while (true) {
            if (match(QStringLiteral("+")))
                left = binary(QStringLiteral("+"), left, parseMultiplicative());
            else if (match(QStringLiteral("-")) || match(QStringLiteral("−")))
                left = binary(QStringLiteral("-"), left, parseMultiplicative());
            else
                return left;
        }
    }

    // Implied multiplication sits at the same level as an explicit one, so
    // 1/2X reads as (1/2)X exactly as the calculator's manual describes.
    bool startsOperand() const {
        const Token &t = peek();
        if (t.type == Token::Number)
            return true;
        if (t.type == Token::Name) {
            static const QStringList keywords = {
                QStringLiteral("and"), QStringLiteral("or"),  QStringLiteral("xor"),
                QStringLiteral("nPr"), QStringLiteral("nCr")};
            return !keywords.contains(t.text);
        }
        if (t.type == Token::Operator)
            return t.text == QStringLiteral("(") || t.text == QStringLiteral("{")
                || t.text == QStringLiteral("[") || t.text == QStringLiteral("√")
                || t.text == QStringLiteral("∛") || t.text == QStringLiteral("π");
        return false;
    }

    NodePtr parseMultiplicative() {
        NodePtr left = parseCombinatoric();
        while (true) {
            if (match(QStringLiteral("*")) || match(QStringLiteral("×")))
                left = binary(QStringLiteral("*"), left, parseCombinatoric());
            else if (match(QStringLiteral("/")) || match(QStringLiteral("÷")))
                left = binary(QStringLiteral("/"), left, parseCombinatoric());
            else if (startsOperand())
                left = binary(QStringLiteral("*"), left, parseCombinatoric());
            else
                return left;
        }
    }

    NodePtr parseCombinatoric() {
        NodePtr left = parseUnary();
        while (checkName(QStringLiteral("nPr")) || checkName(QStringLiteral("nCr"))) {
            const QString op = peek().text;
            ++m_index;
            left = binary(op, left, parseUnary());
        }
        return left;
    }

    // Negation binds looser than a power, which is why -2² is -4.
    NodePtr parseUnary() {
        if (match(QStringLiteral("-")) || match(QStringLiteral("−"))) {
            auto node = makeNode(Node::Unary, QStringLiteral("-"));
            node->kids.push_back(parseUnary());
            return node;
        }
        return parsePower();
    }

    // Powers chain left to right, which is why 2^3^2 is 64 and not 512.
    NodePtr parseExponentOperand() {
        if (match(QStringLiteral("-")) || match(QStringLiteral("−"))) {
            auto node = makeNode(Node::Unary, QStringLiteral("-"));
            node->kids.push_back(parseExponentOperand());
            return node;
        }
        return parsePostfix();
    }

    NodePtr parsePower() {
        NodePtr left = parsePostfix();
        while (true) {
            if (match(QStringLiteral("^"))) {
                left = binary(QStringLiteral("^"), left, parseExponentOperand());
            } else if (match(QStringLiteral("ˣ√"))) {
                // x-th root: the left operand is the index, as on the keypad.
                left = binary(QStringLiteral("xroot"), left, parseExponentOperand());
            } else {
                return left;
            }
        }
    }

    NodePtr parsePostfix() {
        NodePtr value = parsePrimary();
        while (true) {
            static const QStringList postfixOperators = {
                QStringLiteral("²"),     QStringLiteral("³"),    QStringLiteral("⁻¹"),
                QStringLiteral("^-1"),   QStringLiteral("!"),    QStringLiteral("%"),
                QStringLiteral("°"),     QStringLiteral("ʳ"),    QStringLiteral("ᵀ"),
                QStringLiteral("▶Frac"), QStringLiteral("▶Dec"), QStringLiteral("▶Rect"),
                QStringLiteral("▶Polar"), QStringLiteral("▶DMS"),
            };
            QString op;
            for (const QString &candidate : postfixOperators) {
                if (checkOperator(candidate)) {
                    op = candidate;
                    break;
                }
            }
            if (!op.isEmpty()) {
                ++m_index;
                auto node = makeNode(Node::Postfix, op);
                node->kids.push_back(value);
                value = node;
                continue;
            }

            // Y1(2), L1(3) and [A](1,2) index or call the value on the left.
            if ((value->type == Node::Ident || value->type == Node::Index)
                && checkOperator(QStringLiteral("("))) {
                ++m_index;
                auto node = makeNode(Node::Index, value->name);
                node->kids.push_back(value);
                if (!checkOperator(QStringLiteral(")"))) {
                    node->kids.push_back(parseStatement());
                    while (match(QStringLiteral(",")))
                        node->kids.push_back(parseStatement());
                }
                expectClose();
                value = node;
                continue;
            }
            return value;
        }
    }

    NodePtr parsePrimary() {
        const Token &t = peek();

        if (t.type == Token::Number) {
            ++m_index;
            auto node = makeNode(Node::Num);
            node->value = Complex(t.number, 0);
            return node;
        }

        if (t.type == Token::Operator) {
            if (t.text == QStringLiteral("(")) {
                ++m_index;
                NodePtr inner = parseStatement();
                // A parenthesised pair is also how a complex number in polar
                // form arrives from ▶Polar; commas make it a list of results.
                expectClose();
                return inner;
            }
            if (t.text == QStringLiteral("{")) {
                ++m_index;
                auto node = makeNode(Node::ListLit);
                if (!checkOperator(QStringLiteral("}"))) {
                    node->kids.push_back(parseStatement());
                    while (match(QStringLiteral(",")))
                        node->kids.push_back(parseStatement());
                }
                if (!match(QStringLiteral("}")) && peek().type != Token::End)
                    raise("SYNTAX");
                return node;
            }
            if (t.text == QStringLiteral("[")) {
                ++m_index;
                auto node = makeNode(Node::MatLit);
                while (true) {
                    match(QStringLiteral(","));  // rows may be comma separated
                    if (!match(QStringLiteral("[")))
                        break;
                    auto row = makeNode(Node::ListLit);
                    if (!checkOperator(QStringLiteral("]"))) {
                        row->kids.push_back(parseStatement());
                        while (match(QStringLiteral(",")))
                            row->kids.push_back(parseStatement());
                    }
                    if (!match(QStringLiteral("]")) && peek().type != Token::End)
                        raise("SYNTAX");
                    node->kids.push_back(row);
                }
                if (!match(QStringLiteral("]")) && peek().type != Token::End)
                    raise("SYNTAX");
                if (node->kids.empty())
                    raise("SYNTAX");
                return node;
            }
            if (t.text == QStringLiteral("√") || t.text == QStringLiteral("∛")) {
                ++m_index;
                auto node = makeNode(Node::Call, t.text == QStringLiteral("√")
                                         ? QStringLiteral("sqrt")
                                         : QStringLiteral("cbrt"));
                node->kids.push_back(parseFunctionArgument());
                return node;
            }
            if (t.text == QStringLiteral("π")) {
                ++m_index;
                return makeNode(Node::Ident, QStringLiteral("pi"));
            }
            if (t.text == QStringLiteral("-") || t.text == QStringLiteral("−")) {
                return parseUnary();
            }
        }

        if (t.type == Token::Name) {
            const QString name = t.text;
            static const QStringList keywords = {
                QStringLiteral("and"), QStringLiteral("or"),  QStringLiteral("xor"),
                QStringLiteral("nPr"), QStringLiteral("nCr")};
            if (keywords.contains(name))
                raise("SYNTAX");
            ++m_index;
            if (isFunctionName(name)) {
                auto node = makeNode(Node::Call, name);
                if (match(QStringLiteral("("))) {
                    if (!checkOperator(QStringLiteral(")"))) {
                        node->kids.push_back(parseStatement());
                        while (match(QStringLiteral(",")))
                            node->kids.push_back(parseStatement());
                    }
                    expectClose();
                } else if (name != QStringLiteral("rand")) {
                    node->kids.push_back(parseFunctionArgument());
                }
                return node;
            }
            return makeNode(Node::Ident, name);
        }

        raise("SYNTAX");
    }

    // A prefix function written without its parenthesis takes the tightest
    // operand available, so √9+1 is 4 rather than the square root of 10.
    NodePtr parseFunctionArgument() { return parsePostfix(); }

    NodePtr binary(const QString &op, NodePtr left, NodePtr right) {
        auto node = makeNode(Node::Binary, op);
        node->kids.push_back(std::move(left));
        node->kids.push_back(std::move(right));
        return node;
    }

    QList<Token> m_tokens;
    int m_index = 0;
};

}  // namespace

NodePtr parse(const QString &source) {
    if (source.trimmed().isEmpty())
        raise("SYNTAX");
    Parser parser(source);
    return parser.parseProgram();
}

// --- Arithmetic ------------------------------------------------------------

namespace {

void requireComplexAllowed(const Complex &value, const Settings &settings) {
    if (!isReal(value) && settings.complexMode == ComplexMode::Real)
        raise("NONREAL ANS");
}

Complex checkedResult(const Complex &value, const Settings &settings) {
    if (std::isnan(value.real()) || std::isnan(value.imag()))
        raise("DOMAIN");
    if (std::isinf(value.real()) || std::isinf(value.imag()))
        raise("OVERFLOW");
    requireComplexAllowed(value, settings);
    return value;
}

double factorialOf(double n) {
    // The calculator accepts halves as well as whole numbers; both go through
    // the gamma function, with small integers computed exactly.
    if (n < -0.5)
        raise("DOMAIN");
    const double twice = n * 2.0;
    if (std::abs(twice - std::round(twice)) > 1e-9)
        raise("DOMAIN");
    long long whole = 0;
    if (wholeNumber(n, &whole) && whole >= 0 && whole <= 170) {
        double product = 1.0;
        for (long long i = 2; i <= whole; ++i)
            product *= double(i);
        return product;
    }
    return std::exp(std::lgamma(n + 1.0));
}

double permutations(double n, double r) {
    long long ni = 0, ri = 0;
    if (!wholeNumber(n, &ni) || !wholeNumber(r, &ri) || ni < 0 || ri < 0 || ri > ni)
        raise("DOMAIN");
    double product = 1.0;
    for (long long i = 0; i < ri; ++i)
        product *= double(ni - i);
    return product;
}

double combinations(double n, double r) {
    long long ni = 0, ri = 0;
    if (!wholeNumber(n, &ni) || !wholeNumber(r, &ri) || ni < 0 || ri < 0 || ri > ni)
        raise("DOMAIN");
    if (ri > ni - ri)
        ri = ni - ri;
    double result = 1.0;
    for (long long i = 1; i <= ri; ++i)
        result = result * double(ni - ri + i) / double(i);
    return std::round(result);
}

Complex powerOf(Complex base, Complex exponent, const Settings &settings) {
    if (isReal(base) && isReal(exponent)) {
        const double b = base.real();
        const double e = exponent.real();
        if (b == 0.0 && e < 0.0)
            raise("DIVIDE BY 0");
        if (b == 0.0)
            return Complex(e == 0.0 ? 1.0 : 0.0, 0.0);
        long long wholeExponent = 0;
        if (b > 0.0 || wholeNumber(e, &wholeExponent))
            return Complex(std::pow(b, e), 0.0);
        // A negative base raised to an odd reciprocal is the real root the
        // calculator shows for cube roots and the like.
        const double inverse = 1.0 / e;
        long long root = 0;
        if (wholeNumber(inverse, &root) && root % 2 != 0)
            return Complex(-std::pow(-b, e), 0.0);
        if (settings.complexMode == ComplexMode::Real)
            raise("NONREAL ANS");
    }
    long long repeats = 0;
    if (isReal(exponent) && wholeNumber(exponent.real(), &repeats) && std::abs(repeats) <= 64) {
        // Repeated multiplication keeps i² at exactly -1, where exp(n·log z)
        // would leave a speck of imaginary part behind.
        Complex result(1, 0);
        const Complex factor = repeats < 0 ? Complex(1, 0) / base : base;
        for (long long i = 0; i < std::abs(repeats); ++i)
            result *= factor;
        return result;
    }
    return std::pow(base, exponent);
}

Complex numericBinary(const QString &op, const Complex &a, const Complex &b,
                      const Settings &settings) {
    if (op == QStringLiteral("+"))
        return checkedResult(a + b, settings);
    if (op == QStringLiteral("-"))
        return checkedResult(a - b, settings);
    if (op == QStringLiteral("*"))
        return checkedResult(a * b, settings);
    if (op == QStringLiteral("/")) {
        if (b == Complex(0, 0))
            raise("DIVIDE BY 0");
        return checkedResult(a / b, settings);
    }
    if (op == QStringLiteral("^"))
        return checkedResult(powerOf(a, b, settings), settings);
    if (op == QStringLiteral("xroot")) {
        const Complex index = a;
        if (index == Complex(0, 0))
            raise("DOMAIN");
        return checkedResult(powerOf(b, Complex(1, 0) / index, settings), settings);
    }
    if (op == QStringLiteral("nPr"))
        return Complex(permutations(requireReal(a), requireReal(b)), 0);
    if (op == QStringLiteral("nCr"))
        return Complex(combinations(requireReal(a), requireReal(b)), 0);

    const double left = requireReal(a);
    const double right = requireReal(b);
    if (op == QStringLiteral("="))
        return Complex(left == right ? 1 : 0, 0);
    if (op == QStringLiteral("≠") || op == QStringLiteral("!="))
        return Complex(left != right ? 1 : 0, 0);
    if (op == QStringLiteral("<"))
        return Complex(left < right ? 1 : 0, 0);
    if (op == QStringLiteral(">"))
        return Complex(left > right ? 1 : 0, 0);
    if (op == QStringLiteral("≤") || op == QStringLiteral("<="))
        return Complex(left <= right ? 1 : 0, 0);
    if (op == QStringLiteral("≥") || op == QStringLiteral(">="))
        return Complex(left >= right ? 1 : 0, 0);
    if (op == QStringLiteral("and"))
        return Complex((left != 0 && right != 0) ? 1 : 0, 0);
    if (op == QStringLiteral("or"))
        return Complex((left != 0 || right != 0) ? 1 : 0, 0);
    if (op == QStringLiteral("xor"))
        return Complex(((left != 0) != (right != 0)) ? 1 : 0, 0);
    raise("SYNTAX");
}

// --- Matrices --------------------------------------------------------------

Matrix matrixTranspose(const Matrix &m) {
    Matrix out(m.cols, m.rows);
    for (int r = 0; r < m.rows; ++r)
        for (int c = 0; c < m.cols; ++c)
            out.at(c, r) = m.at(r, c);
    return out;
}

Matrix matrixIdentity(int n) {
    Matrix out(n, n);
    for (int i = 0; i < n; ++i)
        out.at(i, i) = Complex(1, 0);
    return out;
}

Matrix matrixMultiply(const Matrix &a, const Matrix &b) {
    if (a.cols != b.rows)
        raise("DIM MISMATCH");
    Matrix out(a.rows, b.cols);
    for (int r = 0; r < a.rows; ++r) {
        for (int c = 0; c < b.cols; ++c) {
            Complex sum(0, 0);
            for (int k = 0; k < a.cols; ++k)
                sum += a.at(r, k) * b.at(k, c);
            out.at(r, c) = sum;
        }
    }
    return out;
}

Matrix matrixCombine(const Matrix &a, const Matrix &b, bool subtract) {
    if (a.rows != b.rows || a.cols != b.cols)
        raise("DIM MISMATCH");
    Matrix out(a.rows, a.cols);
    for (int i = 0; i < int(a.cells.size()); ++i)
        out.cells[i] = subtract ? a.cells[i] - b.cells[i] : a.cells[i] + b.cells[i];
    return out;
}

Matrix matrixScale(const Matrix &a, const Complex &factor) {
    Matrix out = a;
    for (Complex &cell : out.cells)
        cell *= factor;
    return out;
}

// Gauss-Jordan with partial pivoting; `reduced` decides between ref and rref.
Matrix matrixRowReduce(Matrix m, bool reduced) {
    int pivotRow = 0;
    for (int col = 0; col < m.cols && pivotRow < m.rows; ++col) {
        int best = -1;
        double bestMagnitude = 1e-12;
        for (int r = pivotRow; r < m.rows; ++r) {
            const double magnitude = std::abs(m.at(r, col));
            if (magnitude > bestMagnitude) {
                bestMagnitude = magnitude;
                best = r;
            }
        }
        if (best < 0)
            continue;
        for (int c = 0; c < m.cols; ++c)
            std::swap(m.at(pivotRow, c), m.at(best, c));

        const Complex pivot = m.at(pivotRow, col);
        for (int c = 0; c < m.cols; ++c)
            m.at(pivotRow, c) /= pivot;

        for (int r = 0; r < m.rows; ++r) {
            if (r == pivotRow)
                continue;
            if (!reduced && r < pivotRow)
                continue;
            const Complex factor = m.at(r, col);
            if (factor == Complex(0, 0))
                continue;
            for (int c = 0; c < m.cols; ++c)
                m.at(r, c) -= factor * m.at(pivotRow, c);
        }
        ++pivotRow;
    }
    return m;
}

Complex matrixDeterminant(Matrix m) {
    if (m.rows != m.cols)
        raise("INVALID DIM");
    Complex determinant(1, 0);
    for (int col = 0; col < m.cols; ++col) {
        int best = -1;
        double bestMagnitude = 1e-14;
        for (int r = col; r < m.rows; ++r) {
            const double magnitude = std::abs(m.at(r, col));
            if (magnitude > bestMagnitude) {
                bestMagnitude = magnitude;
                best = r;
            }
        }
        if (best < 0)
            return Complex(0, 0);
        if (best != col) {
            for (int c = 0; c < m.cols; ++c)
                std::swap(m.at(col, c), m.at(best, c));
            determinant = -determinant;
        }
        determinant *= m.at(col, col);
        const Complex pivot = m.at(col, col);
        for (int r = col + 1; r < m.rows; ++r) {
            const Complex factor = m.at(r, col) / pivot;
            for (int c = col; c < m.cols; ++c)
                m.at(r, c) -= factor * m.at(col, c);
        }
    }
    return determinant;
}

Matrix matrixInverse(const Matrix &m) {
    if (m.rows != m.cols)
        raise("INVALID DIM");
    const int n = m.rows;
    Matrix work(n, 2 * n);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c)
            work.at(r, c) = m.at(r, c);
        work.at(r, n + r) = Complex(1, 0);
    }
    work = matrixRowReduce(work, true);
    for (int r = 0; r < n; ++r) {
        if (std::abs(work.at(r, r) - Complex(1, 0)) > 1e-9)
            raise("SINGULAR MAT");
    }
    Matrix out(n, n);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            out.at(r, c) = work.at(r, n + c);
    return out;
}

Matrix matrixAugment(const Matrix &a, const Matrix &b) {
    if (a.rows != b.rows)
        raise("DIM MISMATCH");
    Matrix out(a.rows, a.cols + b.cols);
    for (int r = 0; r < a.rows; ++r) {
        for (int c = 0; c < a.cols; ++c)
            out.at(r, c) = a.at(r, c);
        for (int c = 0; c < b.cols; ++c)
            out.at(r, a.cols + c) = b.at(r, c);
    }
    return out;
}

Matrix matrixPower(const Matrix &m, double exponent) {
    long long whole = 0;
    if (!wholeNumber(exponent, &whole))
        raise("DOMAIN");
    if (m.rows != m.cols)
        raise("INVALID DIM");
    Matrix base = whole < 0 ? matrixInverse(m) : m;
    long long count = std::abs(whole);
    Matrix result = matrixIdentity(m.rows);
    for (long long i = 0; i < count; ++i)
        result = matrixMultiply(result, base);
    return result;
}

// --- Value-level operators -------------------------------------------------

Value binaryValue(const QString &op, const Value &a, const Value &b, const Settings &settings) {
    if (a.isMatrix() || b.isMatrix()) {
        if (a.isMatrix() && b.isMatrix()) {
            if (op == QStringLiteral("+"))
                return Value::fromMatrix(matrixCombine(a.matrix(), b.matrix(), false));
            if (op == QStringLiteral("-"))
                return Value::fromMatrix(matrixCombine(a.matrix(), b.matrix(), true));
            if (op == QStringLiteral("*"))
                return Value::fromMatrix(matrixMultiply(a.matrix(), b.matrix()));
            raise("DATA TYPE");
        }
        if (a.isMatrix() && b.isNumber()) {
            if (op == QStringLiteral("*"))
                return Value::fromMatrix(matrixScale(a.matrix(), b.number()));
            if (op == QStringLiteral("/")) {
                if (b.number() == Complex(0, 0))
                    raise("DIVIDE BY 0");
                return Value::fromMatrix(matrixScale(a.matrix(), Complex(1, 0) / b.number()));
            }
            if (op == QStringLiteral("^"))
                return Value::fromMatrix(matrixPower(a.matrix(), b.real()));
            raise("DATA TYPE");
        }
        if (a.isNumber() && b.isMatrix() && op == QStringLiteral("*"))
            return Value::fromMatrix(matrixScale(b.matrix(), a.number()));
        raise("DATA TYPE");
    }

    if (a.isList() || b.isList()) {
        const bool bothLists = a.isList() && b.isList();
        if (bothLists && a.list().size() != b.list().size())
            raise("DIM MISMATCH");
        const size_t count = a.isList() ? a.list().size() : b.list().size();
        std::vector<Complex> out;
        out.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const Complex left = a.isList() ? a.list()[i] : a.number();
            const Complex right = b.isList() ? b.list()[i] : b.number();
            out.push_back(numericBinary(op, left, right, settings));
        }
        return Value::fromList(std::move(out));
    }

    return Value(numericBinary(op, a.number(), b.number(), settings));
}

Value mapValue(const Value &value, const std::function<Complex(const Complex &)> &fn) {
    if (value.isNumber())
        return Value(fn(value.number()));
    if (value.isList()) {
        std::vector<Complex> out;
        out.reserve(value.list().size());
        for (const Complex &item : value.list())
            out.push_back(fn(item));
        return Value::fromList(std::move(out));
    }
    Matrix out = value.matrix();
    for (Complex &cell : out.cells)
        cell = fn(cell);
    return Value::fromMatrix(std::move(out));
}

}  // namespace

// --- Evaluation ------------------------------------------------------------

namespace {

const QString kAns = QStringLiteral("Ans");

bool isListName(const QString &name) {
    return name.size() == 2 && name.at(0) == u'L' && name.at(1).isDigit();
}

bool isMatrixName(const QString &name) {
    return name.size() == 3 && name.at(0) == u'[' && name.at(2) == u']';
}

bool isDefinitionName(const QString &name) {
    if (name.size() >= 2 && (name.at(0) == u'Y' || name.at(0) == u'r' || name.at(0) == u'X')
        && name.at(1).isDigit())
        return true;
    return false;
}

std::vector<double> realList(const Value &value) {
    std::vector<double> out;
    if (value.isNumber()) {
        out.push_back(requireReal(value.number()));
        return out;
    }
    for (const Complex &item : value.list())
        out.push_back(requireReal(item));
    return out;
}

std::vector<Complex> asList(const Value &value) {
    if (value.isNumber())
        return {value.number()};
    return value.list();
}

class Evaluator {
public:
    explicit Evaluator(Context &context) : m_context(context) {}

    Value eval(const NodePtr &node) {
        if (!node)
            raise("SYNTAX");
        switch (node->type) {
        case Node::Num:
            return Value(node->value);
        case Node::Seq: {
            Value last;
            for (const NodePtr &kid : node->kids)
                last = eval(kid);
            return last;
        }
        case Node::Ident:
            return evalIdentifier(node->name);
        case Node::ListLit: {
            std::vector<Complex> items;
            items.reserve(node->kids.size());
            for (const NodePtr &kid : node->kids)
                items.push_back(eval(kid).number());
            return Value::fromList(std::move(items));
        }
        case Node::MatLit:
            return evalMatrixLiteral(node);
        case Node::Unary: {
            const Value inner = eval(node->kids.at(0));
            return binaryValue(QStringLiteral("*"), Value(Complex(-1, 0)), inner,
                               m_context.settings);
        }
        case Node::Binary:
            return binaryValue(node->name, eval(node->kids.at(0)), eval(node->kids.at(1)),
                               m_context.settings);
        case Node::Postfix:
            return evalPostfix(node);
        case Node::Call:
            return evalCall(node);
        case Node::Index:
            return evalIndex(node);
        case Node::Store:
            return evalStore(node);
        }
        raise("SYNTAX");
    }

private:
    Complex toRadiansComplex(const Complex &value) const {
        if (m_context.settings.angle == AngleMode::Degree)
            return value * Complex(kPi / 180.0, 0);
        return value;
    }

    Complex fromRadiansComplex(const Complex &value) const {
        if (m_context.settings.angle == AngleMode::Degree)
            return value * Complex(180.0 / kPi, 0);
        return value;
    }

    Value evalMatrixLiteral(const NodePtr &node) {
        const int rows = int(node->kids.size());
        const int cols = int(node->kids.at(0)->kids.size());
        if (cols == 0)
            raise("INVALID DIM");
        Matrix matrix(rows, cols);
        for (int r = 0; r < rows; ++r) {
            const NodePtr &row = node->kids.at(size_t(r));
            if (int(row->kids.size()) != cols)
                raise("DIM MISMATCH");
            for (int c = 0; c < cols; ++c)
                matrix.at(r, c) = eval(row->kids.at(size_t(c))).number();
        }
        return Value::fromMatrix(std::move(matrix));
    }

    Value evalIdentifier(const QString &name) {
        if (name == QStringLiteral("pi") || name == QStringLiteral("π"))
            return Value(Complex(kPi, 0));
        if (name == QStringLiteral("e"))
            return Value(Complex(kE, 0));
        if (name == QStringLiteral("i")) {
            if (m_context.settings.complexMode == ComplexMode::Real)
                raise("NONREAL ANS");
            return Value(Complex(0, 1));
        }
        if (name == kAns)
            return m_context.has(kAns) ? m_context.value(kAns) : Value(Complex(0, 0));
        if (name == QStringLiteral("theta"))
            return evalIdentifier(QStringLiteral("θ"));

        if (isDefinitionName(name) && m_context.functions.contains(name)) {
            // A bare Y1 on the home screen is Y1 evaluated at the current X.
            const Value x = m_context.has(QStringLiteral("X"))
                ? m_context.value(QStringLiteral("X"))
                : Value(Complex(0, 0));
            return m_context.callDefinition(name, x);
        }

        if (m_context.has(name))
            return m_context.value(name);
        if (isListName(name) || isMatrixName(name) || isDefinitionName(name))
            raise("UNDEFINED");
        return Value(Complex(0, 0));  // Unset letters read as zero, as they do on the calculator.
    }

    Value evalPostfix(const NodePtr &node) {
        const QString &op = node->name;
        if (op == QStringLiteral("▶Frac")) {
            m_context.displayHint = DisplayHint::Fraction;
            return eval(node->kids.at(0));
        }
        if (op == QStringLiteral("▶Dec")) {
            m_context.displayHint = DisplayHint::Decimal;
            return eval(node->kids.at(0));
        }
        if (op == QStringLiteral("▶Rect")) {
            m_context.displayHint = DisplayHint::Rectangular;
            return eval(node->kids.at(0));
        }
        if (op == QStringLiteral("▶Polar")) {
            m_context.displayHint = DisplayHint::Polar;
            return eval(node->kids.at(0));
        }
        if (op == QStringLiteral("▶DMS")) {
            m_context.displayHint = DisplayHint::DegreeMinuteSecond;
            return eval(node->kids.at(0));
        }

        const Value value = eval(node->kids.at(0));
        const Settings &settings = m_context.settings;

        if (op == QStringLiteral("ᵀ"))
            return Value::fromMatrix(matrixTranspose(value.matrix()));
        if (op == QStringLiteral("²"))
            return binaryValue(QStringLiteral("^"), value, Value(Complex(2, 0)), settings);
        if (op == QStringLiteral("³"))
            return binaryValue(QStringLiteral("^"), value, Value(Complex(3, 0)), settings);
        if (op == QStringLiteral("⁻¹") || op == QStringLiteral("^-1")) {
            if (value.isMatrix())
                return Value::fromMatrix(matrixInverse(value.matrix()));
            return binaryValue(QStringLiteral("/"), Value(Complex(1, 0)), value, settings);
        }
        if (op == QStringLiteral("!"))
            return mapValue(value, [](const Complex &c) {
                return Complex(factorialOf(requireReal(c)), 0);
            });
        if (op == QStringLiteral("%"))
            return binaryValue(QStringLiteral("/"), value, Value(Complex(100, 0)), settings);
        if (op == QStringLiteral("°")) {
            // Marks the number as degrees whatever the angle mode is.
            const double factor = settings.angle == AngleMode::Degree ? 1.0 : kPi / 180.0;
            return binaryValue(QStringLiteral("*"), value, Value(Complex(factor, 0)), settings);
        }
        if (op == QStringLiteral("ʳ")) {
            const double factor = settings.angle == AngleMode::Degree ? 180.0 / kPi : 1.0;
            return binaryValue(QStringLiteral("*"), value, Value(Complex(factor, 0)), settings);
        }
        raise("SYNTAX");
    }

    QString variableNameOf(const NodePtr &node) {
        if (!node || node->type != Node::Ident)
            raise("SYNTAX");
        return node->name;
    }

    // Binds `name` to each sampled value while the callback runs, then puts the
    // old value back, so nDeriv and friends never disturb the user's variables.
    std::function<double(double)> boundFunction(const NodePtr &body, const QString &name) {
        return [this, body, name](double x) {
            const bool had = m_context.has(name);
            const Value previous = had ? m_context.value(name) : Value();
            m_context.store(name, Value(Complex(x, 0)));
            double result = 0;
            try {
                result = requireReal(eval(body).number());
            } catch (...) {
                if (had)
                    m_context.store(name, previous);
                else
                    m_context.variables.remove(name);
                throw;
            }
            if (had)
                m_context.store(name, previous);
            else
                m_context.variables.remove(name);
            return result;
        };
    }

    Value evalCall(const NodePtr &node);
    Value callBuiltin(const QString &name, std::vector<Value> &args);

    Value evalIndex(const NodePtr &node) {
        const NodePtr &target = node->kids.at(0);
        std::vector<Value> args;
        for (size_t i = 1; i < node->kids.size(); ++i)
            args.push_back(eval(node->kids.at(i)));

        if (target->type == Node::Ident) {
            const QString name = target->name;
            if (isDefinitionName(name) && m_context.functions.contains(name)) {
                if (args.size() != 1)
                    raise("ARGUMENT");
                return m_context.callDefinition(name, args.at(0));
            }
        }

        const Value base = eval(target);
        if (base.isList()) {
            if (args.size() != 1)
                raise("ARGUMENT");
            const std::vector<Complex> &items = base.list();
            const double index = args.at(0).real();
            const int i = int(std::round(index));
            if (i < 1 || i > int(items.size()))
                raise("INVALID DIM");
            return Value(items[size_t(i - 1)]);
        }
        if (base.isMatrix()) {
            if (args.size() != 2)
                raise("ARGUMENT");
            const Matrix &m = base.matrix();
            const int r = int(std::round(args.at(0).real()));
            const int c = int(std::round(args.at(1).real()));
            if (r < 1 || r > m.rows || c < 1 || c > m.cols)
                raise("INVALID DIM");
            return Value(m.at(r - 1, c - 1));
        }
        // A number followed by a parenthesis is implied multiplication.
        if (args.size() != 1)
            raise("SYNTAX");
        return binaryValue(QStringLiteral("*"), base, args.at(0), m_context.settings);
    }

    Value evalStore(const NodePtr &node) {
        const Value value = eval(node->kids.at(0));
        const NodePtr &target = node->kids.at(1);
        if (target->type == Node::Ident) {
            m_context.store(target->name, value);
            return value;
        }
        // Storing into one element: L1(3)→7 or [A](1,2)→5.
        const NodePtr &container = target->kids.at(0);
        const QString name = variableNameOf(container);
        if (!m_context.has(name))
            raise("UNDEFINED");
        Value existing = m_context.value(name);
        if (existing.isList()) {
            if (target->kids.size() != 2)
                raise("ARGUMENT");
            std::vector<Complex> items = existing.list();
            const int i = int(std::round(eval(target->kids.at(1)).real()));
            if (i < 1 || i > int(items.size()) + 1)
                raise("INVALID DIM");
            if (i == int(items.size()) + 1)
                items.push_back(value.number());
            else
                items[size_t(i - 1)] = value.number();
            m_context.store(name, Value::fromList(std::move(items)));
            return value;
        }
        if (existing.isMatrix()) {
            if (target->kids.size() != 3)
                raise("ARGUMENT");
            Matrix m = existing.matrix();
            const int r = int(std::round(eval(target->kids.at(1)).real()));
            const int c = int(std::round(eval(target->kids.at(2)).real()));
            if (r < 1 || r > m.rows || c < 1 || c > m.cols)
                raise("INVALID DIM");
            m.at(r - 1, c - 1) = value.number();
            m_context.store(name, Value::fromMatrix(std::move(m)));
            return value;
        }
        raise("DATA TYPE");
    }

    Context &m_context;
};

}  // namespace

// --- Numeric routines behind nDeriv, fnInt, solve and the CALC menu ---------

namespace {

using RealFunction = std::function<double(double)>;

double centralDerivative(const RealFunction &f, double x, double h) {
    if (h <= 0)
        h = 1e-5 * std::max(1.0, std::abs(x));
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

double simpsonSegment(const RealFunction &f, double a, double b, double fa, double fm, double fb) {
    return (b - a) / 6.0 * (fa + 4.0 * fm + fb);
}

double adaptiveSimpson(const RealFunction &f, double a, double b, double fa, double fm, double fb,
                       double whole, double tolerance, int depth) {
    const double m = 0.5 * (a + b);
    const double leftMiddle = f(0.5 * (a + m));
    const double rightMiddle = f(0.5 * (m + b));
    const double left = simpsonSegment(f, a, m, fa, leftMiddle, fm);
    const double right = simpsonSegment(f, m, b, fm, rightMiddle, fb);
    if (depth <= 0 || std::abs(left + right - whole) <= 15.0 * tolerance)
        return left + right + (left + right - whole) / 15.0;
    return adaptiveSimpson(f, a, m, fa, leftMiddle, fm, left, tolerance / 2.0, depth - 1)
        + adaptiveSimpson(f, m, b, fm, rightMiddle, fb, right, tolerance / 2.0, depth - 1);
}

double integrate(const RealFunction &f, double a, double b) {
    if (a == b)
        return 0.0;
    const double sign = a < b ? 1.0 : -1.0;
    const double low = std::min(a, b);
    const double high = std::max(a, b);
    const double fa = f(low);
    const double fb = f(high);
    const double fm = f(0.5 * (low + high));
    const double whole = simpsonSegment(f, low, high, fa, fm, fb);
    return sign * adaptiveSimpson(f, low, high, fa, fm, fb, whole, 1e-9, 22);
}

// Bisection: slower than Newton but it cannot run away, which matters for the
// zero, intersect and solve routines that a user aims by eye.
bool bisect(const RealFunction &f, double low, double high, double *root) {
    double fLow = f(low);
    double fHigh = f(high);
    if (!std::isfinite(fLow) || !std::isfinite(fHigh))
        return false;
    if (fLow == 0.0) {
        *root = low;
        return true;
    }
    if (fHigh == 0.0) {
        *root = high;
        return true;
    }
    if ((fLow > 0) == (fHigh > 0))
        return false;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (low + high);
        const double fMid = f(mid);
        if (fMid == 0.0 || (high - low) < 1e-13 * std::max(1.0, std::abs(mid))) {
            *root = mid;
            return true;
        }
        if ((fMid > 0) == (fLow > 0)) {
            low = mid;
            fLow = fMid;
        } else {
            high = mid;
        }
    }
    *root = 0.5 * (low + high);
    return true;
}

// Widens a window around a guess until the function changes sign.
bool solveFromGuess(const RealFunction &f, double guess, double *root) {
    double step = std::max(1e-4, std::abs(guess) * 1e-3);
    for (int i = 0; i < 200; ++i) {
        const double low = guess - step;
        const double high = guess + step;
        if (bisect(f, low, guess, root) || bisect(f, guess, high, root))
            return true;
        step *= 1.6;
        if (step > 1e12)
            break;
    }
    return false;
}

double goldenSection(const RealFunction &f, double low, double high, bool maximum) {
    const double ratio = 0.6180339887498949;
    double a = low;
    double b = high;
    double c = b - ratio * (b - a);
    double d = a + ratio * (b - a);
    auto value = [&](double x) { return maximum ? -f(x) : f(x); };
    double fc = value(c);
    double fd = value(d);
    for (int i = 0; i < 200 && std::abs(b - a) > 1e-11 * std::max(1.0, std::abs(b)); ++i) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - ratio * (b - a);
            fc = value(c);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + ratio * (b - a);
            fd = value(d);
        }
    }
    return 0.5 * (a + b);
}

double randomUniform() { return QRandomGenerator::global()->generateDouble(); }

double randomNormal(double mean, double deviation) {
    // Box-Muller, guarding against a zero draw feeding the logarithm.
    double u1 = randomUniform();
    if (u1 <= 0)
        u1 = 1e-12;
    const double u2 = randomUniform();
    return mean + deviation * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

Value Evaluator_makeList(std::vector<Complex> items) { return Value::fromList(std::move(items)); }

Value Evaluator::evalCall(const NodePtr &node) {
    const QString &name = node->name;
    const size_t count = node->kids.size();

    // These take an expression rather than a value, so the argument stays
    // unevaluated until the routine samples it.
    static const QStringList lazyNames = {QStringLiteral("nDeriv"), QStringLiteral("fnInt"),
                                          QStringLiteral("fMin"),   QStringLiteral("fMax"),
                                          QStringLiteral("solve"),  QStringLiteral("seq")};
    if (lazyNames.contains(name)) {
        if (count < 3)
            raise("ARGUMENT");
        const NodePtr body = node->kids.at(0);
        const QString variable = variableNameOf(node->kids.at(1));
        const RealFunction f = boundFunction(body, variable);

        if (name == QStringLiteral("nDeriv")) {
            const double x = eval(node->kids.at(2)).real();
            const double h = count > 3 ? eval(node->kids.at(3)).real() : 0.0;
            return Value(Complex(centralDerivative(f, x, h), 0));
        }
        if (name == QStringLiteral("fnInt")) {
            if (count < 4)
                raise("ARGUMENT");
            const double a = eval(node->kids.at(2)).real();
            const double b = eval(node->kids.at(3)).real();
            return Value(Complex(integrate(f, a, b), 0));
        }
        if (name == QStringLiteral("fMin") || name == QStringLiteral("fMax")) {
            if (count < 4)
                raise("ARGUMENT");
            const double a = eval(node->kids.at(2)).real();
            const double b = eval(node->kids.at(3)).real();
            return Value(Complex(goldenSection(f, a, b, name == QStringLiteral("fMax")), 0));
        }
        if (name == QStringLiteral("solve")) {
            const double guess = eval(node->kids.at(2)).real();
            double root = 0;
            bool found = false;
            if (count >= 5) {
                const double low = eval(node->kids.at(3)).real();
                const double high = eval(node->kids.at(4)).real();
                found = bisect(f, low, high, &root);
            } else {
                found = solveFromGuess(f, guess, &root);
            }
            if (!found)
                raise("NO SIGN CHNG");
            return Value(Complex(root, 0));
        }
        // seq(expression, variable, start, end[, step])
        if (count < 4)
            raise("ARGUMENT");
        const double start = eval(node->kids.at(2)).real();
        const double end = eval(node->kids.at(3)).real();
        double step = count > 4 ? eval(node->kids.at(4)).real() : 1.0;
        if (step == 0)
            raise("ARGUMENT");
        if ((end - start) / step < 0)
            step = -step;
        std::vector<Complex> items;
        for (double x = start; (step > 0 ? x <= end + 1e-12 : x >= end - 1e-12); x += step) {
            items.push_back(Complex(f(x), 0));
            if (items.size() > 999)
                break;
        }
        return Evaluator_makeList(std::move(items));
    }

    std::vector<Value> args;
    args.reserve(count);
    for (const NodePtr &kid : node->kids)
        args.push_back(eval(kid));
    return callBuiltin(name, args);
}

Value Evaluator::callBuiltin(const QString &name, std::vector<Value> &args) {
    const Settings &settings = m_context.settings;
    const int count = int(args.size());

    auto need = [&](int wanted) {
        if (count != wanted)
            raise("ARGUMENT");
    };
    auto needBetween = [&](int low, double high) {
        if (count < low || count > high)
            raise("ARGUMENT");
    };
    auto first = [&]() -> const Value & { return args.at(0); };
    auto unary = [&](const std::function<Complex(const Complex &)> &fn) {
        need(1);
        return mapValue(first(), fn);
    };
    auto realArg = [&](int index) { return args.at(size_t(index)).real(); };

    // Trigonometry, respecting the angle mode both ways.
    if (name == QStringLiteral("sin"))
        return unary([&](const Complex &x) {
            return checkedResult(std::sin(toRadiansComplex(x)), settings);
        });
    if (name == QStringLiteral("cos"))
        return unary([&](const Complex &x) {
            return checkedResult(std::cos(toRadiansComplex(x)), settings);
        });
    if (name == QStringLiteral("tan"))
        return unary([&](const Complex &x) {
            return checkedResult(std::tan(toRadiansComplex(x)), settings);
        });
    if (name == QStringLiteral("asin") || name == QStringLiteral("arcsin")
        || name == QStringLiteral("sin⁻¹"))
        return unary([&](const Complex &x) {
            if (isReal(x) && std::abs(x.real()) > 1 && settings.complexMode == ComplexMode::Real)
                raise("DOMAIN");
            return checkedResult(fromRadiansComplex(std::asin(x)), settings);
        });
    if (name == QStringLiteral("acos") || name == QStringLiteral("arccos")
        || name == QStringLiteral("cos⁻¹"))
        return unary([&](const Complex &x) {
            if (isReal(x) && std::abs(x.real()) > 1 && settings.complexMode == ComplexMode::Real)
                raise("DOMAIN");
            return checkedResult(fromRadiansComplex(std::acos(x)), settings);
        });
    if (name == QStringLiteral("atan") || name == QStringLiteral("arctan")
        || name == QStringLiteral("tan⁻¹"))
        return unary([&](const Complex &x) {
            return checkedResult(fromRadiansComplex(std::atan(x)), settings);
        });
    if (name == QStringLiteral("sinh"))
        return unary([&](const Complex &x) { return checkedResult(std::sinh(x), settings); });
    if (name == QStringLiteral("cosh"))
        return unary([&](const Complex &x) { return checkedResult(std::cosh(x), settings); });
    if (name == QStringLiteral("tanh"))
        return unary([&](const Complex &x) { return checkedResult(std::tanh(x), settings); });
    if (name == QStringLiteral("asinh") || name == QStringLiteral("sinh⁻¹"))
        return unary([&](const Complex &x) { return checkedResult(std::asinh(x), settings); });
    if (name == QStringLiteral("acosh") || name == QStringLiteral("cosh⁻¹"))
        return unary([&](const Complex &x) {
            if (isReal(x) && x.real() < 1 && settings.complexMode == ComplexMode::Real)
                raise("DOMAIN");
            return checkedResult(std::acosh(x), settings);
        });
    if (name == QStringLiteral("atanh") || name == QStringLiteral("tanh⁻¹"))
        return unary([&](const Complex &x) {
            if (isReal(x) && std::abs(x.real()) >= 1)
                raise("DOMAIN");
            return checkedResult(std::atanh(x), settings);
        });

    // Logarithms, roots and powers.
    auto logarithm = [&](const Complex &x, double base) {
        if (isReal(x)) {
            const double v = x.real();
            if (v == 0)
                raise("DOMAIN");
            if (v < 0 && settings.complexMode == ComplexMode::Real)
                raise("NONREAL ANS");
            if (v > 0)
                return Complex(base == 0 ? std::log(v) : std::log(v) / std::log(base), 0);
        }
        const Complex result = std::log(x);
        return base == 0 ? result : result / std::log(Complex(base, 0));
    };
    if (name == QStringLiteral("ln"))
        return unary([&](const Complex &x) { return checkedResult(logarithm(x, 0), settings); });
    if (name == QStringLiteral("log")) {
        if (count == 2)  // log(value, base) is the same as logBASE
            return Value(checkedResult(logarithm(args.at(0).number(), realArg(1)), settings));
        return unary([&](const Complex &x) { return checkedResult(logarithm(x, 10), settings); });
    }
    if (name == QStringLiteral("logBASE")) {
        need(2);
        return Value(checkedResult(logarithm(args.at(0).number(), realArg(1)), settings));
    }
    if (name == QStringLiteral("exp"))
        return unary([&](const Complex &x) { return checkedResult(std::exp(x), settings); });
    if (name == QStringLiteral("sqrt"))
        return unary([&](const Complex &x) {
            if (isReal(x) && x.real() < 0 && settings.complexMode == ComplexMode::Real)
                raise("NONREAL ANS");
            if (isReal(x) && x.real() >= 0)
                return Complex(std::sqrt(x.real()), 0);
            return checkedResult(std::sqrt(x), settings);
        });
    if (name == QStringLiteral("cbrt"))
        return unary([&](const Complex &x) {
            if (isReal(x))
                return Complex(std::cbrt(x.real()), 0);
            return checkedResult(std::pow(x, Complex(1.0 / 3.0, 0)), settings);
        });
    if (name == QStringLiteral("xroot")) {
        need(2);
        return binaryValue(QStringLiteral("xroot"), args.at(0), args.at(1), settings);
    }

    // Numbers.
    if (name == QStringLiteral("abs"))
        return unary([](const Complex &x) { return Complex(std::abs(x), 0); });
    if (name == QStringLiteral("sign"))
        return unary([](const Complex &x) {
            const double v = requireReal(x);
            return Complex(v > 0 ? 1 : (v < 0 ? -1 : 0), 0);
        });
    if (name == QStringLiteral("int") || name == QStringLiteral("floor"))
        return unary([](const Complex &x) { return Complex(std::floor(requireReal(x)), 0); });
    if (name == QStringLiteral("ceiling") || name == QStringLiteral("ceil"))
        return unary([](const Complex &x) { return Complex(std::ceil(requireReal(x)), 0); });
    if (name == QStringLiteral("iPart"))
        return unary([](const Complex &x) { return Complex(std::trunc(requireReal(x)), 0); });
    if (name == QStringLiteral("fPart"))
        return unary([](const Complex &x) {
            const double v = requireReal(x);
            return Complex(v - std::trunc(v), 0);
        });
    if (name == QStringLiteral("round")) {
        needBetween(1, 2);
        const double places = count == 2 ? realArg(1) : 9;
        const double scale = std::pow(10.0, std::round(places));
        return mapValue(first(), [&](const Complex &x) {
            return Complex(std::round(x.real() * scale) / scale,
                           std::round(x.imag() * scale) / scale);
        });
    }
    if (name == QStringLiteral("remainder")) {
        need(2);
        const double b = realArg(1);
        if (b == 0)
            raise("DIVIDE BY 0");
        return Value(Complex(std::fmod(realArg(0), b), 0));
    }
    if (name == QStringLiteral("gcd") || name == QStringLiteral("lcm")) {
        need(2);
        auto pairwise = [&](const Complex &x, const Complex &y) {
            long long a = 0, b = 0;
            if (!wholeNumber(requireReal(x), &a) || !wholeNumber(requireReal(y), &b))
                raise("DOMAIN");
            a = std::abs(a);
            b = std::abs(b);
            long long p = a, q = b;
            while (q) {
                const long long t = p % q;
                p = q;
                q = t;
            }
            if (name == QStringLiteral("gcd"))
                return Complex(double(p), 0);
            if (p == 0)
                return Complex(0, 0);
            return Complex(double(a / p * b), 0);
        };
        if (args.at(0).isList() || args.at(1).isList()) {
            const std::vector<Complex> left = asList(args.at(0));
            const std::vector<Complex> right = asList(args.at(1));
            const size_t n = std::max(left.size(), right.size());
            std::vector<Complex> out;
            for (size_t i = 0; i < n; ++i)
                out.push_back(pairwise(left[i % left.size()], right[i % right.size()]));
            return Value::fromList(std::move(out));
        }
        return Value(pairwise(args.at(0).number(), args.at(1).number()));
    }
    if (name == QStringLiteral("min") || name == QStringLiteral("max")) {
        const bool wantMax = name == QStringLiteral("max");
        if (count == 1) {
            const std::vector<double> items = realList(first());
            if (items.empty())
                raise("INVALID DIM");
            double best = items.front();
            for (double item : items)
                best = wantMax ? std::max(best, item) : std::min(best, item);
            return Value(Complex(best, 0));
        }
        need(2);
        if (args.at(0).isList() || args.at(1).isList()) {
            const std::vector<Complex> left = asList(args.at(0));
            const std::vector<Complex> right = asList(args.at(1));
            const size_t n = std::max(left.size(), right.size());
            std::vector<Complex> out;
            for (size_t i = 0; i < n; ++i) {
                const double a = requireReal(left[i % left.size()]);
                const double b = requireReal(right[i % right.size()]);
                out.push_back(Complex(wantMax ? std::max(a, b) : std::min(a, b), 0));
            }
            return Value::fromList(std::move(out));
        }
        const double a = realArg(0);
        const double b = realArg(1);
        return Value(Complex(wantMax ? std::max(a, b) : std::min(a, b), 0));
    }
    if (name == QStringLiteral("not")) {
        return unary([](const Complex &x) { return Complex(requireReal(x) == 0 ? 1 : 0, 0); });
    }

    // Complex parts.
    if (name == QStringLiteral("conj"))
        return unary([](const Complex &x) { return std::conj(x); });
    if (name == QStringLiteral("real"))
        return unary([](const Complex &x) { return Complex(x.real(), 0); });
    if (name == QStringLiteral("imag"))
        return unary([](const Complex &x) { return Complex(x.imag(), 0); });
    if (name == QStringLiteral("angle"))
        return unary([&](const Complex &x) {
            return Complex(fromRadians(std::arg(x), settings.angle), 0);
        });

    // Lists.
    if (name == QStringLiteral("dim")) {
        need(1);
        if (first().isMatrix())
            return Value::fromList({Complex(first().matrix().rows, 0),
                                    Complex(first().matrix().cols, 0)});
        return Value(Complex(double(asList(first()).size()), 0));
    }
    if (name == QStringLiteral("sum") || name == QStringLiteral("prod")) {
        needBetween(1, 3);
        const std::vector<Complex> items = asList(first());
        int start = count >= 2 ? int(std::round(realArg(1))) : 1;
        int end = count >= 3 ? int(std::round(realArg(2))) : int(items.size());
        start = std::max(1, start);
        end = std::min(int(items.size()), end);
        Complex total = name == QStringLiteral("sum") ? Complex(0, 0) : Complex(1, 0);
        for (int i = start; i <= end; ++i) {
            if (name == QStringLiteral("sum"))
                total += items[size_t(i - 1)];
            else
                total *= items[size_t(i - 1)];
        }
        return Value(total);
    }
    if (name == QStringLiteral("mean") || name == QStringLiteral("median")
        || name == QStringLiteral("stdDev") || name == QStringLiteral("variance")) {
        needBetween(1, 2);
        std::vector<double> items = realList(first());
        std::vector<double> weights;
        if (count == 2)
            weights = realList(args.at(1));
        if (items.empty())
            raise("INVALID DIM");
        if (!weights.empty() && weights.size() != items.size())
            raise("DIM MISMATCH");

        if (name == QStringLiteral("median")) {
            std::vector<double> expanded;
            for (size_t i = 0; i < items.size(); ++i) {
                const int repeat = weights.empty() ? 1 : int(std::round(weights[i]));
                for (int k = 0; k < std::max(1, repeat); ++k)
                    expanded.push_back(items[i]);
            }
            std::sort(expanded.begin(), expanded.end());
            const size_t n = expanded.size();
            const double median = n % 2 ? expanded[n / 2]
                                        : 0.5 * (expanded[n / 2 - 1] + expanded[n / 2]);
            return Value(Complex(median, 0));
        }

        double weightTotal = 0;
        double sum = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const double w = weights.empty() ? 1.0 : weights[i];
            weightTotal += w;
            sum += w * items[i];
        }
        if (weightTotal == 0)
            raise("DIVIDE BY 0");
        const double mean = sum / weightTotal;
        if (name == QStringLiteral("mean"))
            return Value(Complex(mean, 0));
        double squares = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const double w = weights.empty() ? 1.0 : weights[i];
            squares += w * (items[i] - mean) * (items[i] - mean);
        }
        if (weightTotal <= 1)
            raise("DIVIDE BY 0");
        const double variance = squares / (weightTotal - 1.0);
        return Value(Complex(name == QStringLiteral("variance") ? variance : std::sqrt(variance), 0));
    }
    if (name == QStringLiteral("cumSum")) {
        need(1);
        std::vector<Complex> items = asList(first());
        for (size_t i = 1; i < items.size(); ++i)
            items[i] += items[i - 1];
        return Value::fromList(std::move(items));
    }
    if (name == QStringLiteral("ΔList")) {
        need(1);
        const std::vector<Complex> items = asList(first());
        if (items.size() < 2)
            raise("INVALID DIM");
        std::vector<Complex> out;
        for (size_t i = 1; i < items.size(); ++i)
            out.push_back(items[i] - items[i - 1]);
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("sortA") || name == QStringLiteral("sortD")) {
        need(1);
        std::vector<double> items = realList(first());
        std::sort(items.begin(), items.end());
        if (name == QStringLiteral("sortD"))
            std::reverse(items.begin(), items.end());
        std::vector<Complex> out;
        for (double item : items)
            out.push_back(Complex(item, 0));
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("augment")) {
        need(2);
        if (args.at(0).isMatrix() && args.at(1).isMatrix())
            return Value::fromMatrix(matrixAugment(args.at(0).matrix(), args.at(1).matrix()));
        std::vector<Complex> out = asList(args.at(0));
        const std::vector<Complex> tail = asList(args.at(1));
        out.insert(out.end(), tail.begin(), tail.end());
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("fill")) {
        need(2);
        const Complex filler = args.at(0).number();
        if (args.at(1).isMatrix()) {
            Matrix m = args.at(1).matrix();
            for (Complex &cell : m.cells)
                cell = filler;
            return Value::fromMatrix(std::move(m));
        }
        std::vector<Complex> out(asList(args.at(1)).size(), filler);
        return Value::fromList(std::move(out));
    }

    // Matrices.
    if (name == QStringLiteral("det")) {
        need(1);
        return Value(matrixDeterminant(first().matrix()));
    }
    if (name == QStringLiteral("transpose")) {
        need(1);
        return Value::fromMatrix(matrixTranspose(first().matrix()));
    }
    if (name == QStringLiteral("identity")) {
        need(1);
        const int n = int(std::round(realArg(0)));
        if (n <= 0 || n > 99)
            raise("INVALID DIM");
        return Value::fromMatrix(matrixIdentity(n));
    }
    if (name == QStringLiteral("ref") || name == QStringLiteral("rref")) {
        need(1);
        return Value::fromMatrix(matrixRowReduce(first().matrix(), name == QStringLiteral("rref")));
    }
    if (name == QStringLiteral("randM")) {
        need(2);
        const int rows = int(std::round(realArg(0)));
        const int cols = int(std::round(realArg(1)));
        if (rows <= 0 || cols <= 0 || rows > 99 || cols > 99)
            raise("INVALID DIM");
        Matrix m(rows, cols);
        for (Complex &cell : m.cells)
            cell = Complex(std::round(randomUniform() * 18.0) - 9.0, 0);
        return Value::fromMatrix(std::move(m));
    }

    // Probability.
    if (name == QStringLiteral("rand")) {
        needBetween(0, 1);
        if (count == 0)
            return Value(Complex(randomUniform(), 0));
        const int n = int(std::round(realArg(0)));
        std::vector<Complex> out;
        for (int i = 0; i < std::max(0, n); ++i)
            out.push_back(Complex(randomUniform(), 0));
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("randInt")) {
        needBetween(2, 3);
        const double low = realArg(0);
        const double high = realArg(1);
        auto draw = [&] {
            return Complex(std::floor(randomUniform() * (high - low + 1.0)) + low, 0);
        };
        if (count == 2)
            return Value(draw());
        std::vector<Complex> out;
        for (int i = 0; i < int(std::round(realArg(2))); ++i)
            out.push_back(draw());
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("randNorm")) {
        needBetween(2, 3);
        if (count == 2)
            return Value(Complex(randomNormal(realArg(0), realArg(1)), 0));
        std::vector<Complex> out;
        for (int i = 0; i < int(std::round(realArg(2))); ++i)
            out.push_back(Complex(randomNormal(realArg(0), realArg(1)), 0));
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("randBin")) {
        needBetween(2, 3);
        auto draw = [&] {
            const int trials = int(std::round(realArg(0)));
            const double p = realArg(1);
            int successes = 0;
            for (int i = 0; i < trials; ++i)
                if (randomUniform() < p)
                    ++successes;
            return Complex(successes, 0);
        };
        if (count == 2)
            return Value(draw());
        std::vector<Complex> out;
        for (int i = 0; i < int(std::round(realArg(2))); ++i)
            out.push_back(draw());
        return Value::fromList(std::move(out));
    }

    // Distributions.
    auto distribution = [&](const std::function<double(const std::vector<double> &)> &fn,
                            int minimum, int maximum, const std::vector<double> &defaults) {
        if (count < minimum || count > maximum)
            raise("ARGUMENT");
        // The first argument may be a list, which evaluates the distribution
        // once per entry exactly as the calculator does.
        std::vector<double> parameters = defaults;
        for (int i = 1; i < count; ++i)
            parameters[size_t(i - 1)] = realArg(i);
        auto evaluateAt = [&](double x) {
            std::vector<double> all;
            all.push_back(x);
            all.insert(all.end(), parameters.begin(), parameters.end());
            return fn(all);
        };
        if (first().isList()) {
            std::vector<Complex> out;
            for (double x : realList(first()))
                out.push_back(Complex(evaluateAt(x), 0));
            return Value::fromList(std::move(out));
        }
        return Value(Complex(evaluateAt(realArg(0)), 0));
    };

    if (name == QStringLiteral("normalpdf"))
        return distribution([](const std::vector<double> &a) {
            return special::normalPdf(a[0], a[1], a[2]);
        }, 1, 3, {0.0, 1.0});
    if (name == QStringLiteral("normalcdf")) {
        needBetween(2, 4);
        const double mean = count > 2 ? realArg(2) : 0.0;
        const double deviation = count > 3 ? realArg(3) : 1.0;
        return Value(Complex(special::normalCdf(realArg(0), realArg(1), mean, deviation), 0));
    }
    if (name == QStringLiteral("invNorm")) {
        needBetween(1, 3);
        const double mean = count > 1 ? realArg(1) : 0.0;
        const double deviation = count > 2 ? realArg(2) : 1.0;
        return Value(Complex(special::invNormal(realArg(0), mean, deviation), 0));
    }
    if (name == QStringLiteral("tpdf"))
        return distribution([](const std::vector<double> &a) {
            return special::studentPdf(a[0], a[1]);
        }, 2, 2, {1.0});
    if (name == QStringLiteral("tcdf")) {
        need(3);
        return Value(Complex(special::studentCdf(realArg(0), realArg(1), realArg(2)), 0));
    }
    if (name == QStringLiteral("invT")) {
        need(2);
        return Value(Complex(special::invStudent(realArg(0), realArg(1)), 0));
    }
    if (name == QStringLiteral("χ²pdf") || name == QStringLiteral("chi2pdf"))
        return distribution([](const std::vector<double> &a) {
            return special::chiSquarePdf(a[0], a[1]);
        }, 2, 2, {1.0});
    if (name == QStringLiteral("χ²cdf") || name == QStringLiteral("chi2cdf")) {
        need(3);
        return Value(Complex(special::chiSquareCdf(realArg(0), realArg(1), realArg(2)), 0));
    }
    if (name == QStringLiteral("Fpdf"))
        return distribution([](const std::vector<double> &a) {
            return special::fPdf(a[0], a[1], a[2]);
        }, 3, 3, {1.0, 1.0});
    if (name == QStringLiteral("Fcdf")) {
        need(4);
        return Value(Complex(special::fCdf(realArg(0), realArg(1), realArg(2), realArg(3)), 0));
    }
    if (name == QStringLiteral("binompdf") || name == QStringLiteral("binomcdf")) {
        needBetween(2, 3);
        const bool cumulative = name == QStringLiteral("binomcdf");
        const double trials = realArg(0);
        const double probability = realArg(1);
        auto at = [&](double k) {
            return cumulative ? special::binomialCdf(trials, probability, k)
                              : special::binomialPdf(trials, probability, k);
        };
        if (count == 3) {
            if (args.at(2).isList()) {
                std::vector<Complex> out;
                for (double k : realList(args.at(2)))
                    out.push_back(Complex(at(k), 0));
                return Value::fromList(std::move(out));
            }
            return Value(Complex(at(realArg(2)), 0));
        }
        std::vector<Complex> out;
        for (int k = 0; k <= int(std::round(trials)); ++k)
            out.push_back(Complex(at(k), 0));
        return Value::fromList(std::move(out));
    }
    if (name == QStringLiteral("poissonpdf") || name == QStringLiteral("poissoncdf")) {
        need(2);
        const bool cumulative = name == QStringLiteral("poissoncdf");
        const double mean = realArg(0);
        auto at = [&](double k) {
            return cumulative ? special::poissonCdf(mean, k) : special::poissonPdf(mean, k);
        };
        if (args.at(1).isList()) {
            std::vector<Complex> out;
            for (double k : realList(args.at(1)))
                out.push_back(Complex(at(k), 0));
            return Value::fromList(std::move(out));
        }
        return Value(Complex(at(realArg(1)), 0));
    }
    if (name == QStringLiteral("geometpdf") || name == QStringLiteral("geometcdf")) {
        need(2);
        const bool cumulative = name == QStringLiteral("geometcdf");
        const double probability = realArg(0);
        auto at = [&](double k) {
            return cumulative ? special::geometricCdf(probability, k)
                              : special::geometricPdf(probability, k);
        };
        if (args.at(1).isList()) {
            std::vector<Complex> out;
            for (double k : realList(args.at(1)))
                out.push_back(Complex(at(k), 0));
            return Value::fromList(std::move(out));
        }
        return Value(Complex(at(realArg(1)), 0));
    }

    raise("SYNTAX");
}

}  // namespace

// --- Context ---------------------------------------------------------------

bool Context::has(const QString &name) const { return variables.contains(name); }

Value Context::value(const QString &name) const { return variables.value(name); }

void Context::store(const QString &name, const Value &value) { variables.insert(name, value); }

NodePtr Context::parseCached(const QString &source) {
    const auto it = m_cache.constFind(source);
    if (it != m_cache.constEnd())
        return it.value();
    NodePtr parsed = parse(source);
    m_cache.insert(source, parsed);
    return parsed;
}

Value Context::callWith(const QString &source, const QString &parameter, const Value &argument) {
    if (m_depth > 24)
        raise("MEMORY");
    ++m_depth;
    const bool had = has(parameter);
    const Value previous = had ? value(parameter) : Value();
    store(parameter, argument);
    try {
        const Value result = evaluate(parseCached(source), *this);
        if (had)
            store(parameter, previous);
        else
            variables.remove(parameter);
        --m_depth;
        return result;
    } catch (...) {
        if (had)
            store(parameter, previous);
        else
            variables.remove(parameter);
        --m_depth;
        throw;
    }
}

Value Context::callDefinition(const QString &name, const Value &argument) {
    const QString source = functions.value(name);
    if (source.trimmed().isEmpty())
        raise("UNDEFINED");
    QString parameter = QStringLiteral("X");
    if (name.startsWith(QLatin1Char('r')))
        parameter = QStringLiteral("θ");
    else if (name.endsWith(QLatin1Char('T')))
        parameter = QStringLiteral("T");
    else if (name.startsWith(QLatin1Char('u')) || name.startsWith(QLatin1Char('v'))
             || name.startsWith(QLatin1Char('w')))
        parameter = QStringLiteral("n");
    return callWith(source, parameter, argument);
}

Value evaluate(const NodePtr &program, Context &context) {
    Evaluator evaluator(context);
    return evaluator.eval(program);
}

Value evaluate(const QString &source, Context &context) {
    context.displayHint = DisplayHint::None;
    return evaluate(parse(source), context);
}

// --- Formatting ------------------------------------------------------------

namespace {

QString stripTrailingZeros(QString text) {
    if (!text.contains(QLatin1Char('.')))
        return text;
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

// Ten significant digits is what the display holds; everything wider goes to
// scientific notation, matching what the calculator shows.
QString scientificString(double value, const Settings &settings, bool engineering) {
    if (value == 0)
        return settings.fixDigits >= 0
            ? QStringLiteral("0.").append(QString(settings.fixDigits, QLatin1Char('0')))
                  .append(QStringLiteral("E0"))
            : QStringLiteral("0E0");

    int exponent = int(std::floor(std::log10(std::abs(value))));
    if (engineering)
        exponent = int(std::floor(double(exponent) / 3.0)) * 3;
    double mantissa = value / std::pow(10.0, exponent);

    const int decimals = settings.fixDigits >= 0 ? settings.fixDigits
                                                 : (engineering ? 7 : 9);
    QString mantissaText = QString::number(mantissa, 'f', decimals);
    // Rounding can carry the mantissa over the top of its range.
    if (std::abs(QLocale::c().toDouble(mantissaText)) >= (engineering ? 1000.0 : 10.0)) {
        exponent += engineering ? 3 : 1;
        mantissa = value / std::pow(10.0, exponent);
        mantissaText = QString::number(mantissa, 'f', decimals);
    }
    if (settings.fixDigits < 0)
        mantissaText = stripTrailingZeros(mantissaText);
    return mantissaText + QStringLiteral("E") + QString::number(exponent);
}

}  // namespace

QString formatReal(double value, const Settings &settings) {
    if (value == 0)
        value = 0;  // Collapse negative zero.
    if (std::isnan(value))
        return QStringLiteral("undefined");
    if (std::isinf(value))
        return value > 0 ? QStringLiteral("∞") : QStringLiteral("-∞");

    if (settings.format == NumberFormat::Scientific)
        return scientificString(value, settings, false);
    if (settings.format == NumberFormat::Engineering)
        return scientificString(value, settings, true);

    const double magnitude = std::abs(value);
    if (magnitude != 0 && (magnitude >= 1e10 || magnitude < 1e-3))
        return scientificString(value, settings, false);

    if (settings.fixDigits >= 0)
        return QString::number(value, 'f', settings.fixDigits);

    const int integerDigits = magnitude < 1 ? 0 : int(std::floor(std::log10(magnitude))) + 1;
    const int decimals = std::max(0, 10 - integerDigits);
    return stripTrailingZeros(QString::number(value, 'f', decimals));
}

QString formatNumber(Complex value, const Settings &settings) {
    if (value.imag() == 0)
        return formatReal(value.real(), settings);

    if (settings.complexMode == ComplexMode::Polar) {
        const double radius = std::abs(value);
        const double angle = fromRadians(std::arg(value), settings.angle);
        return formatReal(radius, settings) + QStringLiteral("e^(")
            + formatReal(angle, settings) + QStringLiteral("i)");
    }

    const QString imaginary = formatReal(std::abs(value.imag()), settings);
    const QString sign = value.imag() < 0 ? QStringLiteral("-") : QStringLiteral("+");
    if (value.real() == 0)
        return (value.imag() < 0 ? QStringLiteral("-") : QString()) + imaginary
            + QStringLiteral("i");
    return formatReal(value.real(), settings) + sign + imaginary + QStringLiteral("i");
}

QString formatValue(const Value &value, const Settings &settings) {
    if (value.isNumber())
        return formatNumber(value.number(), settings);
    if (value.isList()) {
        QStringList parts;
        for (const Complex &item : value.list())
            parts << formatNumber(item, settings);
        return QStringLiteral("{") + parts.join(QStringLiteral(", ")) + QStringLiteral("}");
    }
    const Matrix &m = value.matrix();
    QStringList rows;
    for (int r = 0; r < m.rows; ++r) {
        QStringList cells;
        for (int c = 0; c < m.cols; ++c)
            cells << formatNumber(m.at(r, c), settings);
        rows << QStringLiteral("[") + cells.join(QStringLiteral(", ")) + QStringLiteral("]");
    }
    return QStringLiteral("[") + rows.join(QString()) + QStringLiteral("]");
}

// Continued fractions, stopping at the same four-digit denominator the
// calculator gives up at.
bool toFraction(double value, long long *numerator, long long *denominator) {
    if (!std::isfinite(value) || std::abs(value) > 1e9)
        return false;
    const double sign = value < 0 ? -1.0 : 1.0;
    double x = std::abs(value);
    long long previousNumerator = 1, previousDenominator = 0;
    long long currentNumerator = 0, currentDenominator = 1;
    double remainder = x;
    for (int i = 0; i < 32; ++i) {
        const double whole = std::floor(remainder);
        const long long a = static_cast<long long>(whole);
        const long long nextNumerator = a * previousNumerator + currentNumerator;
        const long long nextDenominator = a * previousDenominator + currentDenominator;
        currentNumerator = previousNumerator;
        currentDenominator = previousDenominator;
        previousNumerator = nextNumerator;
        previousDenominator = nextDenominator;
        if (previousDenominator > 9999 || previousDenominator <= 0)
            break;
        if (std::abs(double(previousNumerator) / double(previousDenominator) - x)
            < 1e-10 * std::max(1.0, x)) {
            *numerator = static_cast<long long>(sign) * previousNumerator;
            *denominator = previousDenominator;
            return *denominator != 1;
        }
        const double fraction = remainder - whole;
        if (fraction < 1e-12)
            break;
        remainder = 1.0 / fraction;
    }
    return false;
}

QString formatWithHint(const Value &value, const Settings &settings, DisplayHint hint) {
    if (hint == DisplayHint::Fraction) {
        auto asFraction = [&](const Complex &item) {
            long long numerator = 0, denominator = 0;
            if (item.imag() == 0 && toFraction(item.real(), &numerator, &denominator))
                return QString::number(numerator) + QStringLiteral("/")
                    + QString::number(denominator);
            return formatNumber(item, settings);
        };
        if (value.isNumber())
            return asFraction(value.number());
        if (value.isList()) {
            QStringList parts;
            for (const Complex &item : value.list())
                parts << asFraction(item);
            return QStringLiteral("{") + parts.join(QStringLiteral(", ")) + QStringLiteral("}");
        }
    }
    if (hint == DisplayHint::DegreeMinuteSecond && value.isNumber()
        && value.number().imag() == 0) {
        double degrees = value.number().real();
        const QString sign = degrees < 0 ? QStringLiteral("-") : QString();
        degrees = std::abs(degrees);
        const int whole = int(std::floor(degrees));
        const double minutesTotal = (degrees - whole) * 60.0;
        int minutes = int(std::floor(minutesTotal));
        double seconds = (minutesTotal - minutes) * 60.0;
        if (seconds >= 59.9995) {  // Keep the rounded seconds from reading 60.
            seconds = 0;
            ++minutes;
        }
        return sign + QString::number(whole) + QStringLiteral("°") + QString::number(minutes)
            + QStringLiteral("'") + QString::number(seconds, 'f', 3) + QStringLiteral("\"");
    }
    if (hint == DisplayHint::Polar || hint == DisplayHint::Rectangular) {
        Settings adjusted = settings;
        adjusted.complexMode = hint == DisplayHint::Polar ? ComplexMode::Polar
                                                          : ComplexMode::Rectangular;
        return formatValue(value, adjusted);
    }
    return formatValue(value, settings);
}

}  // namespace calc
