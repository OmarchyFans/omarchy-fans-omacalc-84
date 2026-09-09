#include "stats.h"

#include "special.h"

#include <algorithm>
#include <cmath>

namespace stats {
namespace {

constexpr double kPi = 3.14159265358979323846;

double quantile(const std::vector<double> &sorted, double position) {
    if (sorted.empty())
        return 0;
    if (position <= 0)
        return sorted.front();
    if (position >= double(sorted.size()) - 1)
        return sorted.back();
    const size_t low = size_t(std::floor(position));
    const double fraction = position - double(low);
    return sorted[low] + fraction * (sorted[low + 1] - sorted[low]);
}

// The calculator's quartiles are the medians of the halves either side of the
// median, with the median itself left out when the count is odd.
double medianOf(const std::vector<double> &sorted, size_t from, size_t to) {
    if (from >= to)
        return 0;
    const size_t n = to - from;
    if (n % 2)
        return sorted[from + n / 2];
    return 0.5 * (sorted[from + n / 2 - 1] + sorted[from + n / 2]);
}

std::vector<double> expand(const std::vector<double> &values,
                           const std::vector<double> &frequencies) {
    std::vector<double> out;
    for (size_t i = 0; i < values.size(); ++i) {
        const int repeat = frequencies.empty() ? 1 : int(std::round(frequencies[i]));
        for (int k = 0; k < std::max(0, repeat); ++k)
            out.push_back(values[i]);
    }
    std::sort(out.begin(), out.end());
    return out;
}

QString formatCoefficient(double value) {
    QString text = QString::number(value, 'g', 10);
    return text;
}

}  // namespace

bool solveLinearSystem(std::vector<std::vector<double>> matrix, std::vector<double> &solution) {
    const size_t n = matrix.size();
    if (n == 0)
        return false;
    for (size_t col = 0; col < n; ++col) {
        size_t pivot = col;
        for (size_t r = col + 1; r < n; ++r) {
            if (std::abs(matrix[r][col]) > std::abs(matrix[pivot][col]))
                pivot = r;
        }
        if (std::abs(matrix[pivot][col]) < 1e-12)
            return false;
        std::swap(matrix[col], matrix[pivot]);
        for (size_t r = 0; r < n; ++r) {
            if (r == col)
                continue;
            const double factor = matrix[r][col] / matrix[col][col];
            for (size_t c = col; c <= n; ++c)
                matrix[r][c] -= factor * matrix[col][c];
        }
    }
    solution.assign(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        solution[i] = matrix[i][n] / matrix[i][i];
    return true;
}

OneVariable oneVariable(const std::vector<double> &values,
                        const std::vector<double> &frequencies) {
    OneVariable result;
    if (values.empty())
        return result;

    double count = 0;
    double sum = 0;
    double sumSquares = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        const double weight = frequencies.empty() ? 1.0 : frequencies[i];
        count += weight;
        sum += weight * values[i];
        sumSquares += weight * values[i] * values[i];
    }
    result.count = count;
    result.sum = sum;
    result.sumSquares = sumSquares;
    if (count <= 0)
        return result;

    result.mean = sum / count;
    const double spread = sumSquares - sum * sum / count;
    result.populationDeviation = std::sqrt(std::max(0.0, spread) / count);
    result.sampleDeviation = count > 1 ? std::sqrt(std::max(0.0, spread) / (count - 1)) : 0.0;

    const std::vector<double> sorted = expand(values, frequencies);
    if (sorted.empty())
        return result;
    result.minimum = sorted.front();
    result.maximum = sorted.back();
    const size_t n = sorted.size();
    result.median = medianOf(sorted, 0, n);
    result.lowerQuartile = medianOf(sorted, 0, n / 2);
    result.upperQuartile = medianOf(sorted, n % 2 ? n / 2 + 1 : n / 2, n);
    return result;
}

TwoVariable twoVariable(const std::vector<double> &x, const std::vector<double> &y) {
    TwoVariable result;
    const size_t n = std::min(x.size(), y.size());
    if (n == 0)
        return result;

    const OneVariable statsX = oneVariable({x.begin(), x.begin() + long(n)}, {});
    const OneVariable statsY = oneVariable({y.begin(), y.begin() + long(n)}, {});
    result.count = double(n);
    result.sumX = statsX.sum;
    result.sumY = statsY.sum;
    result.sumXSquares = statsX.sumSquares;
    result.sumYSquares = statsY.sumSquares;
    result.meanX = statsX.mean;
    result.meanY = statsY.mean;
    result.sampleDeviationX = statsX.sampleDeviation;
    result.populationDeviationX = statsX.populationDeviation;
    result.sampleDeviationY = statsY.sampleDeviation;
    result.populationDeviationY = statsY.populationDeviation;
    result.minimumX = statsX.minimum;
    result.maximumX = statsX.maximum;
    result.minimumY = statsY.minimum;
    result.maximumY = statsY.maximum;
    for (size_t i = 0; i < n; ++i)
        result.sumXY += x[i] * y[i];
    return result;
}

namespace {

// Fits y against the given powers of x by least squares.
Regression polynomialFit(const std::vector<double> &x, const std::vector<double> &y, int degree,
                         const QString &name) {
    Regression result;
    result.name = name;
    const size_t n = std::min(x.size(), y.size());
    const int terms = degree + 1;
    if (int(n) < terms)
        return result;

    std::vector<std::vector<double>> normal(size_t(terms), std::vector<double>(size_t(terms) + 1, 0.0));
    for (int r = 0; r < terms; ++r) {
        for (int c = 0; c < terms; ++c) {
            double sum = 0;
            for (size_t i = 0; i < n; ++i)
                sum += std::pow(x[i], double(degree - r)) * std::pow(x[i], double(degree - c));
            normal[size_t(r)][size_t(c)] = sum;
        }
        double sum = 0;
        for (size_t i = 0; i < n; ++i)
            sum += y[i] * std::pow(x[i], double(degree - r));
        normal[size_t(r)][size_t(terms)] = sum;
    }

    std::vector<double> solution;
    if (!solveLinearSystem(normal, solution))
        return result;

    result.valid = true;
    result.coefficients = solution;

    double meanY = 0;
    for (size_t i = 0; i < n; ++i)
        meanY += y[i];
    meanY /= double(n);
    double residual = 0;
    double total = 0;
    for (size_t i = 0; i < n; ++i) {
        double predicted = 0;
        for (int t = 0; t < terms; ++t)
            predicted += solution[size_t(t)] * std::pow(x[i], double(degree - t));
        residual += (y[i] - predicted) * (y[i] - predicted);
        total += (y[i] - meanY) * (y[i] - meanY);
    }
    result.determination = total > 0 ? 1.0 - residual / total : 1.0;
    if (degree == 1) {
        result.hasCorrelation = true;
        const double sign = solution[0] < 0 ? -1.0 : 1.0;
        result.correlation = sign * std::sqrt(std::max(0.0, result.determination));
    }
    return result;
}

// Straight-line fit of transformed data, which is how the calculator produces
// its logarithmic, exponential and power models.
Regression transformedFit(const std::vector<double> &x, const std::vector<double> &y,
                          bool logX, bool logY, const QString &name) {
    Regression result;
    result.name = name;
    std::vector<double> tx;
    std::vector<double> ty;
    const size_t n = std::min(x.size(), y.size());
    for (size_t i = 0; i < n; ++i) {
        if ((logX && x[i] <= 0) || (logY && y[i] <= 0))
            return result;
        tx.push_back(logX ? std::log(x[i]) : x[i]);
        ty.push_back(logY ? std::log(y[i]) : y[i]);
    }
    Regression linear = polynomialFit(tx, ty, 1, name);
    if (!linear.valid)
        return result;

    result.valid = true;
    result.hasCorrelation = true;
    result.correlation = linear.correlation;
    result.determination = linear.determination;
    const double slope = linear.coefficients[0];
    const double intercept = linear.coefficients[1];
    if (!logY) {
        result.coefficients = {intercept, slope};  // y = a + b ln x
    } else if (logX) {
        result.coefficients = {std::exp(intercept), slope};  // y = a x^b
    } else {
        result.coefficients = {std::exp(intercept), std::exp(slope)};  // y = a b^x
    }
    return result;
}

// Gauss-Newton with a damping term, used for the two models that cannot be
// reached by a change of variables.
Regression iterativeFit(const std::vector<double> &x, const std::vector<double> &y,
                        std::vector<double> guess,
                        const std::function<double(const std::vector<double> &, double)> &model,
                        const QString &name) {
    Regression result;
    result.name = name;
    const size_t n = std::min(x.size(), y.size());
    const size_t p = guess.size();
    if (n < p)
        return result;

    std::vector<double> parameters = guess;
    double damping = 1e-3;
    for (int iteration = 0; iteration < 400; ++iteration) {
        std::vector<std::vector<double>> normal(p, std::vector<double>(p + 1, 0.0));
        double before = 0;
        for (size_t i = 0; i < n; ++i) {
            const double residual = y[i] - model(parameters, x[i]);
            before += residual * residual;
            std::vector<double> gradient(p, 0.0);
            for (size_t k = 0; k < p; ++k) {
                std::vector<double> shifted = parameters;
                const double step = std::max(1e-7, std::abs(parameters[k]) * 1e-6);
                shifted[k] += step;
                gradient[k] = (model(shifted, x[i]) - model(parameters, x[i])) / step;
            }
            for (size_t r = 0; r < p; ++r) {
                for (size_t c = 0; c < p; ++c)
                    normal[r][c] += gradient[r] * gradient[c];
                normal[r][p] += gradient[r] * residual;
            }
        }
        for (size_t k = 0; k < p; ++k)
            normal[k][k] *= (1.0 + damping);

        std::vector<double> delta;
        if (!solveLinearSystem(normal, delta))
            break;

        std::vector<double> candidate = parameters;
        for (size_t k = 0; k < p; ++k)
            candidate[k] += delta[k];
        double after = 0;
        for (size_t i = 0; i < n; ++i) {
            const double residual = y[i] - model(candidate, x[i]);
            after += residual * residual;
        }
        if (!std::isfinite(after)) {
            damping *= 10.0;
            continue;
        }
        if (after < before) {
            parameters = candidate;
            damping = std::max(1e-9, damping / 3.0);
            if (before - after < 1e-14 * std::max(1.0, before))
                break;
        } else {
            damping *= 4.0;
            if (damping > 1e12)
                break;
        }
    }

    double meanY = 0;
    for (size_t i = 0; i < n; ++i)
        meanY += y[i];
    meanY /= double(n);
    double residual = 0;
    double total = 0;
    for (size_t i = 0; i < n; ++i) {
        const double error = y[i] - model(parameters, x[i]);
        residual += error * error;
        total += (y[i] - meanY) * (y[i] - meanY);
    }
    if (!std::isfinite(residual))
        return result;
    result.valid = true;
    result.coefficients = parameters;
    result.determination = total > 0 ? 1.0 - residual / total : 1.0;
    return result;
}

// The median-median line: the medians of the outer thirds set the slope.
Regression medianMedianFit(const std::vector<double> &x, const std::vector<double> &y) {
    Regression result;
    result.name = QStringLiteral("Med-Med");
    const size_t n = std::min(x.size(), y.size());
    if (n < 3)
        return result;

    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return x[a] < x[b]; });

