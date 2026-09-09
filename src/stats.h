#pragma once

// The STAT menu's arithmetic: one- and two-variable summaries and the
// regression models. Kept free of Qt widgets so the numbers can be tested
// on their own.

#include <QString>

#include <functional>
#include <vector>

namespace stats {

struct OneVariable {
    double count = 0;
    double sum = 0;
    double sumSquares = 0;
    double mean = 0;
    double sampleDeviation = 0;
    double populationDeviation = 0;
    double minimum = 0;
    double lowerQuartile = 0;
    double median = 0;
    double upperQuartile = 0;
    double maximum = 0;
};

struct TwoVariable {
    double count = 0;
    double sumX = 0;
    double sumY = 0;
    double sumXSquares = 0;
    double sumYSquares = 0;
    double sumXY = 0;
    double meanX = 0;
    double meanY = 0;
    double sampleDeviationX = 0;
    double populationDeviationX = 0;
    double sampleDeviationY = 0;
    double populationDeviationY = 0;
    double minimumX = 0;
    double maximumX = 0;
    double minimumY = 0;
    double maximumY = 0;
};

enum class Model {
    Linear,        // y = ax + b
    LinearAB,      // y = a + bx
    Quadratic,
    Cubic,
    Quartic,
    Logarithmic,   // y = a + b ln x
    Exponential,   // y = a b^x
    Power,         // y = a x^b
    Logistic,      // y = c / (1 + a e^(-bx))
    Sinusoidal,    // y = a sin(bx + c) + d
    MedianMedian,
};

struct Regression {
    bool valid = false;
    QString name;
    QString equation;
    std::vector<double> coefficients;  // in the order the equation reads
    double correlation = 0;            // r, where the model has one
    double determination = 0;          // r²
    bool hasCorrelation = false;
};

OneVariable oneVariable(const std::vector<double> &values, const std::vector<double> &frequencies);
TwoVariable twoVariable(const std::vector<double> &x, const std::vector<double> &y);
Regression fit(Model model, const std::vector<double> &x, const std::vector<double> &y);

// Solves a small dense system; exposed because the polynomial fits and the
// grapher both need it.
bool solveLinearSystem(std::vector<std::vector<double>> matrix, std::vector<double> &solution);

}  // namespace stats
