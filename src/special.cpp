#include "special.h"

#include <cmath>
#include <limits>

namespace special {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 3.0e-14;
constexpr int kMaxIterations = 400;
constexpr double kTiny = 1.0e-300;
}  // namespace

double logGamma(double x) { return std::lgamma(x); }

double erfValue(double x) { return std::erf(x); }
double erfcValue(double x) { return std::erfc(x); }

// Series expansion, good for x below a+1.
static double gammaSeries(double a, double x) {
    double ap = a;
    double sum = 1.0 / a;
    double term = sum;
    for (int i = 0; i < kMaxIterations; ++i) {
        ap += 1.0;
        term *= x / ap;
        sum += term;
        if (std::abs(term) < std::abs(sum) * kEpsilon)
            break;
    }
    return sum * std::exp(-x + a * std::log(x) - logGamma(a));
}

// Continued fraction, good for x above a+1.
static double gammaContinuedFraction(double a, double x) {
    double b = x + 1.0 - a;
    double c = 1.0 / kTiny;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= kMaxIterations; ++i) {
        const double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::abs(d) < kTiny)
            d = kTiny;
        c = b + an / c;
        if (std::abs(c) < kTiny)
            c = kTiny;
        d = 1.0 / d;
        const double delta = d * c;
        h *= delta;
        if (std::abs(delta - 1.0) < kEpsilon)
            break;
    }
    return h * std::exp(-x + a * std::log(x) - logGamma(a));
}

double gammaP(double a, double x) {
    if (x <= 0.0 || a <= 0.0)
        return 0.0;
    if (x < a + 1.0)
        return gammaSeries(a, x);
    return 1.0 - gammaContinuedFraction(a, x);
}

double gammaQ(double a, double x) { return 1.0 - gammaP(a, x); }

static double betaContinuedFraction(double a, double b, double x) {
    const double qab = a + b;
    const double qap = a + 1.0;
    const double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::abs(d) < kTiny)
        d = kTiny;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= kMaxIterations; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < kTiny)
            d = kTiny;
        c = 1.0 + aa / c;
        if (std::abs(c) < kTiny)
            c = kTiny;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < kTiny)
            d = kTiny;
        c = 1.0 + aa / c;
        if (std::abs(c) < kTiny)
            c = kTiny;
        d = 1.0 / d;
        const double delta = d * c;
        h *= delta;
        if (std::abs(delta - 1.0) < kEpsilon)
            break;
    }
    return h;
}

double betaI(double a, double b, double x) {
    if (x <= 0.0)
        return 0.0;
    if (x >= 1.0)
        return 1.0;
    const double front = std::exp(logGamma(a + b) - logGamma(a) - logGamma(b)
                                  + a * std::log(x) + b * std::log(1.0 - x));
    if (x < (a + 1.0) / (a + b + 2.0))
        return front * betaContinuedFraction(a, b, x) / a;
    return 1.0 - front * betaContinuedFraction(b, a, 1.0 - x) / b;
}

double normalPdf(double x, double mean, double deviation) {
    if (deviation <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double z = (x - mean) / deviation;
    return std::exp(-0.5 * z * z) / (deviation * std::sqrt(2.0 * kPi));
}

static double standardNormalCdf(double z) { return 0.5 * erfcValue(-z / std::sqrt(2.0)); }

double normalCdf(double lower, double upper, double mean, double deviation) {
    if (deviation <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double a = (lower - mean) / deviation;
    const double b = (upper - mean) / deviation;
    return standardNormalCdf(b) - standardNormalCdf(a);
}

// Acklam's rational approximation, then two Halley steps to bring it to
// full double precision.
double invNormal(double area, double mean, double deviation) {
    if (area <= 0.0 || area >= 1.0)
        return std::numeric_limits<double>::quiet_NaN();

    static const double a[6] = {-3.969683028665376e+01, 2.209460984245205e+02,
                                -2.759285104469687e+02, 1.383577518672690e+02,
                                -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[5] = {-5.447609879822406e+01, 1.615858368580409e+02,
                                -1.556989798598866e+02, 6.680131188771972e+01,
                                -1.328068155288572e+01};
    static const double c[6] = {-7.784894002430293e-03, -3.223964580411365e-01,
                                -2.400758277161838e+00, -2.549732539343734e+00,
                                4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[4] = {7.784695709041462e-03, 3.224671290700398e-01,
                                2.445134137142996e+00, 3.754408661907416e+00};

    const double pLow = 0.02425;
    double z;
    if (area < pLow) {
        const double q = std::sqrt(-2.0 * std::log(area));
        z = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5])
            / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (area <= 1.0 - pLow) {
        const double q = area - 0.5;
        const double r = q * q;
        z = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q
            / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - area));
        z = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5])
            / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }

    for (int i = 0; i < 2; ++i) {
        const double error = standardNormalCdf(z) - area;
        const double density = std::exp(-0.5 * z * z) / std::sqrt(2.0 * kPi);
        if (density <= 0.0)
            break;
        const double u = error / density;
        z -= u / (1.0 + 0.5 * z * u);
    }
    return mean + deviation * z;
}