    const size_t third = (n + 2) / 3;
    auto medianOfRange = [&](size_t from, size_t to, bool useX) {
        std::vector<double> values;
        for (size_t i = from; i < to; ++i)
            values.push_back(useX ? x[order[i]] : y[order[i]]);
        std::sort(values.begin(), values.end());
        return medianOf(values, 0, values.size());
    };

    const double x1 = medianOfRange(0, third, true);
    const double y1 = medianOfRange(0, third, false);
    const double x3 = medianOfRange(n - third, n, true);
    const double y3 = medianOfRange(n - third, n, false);
    if (x3 == x1)
        return result;

    const double slope = (y3 - y1) / (x3 - x1);
    const double x2 = medianOfRange(third, n - third, true);
    const double y2 = medianOfRange(third, n - third, false);
    const double intercept = ((y1 - slope * x1) + (y2 - slope * x2) + (y3 - slope * x3)) / 3.0;

    result.valid = true;
    result.coefficients = {slope, intercept};
    return result;
}

}  // namespace

Regression fit(Model model, const std::vector<double> &x, const std::vector<double> &y) {
    Regression result;
    switch (model) {
    case Model::Linear:
        result = polynomialFit(x, y, 1, QStringLiteral("LinReg(ax+b)"));
        if (result.valid)
            result.equation = QStringLiteral("y=") + formatCoefficient(result.coefficients[0])
                + QStringLiteral("x+") + formatCoefficient(result.coefficients[1]);
        return result;
    case Model::LinearAB:
        result = polynomialFit(x, y, 1, QStringLiteral("LinReg(a+bx)"));
        if (result.valid) {
            std::swap(result.coefficients[0], result.coefficients[1]);
            result.equation = QStringLiteral("y=") + formatCoefficient(result.coefficients[0])
                + QStringLiteral("+") + formatCoefficient(result.coefficients[1])
                + QStringLiteral("x");
        }
        return result;
    case Model::Quadratic:
        result = polynomialFit(x, y, 2, QStringLiteral("QuadReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=ax²+bx+c");
        return result;
    case Model::Cubic:
        result = polynomialFit(x, y, 3, QStringLiteral("CubicReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=ax³+bx²+cx+d");
        return result;
    case Model::Quartic:
        result = polynomialFit(x, y, 4, QStringLiteral("QuartReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=ax⁴+bx³+cx²+dx+e");
        return result;
    case Model::Logarithmic:
        result = transformedFit(x, y, true, false, QStringLiteral("LnReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=a+b ln(x)");
        return result;
    case Model::Exponential:
        result = transformedFit(x, y, false, true, QStringLiteral("ExpReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=a·b^x");
        return result;
    case Model::Power:
        result = transformedFit(x, y, true, true, QStringLiteral("PwrReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=a·x^b");
        return result;
    case Model::MedianMedian:
        result = medianMedianFit(x, y);
        if (result.valid)
            result.equation = QStringLiteral("y=ax+b");
        return result;
    case Model::Logistic: {
        double maximum = 1;
        for (double value : y)
            maximum = std::max(maximum, value);
        const std::vector<double> guess = {1.0, 0.5, maximum * 1.05};
        result = iterativeFit(x, y, guess,
                              [](const std::vector<double> &p, double xv) {
                                  return p[2] / (1.0 + p[0] * std::exp(-p[1] * xv));
                              },
                              QStringLiteral("Logistic"));
        if (result.valid)
            result.equation = QStringLiteral("y=c/(1+a·e^(-bx))");
        return result;
    }
    case Model::Sinusoidal: {
        double minimum = y.empty() ? 0 : y.front();
        double maximum = y.empty() ? 0 : y.front();
        for (double value : y) {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
        double spanX = 1;
        if (x.size() > 1)
            spanX = std::max(1e-9, *std::max_element(x.begin(), x.end())
                                 - *std::min_element(x.begin(), x.end()));
        const std::vector<double> guess = {(maximum - minimum) / 2.0, 2.0 * kPi / spanX, 0.0,
                                           (maximum + minimum) / 2.0};
        result = iterativeFit(x, y, guess,
                              [](const std::vector<double> &p, double xv) {
                                  return p[0] * std::sin(p[1] * xv + p[2]) + p[3];
                              },
                              QStringLiteral("SinReg"));
        if (result.valid)
            result.equation = QStringLiteral("y=a·sin(bx+c)+d");
        return result;
    }
    }
    return result;
}

}  // namespace stats

// --- Inferential statistics ------------------------------------------------

namespace stats {
namespace {

double normalTailProbability(double z, Tail tail) {
    const double upper = special::normalCdf(z, 40, 0, 1);
    switch (tail) {
    case Tail::Less:
        return special::normalCdf(-40, z, 0, 1);
    case Tail::Greater:
        return upper;
    default:
        return 2.0 * special::normalCdf(std::abs(z), 40, 0, 1);
    }
}

double studentTailProbability(double t, double degrees, Tail tail) {
    switch (tail) {
    case Tail::Less:
        return special::studentCdf(-1e4, t, degrees);
    case Tail::Greater:
        return special::studentCdf(t, 1e4, degrees);
    default:
        return 2.0 * special::studentCdf(std::abs(t), 1e4, degrees);
    }
}

Inference make(const QString &name, std::vector<std::pair<QString, double>> values) {
    Inference result;
    result.valid = true;
    result.name = name;
    result.values = std::move(values);
    return result;
}

}  // namespace

Inference zTest(double hypothesised, double deviation, double mean, double count, Tail tail) {
    if (deviation <= 0 || count <= 0)
        return {};
    const double z = (mean - hypothesised) / (deviation / std::sqrt(count));
    return make(QStringLiteral("Z-Test"),
                {{QStringLiteral("z"), z},
                 {QStringLiteral("p"), normalTailProbability(z, tail)},
                 {QStringLiteral("x̄"), mean},
                 {QStringLiteral("n"), count}});
}

Inference tTest(double hypothesised, double mean, double sampleDeviation, double count,
                Tail tail) {
    if (sampleDeviation <= 0 || count <= 1)
        return {};
    const double t = (mean - hypothesised) / (sampleDeviation / std::sqrt(count));
    const double degrees = count - 1;
    return make(QStringLiteral("T-Test"),
                {{QStringLiteral("t"), t},
                 {QStringLiteral("p"), studentTailProbability(t, degrees, tail)},
                 {QStringLiteral("df"), degrees},
                 {QStringLiteral("x̄"), mean},
                 {QStringLiteral("Sx"), sampleDeviation},
                 {QStringLiteral("n"), count}});
}

Inference twoSampleTTest(double meanA, double deviationA, double countA, double meanB,
                         double deviationB, double countB, Tail tail, bool pooled) {
    if (countA <= 1 || countB <= 1 || deviationA <= 0 || deviationB <= 0)
        return {};

    double t = 0;
    double degrees = 0;
    if (pooled) {
        const double pooledVariance =
            ((countA - 1) * deviationA * deviationA + (countB - 1) * deviationB * deviationB)
            / (countA + countB - 2);
        t = (meanA - meanB) / std::sqrt(pooledVariance * (1.0 / countA + 1.0 / countB));
        degrees = countA + countB - 2;
    } else {
        const double a = deviationA * deviationA / countA;
        const double b = deviationB * deviationB / countB;
        t = (meanA - meanB) / std::sqrt(a + b);
        // Welch-Satterthwaite, the calculator's default for unpooled samples.
        degrees = (a + b) * (a + b)
            / (a * a / (countA - 1) + b * b / (countB - 1));
    }
    return make(QStringLiteral("2-SampTTest"),
                {{QStringLiteral("t"), t},
                 {QStringLiteral("p"), studentTailProbability(t, degrees, tail)},
                 {QStringLiteral("df"), degrees},
                 {QStringLiteral("x̄1"), meanA},
                 {QStringLiteral("x̄2"), meanB}});
}

Inference twoSampleZTest(double deviationA, double deviationB, double meanA, double countA,
                         double meanB, double countB, Tail tail) {
    if (deviationA <= 0 || deviationB <= 0 || countA <= 0 || countB <= 0)
        return {};
    const double z = (meanA - meanB)
        / std::sqrt(deviationA * deviationA / countA + deviationB * deviationB / countB);
    return make(QStringLiteral("2-SampZTest"),
                {{QStringLiteral("z"), z},
                 {QStringLiteral("p"), normalTailProbability(z, tail)},
                 {QStringLiteral("x̄1"), meanA},
                 {QStringLiteral("x̄2"), meanB}});
}

Inference onePropZTest(double hypothesised, double successes, double count, Tail tail) {
    if (count <= 0 || hypothesised <= 0 || hypothesised >= 1)
        return {};
    const double observed = successes / count;
    const double z = (observed - hypothesised)
        / std::sqrt(hypothesised * (1.0 - hypothesised) / count);
    return make(QStringLiteral("1-PropZTest"),
                {{QStringLiteral("z"), z},
                 {QStringLiteral("p"), normalTailProbability(z, tail)},
                 {QStringLiteral("p̂"), observed},
                 {QStringLiteral("n"), count}});
}

Inference twoPropZTest(double successesA, double countA, double successesB, double countB,
                       Tail tail) {
    if (countA <= 0 || countB <= 0)
        return {};
    const double first = successesA / countA;
    const double second = successesB / countB;
    const double combined = (successesA + successesB) / (countA + countB);
    const double standardError =
        std::sqrt(combined * (1.0 - combined) * (1.0 / countA + 1.0 / countB));
    if (standardError <= 0)
        return {};
    const double z = (first - second) / standardError;
    return make(QStringLiteral("2-PropZTest"),
                {{QStringLiteral("z"), z},
                 {QStringLiteral("p"), normalTailProbability(z, tail)},
                 {QStringLiteral("p̂1"), first},
                 {QStringLiteral("p̂2"), second},
                 {QStringLiteral("p̂"), combined}});
}

Inference goodnessOfFit(const std::vector<double> &observed, const std::vector<double> &expected,
                        double degrees) {
    if (observed.empty() || observed.size() != expected.size())
        return {};
    double statistic = 0;
    for (size_t i = 0; i < observed.size(); ++i) {
        if (expected[i] <= 0)
            return {};
        const double difference = observed[i] - expected[i];
        statistic += difference * difference / expected[i];
    }
    if (degrees <= 0)
        degrees = double(observed.size()) - 1;
    return make(QStringLiteral("χ²GOF-Test"),
                {{QStringLiteral("χ²"), statistic},
                 {QStringLiteral("p"), 1.0 - special::chiSquareCdf(0, statistic, degrees)},
                 {QStringLiteral("df"), degrees}});
}

Inference independenceTest(const std::vector<std::vector<double>> &table) {
    if (table.size() < 2 || table.front().size() < 2)
        return {};
    const size_t rows = table.size();
    const size_t columns = table.front().size();

    std::vector<double> rowTotals(rows, 0.0);
    std::vector<double> columnTotals(columns, 0.0);
    double total = 0;
    for (size_t r = 0; r < rows; ++r) {
        if (table[r].size() != columns)
            return {};
        for (size_t c = 0; c < columns; ++c) {
            rowTotals[r] += table[r][c];
            columnTotals[c] += table[r][c];
            total += table[r][c];
        }
    }
    if (total <= 0)
        return {};

    double statistic = 0;
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < columns; ++c) {
            const double expected = rowTotals[r] * columnTotals[c] / total;
            if (expected <= 0)
                return {};
            const double difference = table[r][c] - expected;
            statistic += difference * difference / expected;
        }
    }
    const double degrees = double(rows - 1) * double(columns - 1);
    return make(QStringLiteral("χ²-Test"),
                {{QStringLiteral("χ²"), statistic},
                 {QStringLiteral("p"), 1.0 - special::chiSquareCdf(0, statistic, degrees)},
                 {QStringLiteral("df"), degrees}});
}

Inference linearRegressionTTest(const std::vector<double> &x, const std::vector<double> &y,
                                Tail tail) {
    const size_t n = std::min(x.size(), y.size());
    if (n < 3)
        return {};
    const Regression line = fit(Model::Linear, x, y);
    if (!line.valid)
        return {};

    const double slope = line.coefficients[0];
    const double intercept = line.coefficients[1];
    double meanX = 0;
    for (size_t i = 0; i < n; ++i)
        meanX += x[i];
    meanX /= double(n);

    double residual = 0;
    double spreadX = 0;
    for (size_t i = 0; i < n; ++i) {
        const double error = y[i] - (slope * x[i] + intercept);
        residual += error * error;
        spreadX += (x[i] - meanX) * (x[i] - meanX);
    }
    if (spreadX <= 0)
        return {};
    const double degrees = double(n) - 2;
    const double standardError = std::sqrt(residual / degrees / spreadX);
    if (standardError <= 0)
        return {};
    const double t = slope / standardError;
    return make(QStringLiteral("LinRegTTest"),
                {{QStringLiteral("t"), t},
                 {QStringLiteral("p"), studentTailProbability(t, degrees, tail)},
                 {QStringLiteral("df"), degrees},
                 {QStringLiteral("a"), slope},
                 {QStringLiteral("b"), intercept},
                 {QStringLiteral("r²"), line.determination},
                 {QStringLiteral("r"), line.correlation}});
}

Inference analysisOfVariance(const std::vector<std::vector<double>> &groups) {
    if (groups.size() < 2)
        return {};
    double total = 0;
    double count = 0;
    for (const std::vector<double> &group : groups) {
        if (group.empty())
            return {};
        for (double value : group) {
            total += value;
            count += 1;
        }
    }
    const double grandMean = total / count;

    double between = 0;
    double within = 0;
    for (const std::vector<double> &group : groups) {
        double groupMean = 0;
        for (double value : group)
            groupMean += value;
        groupMean /= double(group.size());
        between += double(group.size()) * (groupMean - grandMean) * (groupMean - grandMean);
        for (double value : group)
            within += (value - groupMean) * (value - groupMean);
    }

    const double degreesBetween = double(groups.size()) - 1;
    const double degreesWithin = count - double(groups.size());
    if (degreesWithin <= 0 || within <= 0)
        return {};
    const double f = (between / degreesBetween) / (within / degreesWithin);
    return make(QStringLiteral("ANOVA"),
                {{QStringLiteral("F"), f},
                 {QStringLiteral("p"), 1.0 - special::fCdf(0, f, degreesBetween, degreesWithin)},
                 {QStringLiteral("Factor df"), degreesBetween},
                 {QStringLiteral("Error df"), degreesWithin},
                 {QStringLiteral("Factor MS"), between / degreesBetween},
                 {QStringLiteral("Error MS"), within / degreesWithin}});
}

Inference zInterval(double deviation, double mean, double count, double level) {
    if (deviation <= 0 || count <= 0 || level <= 0 || level >= 1)
        return {};
    const double critical = special::invNormal(1.0 - (1.0 - level) / 2.0, 0, 1);
    const double margin = critical * deviation / std::sqrt(count);
    return make(QStringLiteral("ZInterval"),
                {{QStringLiteral("lower"), mean - margin},
                 {QStringLiteral("upper"), mean + margin},
                 {QStringLiteral("x̄"), mean},
                 {QStringLiteral("ME"), margin},
                 {QStringLiteral("n"), count}});
}

Inference tInterval(double mean, double sampleDeviation, double count, double level) {
    if (sampleDeviation <= 0 || count <= 1 || level <= 0 || level >= 1)
        return {};
    const double degrees = count - 1;
    const double critical = special::invStudent(1.0 - (1.0 - level) / 2.0, degrees);
    const double margin = critical * sampleDeviation / std::sqrt(count);
    return make(QStringLiteral("TInterval"),
                {{QStringLiteral("lower"), mean - margin},
                 {QStringLiteral("upper"), mean + margin},
                 {QStringLiteral("x̄"), mean},
                 {QStringLiteral("ME"), margin},
                 {QStringLiteral("df"), degrees}});
}

Inference twoSampleTInterval(double meanA, double deviationA, double countA, double meanB,
                             double deviationB, double countB, double level, bool pooled) {
    if (countA <= 1 || countB <= 1 || level <= 0 || level >= 1)
        return {};
    double standardError = 0;
    double degrees = 0;
    if (pooled) {
        const double pooledVariance =
            ((countA - 1) * deviationA * deviationA + (countB - 1) * deviationB * deviationB)
            / (countA + countB - 2);
        standardError = std::sqrt(pooledVariance * (1.0 / countA + 1.0 / countB));
        degrees = countA + countB - 2;
    } else {
        const double a = deviationA * deviationA / countA;
        const double b = deviationB * deviationB / countB;
        standardError = std::sqrt(a + b);
        degrees = (a + b) * (a + b) / (a * a / (countA - 1) + b * b / (countB - 1));
    }
    const double critical = special::invStudent(1.0 - (1.0 - level) / 2.0, degrees);
    const double margin = critical * standardError;
    const double difference = meanA - meanB;
    return make(QStringLiteral("2-SampTInt"),
                {{QStringLiteral("lower"), difference - margin},
                 {QStringLiteral("upper"), difference + margin},
                 {QStringLiteral("df"), degrees},
                 {QStringLiteral("ME"), margin}});
}

Inference onePropZInterval(double successes, double count, double level) {
    if (count <= 0 || level <= 0 || level >= 1)
        return {};
    const double observed = successes / count;
    const double critical = special::invNormal(1.0 - (1.0 - level) / 2.0, 0, 1);
    const double margin = critical * std::sqrt(observed * (1.0 - observed) / count);
    return make(QStringLiteral("1-PropZInt"),
                {{QStringLiteral("lower"), observed - margin},
                 {QStringLiteral("upper"), observed + margin},
                 {QStringLiteral("p̂"), observed},
                 {QStringLiteral("ME"), margin}});
}

Inference twoPropZInterval(double successesA, double countA, double successesB, double countB,
                           double level) {
    if (countA <= 0 || countB <= 0 || level <= 0 || level >= 1)
        return {};
    const double first = successesA / countA;
    const double second = successesB / countB;
    const double critical = special::invNormal(1.0 - (1.0 - level) / 2.0, 0, 1);
    const double margin = critical
        * std::sqrt(first * (1.0 - first) / countA + second * (1.0 - second) / countB);
    const double difference = first - second;
    return make(QStringLiteral("2-PropZInt"),
                {{QStringLiteral("lower"), difference - margin},
                 {QStringLiteral("upper"), difference + margin},
                 {QStringLiteral("p̂1"), first},
                 {QStringLiteral("p̂2"), second},
                 {QStringLiteral("ME"), margin}});
}

}  // namespace stats
