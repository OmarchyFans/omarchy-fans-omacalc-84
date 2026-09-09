#include "stats.h"

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