double studentPdf(double x, double degrees) {
    if (degrees <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double norm = std::exp(logGamma((degrees + 1.0) / 2.0) - logGamma(degrees / 2.0))
        / std::sqrt(degrees * kPi);
    return norm * std::pow(1.0 + x * x / degrees, -(degrees + 1.0) / 2.0);
}

static double studentCdfLower(double x, double degrees) {
    const double p = betaI(degrees / 2.0, 0.5, degrees / (degrees + x * x));
    return x >= 0.0 ? 1.0 - 0.5 * p : 0.5 * p;
}

double studentCdf(double lower, double upper, double degrees) {
    if (degrees <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return studentCdfLower(upper, degrees) - studentCdfLower(lower, degrees);
}

double invStudent(double area, double degrees) {
    if (area <= 0.0 || area >= 1.0 || degrees <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    // Bracket generously, then bisect: the cdf is monotone, so this always
    // converges and never depends on a good starting guess.
    double low = -1.0e4;
    double high = 1.0e4;
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (low + high);
        if (studentCdfLower(mid, degrees) < area)
            low = mid;
        else
            high = mid;
    }
    return 0.5 * (low + high);
}

double chiSquarePdf(double x, double degrees) {
    if (x < 0.0 || degrees <= 0.0)
        return 0.0;
    const double half = degrees / 2.0;
    return std::exp((half - 1.0) * std::log(x) - x / 2.0 - half * std::log(2.0) - logGamma(half));
}

double chiSquareCdf(double lower, double upper, double degrees) {
    if (degrees <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double a = gammaP(degrees / 2.0, std::max(0.0, lower) / 2.0);
    const double b = gammaP(degrees / 2.0, std::max(0.0, upper) / 2.0);
    return b - a;
}

double fPdf(double x, double numerator, double denominator) {
    if (x < 0.0 || numerator <= 0.0 || denominator <= 0.0)
        return 0.0;
    const double n = numerator;
    const double m = denominator;
    const double logValue = 0.5 * n * std::log(n) + 0.5 * m * std::log(m)
        + (0.5 * n - 1.0) * std::log(x) - 0.5 * (n + m) * std::log(m + n * x)
        + logGamma(0.5 * (n + m)) - logGamma(0.5 * n) - logGamma(0.5 * m);
    return std::exp(logValue);
}

static double fCdfLower(double x, double n, double m) {
    if (x <= 0.0)
        return 0.0;
    return betaI(0.5 * n, 0.5 * m, n * x / (n * x + m));
}

double fCdf(double lower, double upper, double numerator, double denominator) {
    if (numerator <= 0.0 || denominator <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return fCdfLower(upper, numerator, denominator) - fCdfLower(lower, numerator, denominator);
}

double binomialPdf(double trials, double probability, double successes) {
    const double n = std::round(trials);
    const double k = std::round(successes);
    if (k < 0.0 || k > n || probability < 0.0 || probability > 1.0)
        return 0.0;
    if (probability == 0.0)
        return k == 0.0 ? 1.0 : 0.0;
    if (probability == 1.0)
        return k == n ? 1.0 : 0.0;
    const double logValue = logGamma(n + 1.0) - logGamma(k + 1.0) - logGamma(n - k + 1.0)
        + k * std::log(probability) + (n - k) * std::log(1.0 - probability);
    return std::exp(logValue);
}

double binomialCdf(double trials, double probability, double successes) {
    const double n = std::round(trials);
    const double k = std::round(successes);
    if (k < 0.0)
        return 0.0;
    if (k >= n)
        return 1.0;
    double total = 0.0;
    for (double i = 0.0; i <= k; i += 1.0)
        total += binomialPdf(n, probability, i);
    return total > 1.0 ? 1.0 : total;
}

double poissonPdf(double mean, double count) {
    const double k = std::round(count);
    if (k < 0.0 || mean < 0.0)
        return 0.0;
    return std::exp(-mean + k * std::log(mean == 0.0 ? 1.0 : mean) - logGamma(k + 1.0))
        * (mean == 0.0 ? (k == 0.0 ? 1.0 : 0.0) : 1.0);
}

double poissonCdf(double mean, double count) {
    const double k = std::round(count);
    if (k < 0.0)
        return 0.0;
    return gammaQ(k + 1.0, mean);
}

double geometricPdf(double probability, double trial) {
    const double k = std::round(trial);
    if (k < 1.0 || probability <= 0.0 || probability > 1.0)
        return 0.0;
    return probability * std::pow(1.0 - probability, k - 1.0);
}

double geometricCdf(double probability, double trial) {
    const double k = std::round(trial);
    if (k < 1.0 || probability <= 0.0 || probability > 1.0)
        return 0.0;
    return 1.0 - std::pow(1.0 - probability, k);
}

}  // namespace special
