#pragma once

// Special functions behind the DISTR menu. Kept apart from the expression
// engine so the numerics can be tested on their own.

namespace special {

double erfValue(double x);
double erfcValue(double x);

// Regularised incomplete gamma and beta, the two workhorses every continuous
// distribution here is built from.
double gammaP(double a, double x);
double gammaQ(double a, double x);
double betaI(double a, double b, double x);
double logGamma(double x);

double normalPdf(double x, double mean, double deviation);
double normalCdf(double lower, double upper, double mean, double deviation);
double invNormal(double area, double mean, double deviation);

double studentPdf(double x, double degrees);
double studentCdf(double lower, double upper, double degrees);
double invStudent(double area, double degrees);

double chiSquarePdf(double x, double degrees);
double chiSquareCdf(double lower, double upper, double degrees);

double fPdf(double x, double numerator, double denominator);
double fCdf(double lower, double upper, double numerator, double denominator);

double binomialPdf(double trials, double probability, double successes);
double binomialCdf(double trials, double probability, double successes);
double poissonPdf(double mean, double count);
double poissonCdf(double mean, double count);
double geometricPdf(double probability, double trial);
double geometricCdf(double probability, double trial);

}  // namespace special
