// This file is part of the SpeedCrunch project
// Copyright (C) 2004-2006 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2007-2009, 2013, 2016 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#include "core/evaluator.h"
#include "core/numberformatter.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/unicodechars.h"
#include "gui/editorutils.h"
#include "gui/functiontooltiputils.h"
#include "gui/simplifiedexpressionutils.h"
#include "tests/testcommon.h"

#include <QtCore/QCoreApplication>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

typedef Quantity::Format Format;

static Evaluator* eval = 0;
static int eval_total_tests = 0;
static int eval_failed_tests = 0;
static int eval_new_failed_tests = 0;
static const QString space(UnicodeChars::MediumMathematicalSpace);
static const QString slash = space + QStringLiteral("/") + space;

#define CHECK_AUTOFIX(s,p) checkAutoFix(__FILE__,__LINE__,#s,s,p)
#define CHECK_DIV_BY_ZERO(s) checkDivisionByZero(__FILE__,__LINE__,#s,s)
#define CHECK_EVAL(x,y) checkEval(__FILE__,__LINE__,#x,x,y)
#define CHECK_EVAL_FORMAT(x,y) checkEval(__FILE__,__LINE__,#x,x,y,0,false,true)
#define CHECK_EVAL_FORMAT_HAS_SLASH(x) checkEvalFormatSlash(__FILE__, __LINE__, #x, x, true)
#define CHECK_EVAL_FORMAT_NO_SLASH(x) checkEvalFormatSlash(__FILE__, __LINE__, #x, x, false)
#define CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH(x) checkEvalFormatMediumSpacedSlash(__FILE__, __LINE__, #x, x)
#define CHECK_EVAL_FORMAT_EXACT(x,y) checkEvalFormatExact(__FILE__, __LINE__, #x, x, y)
#define CHECK_EVAL_KNOWN_ISSUE(x,y,n) checkEval(__FILE__,__LINE__,#x,x,y,n)
#define CHECK_EVAL_PRECISE(x,y) checkEvalPrecise(__FILE__,__LINE__,#x,x,y)
#define CHECK_EVAL_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true)
#define CHECK_EVAL_FORMAT_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true,true)
#define CHECK_INTERPRETED(x,y) checkInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_DISPLAY_INTERPRETED(x,y) checkDisplayInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(x,y) checkDisplaySimplifiedInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_USERFUNC_SET(x) checkEval(__FILE__,__LINE__,#x,x,"NaN")
#define CHECK_USERFUNC_SET_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true)
#define CHECK_USERFUNC_DESC(n,d) checkUserFunctionDescription(__FILE__,__LINE__,#n,n,d)
#define CHECK_USERFUNC_INTERPRETED(n,e) checkUserFunctionInterpreted(__FILE__,__LINE__,#n,n,e)
#define CHECK_USERVAR_DESC(n,d) checkUserVariableDescription(__FILE__,__LINE__,#n,n,d)

static void checkAutoFix(const char* file, int line, const char* msg, const char* expr, const char* fixed)
{
    ++eval_total_tests;

    string r = eval->autoFix(QString(expr)).toStdString();
    DisplayErrorOnMismatch(file, line, msg, r, fixed, eval_failed_tests, eval_new_failed_tests);
}

static void checkDivisionByZero(const char* file, int line, const char* msg, const QString& expr)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (eval->error().isEmpty()) {
        ++eval_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << endl
             << "\tError: " << "division by zero not caught" << endl;
    }
}

static void checkEval(const char* file, int line, const char* msg, const QString& expr,
                      const char* expected, int issue = 0, bool shouldFail = false, bool format = false)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        if (!shouldFail) {
            ++eval_failed_tests;
            cerr << file << "[" << line << "]\t" << msg;
            if (issue)
                cerr << "\t[ISSUE " << issue << "]";
            else {
                cerr << "\t[NEW]";
                ++eval_new_failed_tests;
            }
            cerr << endl;
            cerr << "\tError: " << qPrintable(eval->error()) << endl;
        }
    } else {
        QString result = (format ? NumberFormatter::format(rn) : DMath::format(rn, Format::Fixed()));
        result.replace(QString::fromUtf8("−"), "-");
        if (shouldFail || result != expected) {
            ++eval_failed_tests;
            cerr << file << "[" << line << "]\t" << msg;
            if (issue)
                cerr << "\t[ISSUE " << issue << "]";
            else {
                cerr << "\t[NEW]";
                ++eval_new_failed_tests;
            }
            cerr << endl;
            cerr << "\tResult   : " << result.toLatin1().constData() << endl
                 << "\tExpected : " << (shouldFail ? "should fail" : expected) << endl;
        }
    }
}

static void checkEvalFormatSlash(const char* file, int line, const char* msg,
                                 const QString& expr, bool shouldHaveSlash)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    const bool hasSlash = result.contains('/');
    if (hasSlash != shouldHaveSlash) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult      : " << result.toLatin1().constData() << endl
             << "\tExpected '/' : " << (shouldHaveSlash ? "yes" : "no") << endl;
    }
}

static void checkEvalFormatMediumSpacedSlash(const char* file, int line, const char* msg,
                                             const QString& expr)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    const QString mediumSlash = slash;
    if (!result.contains(mediumSlash)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult                 : " << result.toLatin1().constData() << endl
             << "\tExpected medium slash  : " << mediumSlash.toLatin1().constData() << endl;
    }
}

static void checkEvalFormatExact(const char* file, int line, const char* msg,
                                 const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    if (result != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult   : " << result.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }
}

static void checkEvalPrecise(const char* file, int line, const char* msg, const QString& expr, const char* expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    // We compare up to 50 decimals, not exact number because it's often difficult
    // to represent the result as an irrational number, e.g. PI.
    string result = DMath::format(rn, Format::Fixed() + Format::Precision(50)).toStdString();
    DisplayErrorOnMismatch(file, line, msg, result, expected, eval_failed_tests, eval_new_failed_tests, 0);
}

static void checkInterpreted(const char* file, int line, const char* msg, const QString& expr,
                             const char* expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    if (interpreted != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tInterpreted: " << interpreted.toLatin1().constData() << endl
             << "\tExpected   : " << expected << endl;
    }
}

static string toCodePointList(const QString& text)
{
    const QVector<uint> cps = text.toUcs4();
    std::ostringstream os;
    for (int i = 0; i < cps.size(); ++i) {
        if (i > 0)
            os << " ";
        os << "U+"
           << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
           << cps.at(i);
    }
    return os.str();
}

static void checkDisplayInterpreted(const char* file, int line, const char* msg,
                                    const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    const QString displayed = Evaluator::formatInterpretedExpressionForDisplay(interpreted);
    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tDisplayed : " << displayed.toUtf8().constData() << endl
             << "\tExpected  : " << expected.toUtf8().constData() << endl
             << "\tDisplayed code points: " << toCodePointList(displayed) << endl
             << "\tExpected  code points: " << toCodePointList(expected) << endl;
    }
}

static void checkDisplaySimplifiedInterpreted(const char* file, int line, const char* msg,
                                              const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    const QString displayed = Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpreted);
    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tDisplayed : " << displayed.toUtf8().constData() << endl
             << "\tExpected  : " << expected.toUtf8().constData() << endl
             << "\tDisplayed code points: " << toCodePointList(displayed) << endl
             << "\tExpected  code points: " << toCodePointList(expected) << endl;
    }
}

static void checkSuppressSimplifiedExpressionLine(const char* file, int line, const char* msg,
                                                  const QString& expr, bool expectedSuppressed)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted =
        Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression());
    const QString simplified =
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(eval->interpretedExpression());
    const bool suppressed = SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
        interpreted, simplified);
    if (suppressed != expectedSuppressed) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
             << "\tSimplified : " << simplified.toUtf8().constData() << endl
             << "\tSuppressed : " << (suppressed ? "true" : "false") << endl
             << "\tExpected   : " << (expectedSuppressed ? "true" : "false") << endl;
    }
}

static void checkUserFunctionDescription(const char* file, int line, const char* msg,
                                         const QString& functionName,
                                         const QString& expectedDescription)
{
    ++eval_total_tests;

    const auto userFunctions = eval->getUserFunctions();
    for (const auto& function : userFunctions) {
        if (function.name() == functionName) {
            if (function.description() != expectedDescription) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
                cerr << "\tDescription: " << function.description().toLatin1().constData() << endl
                     << "\tExpected   : " << expectedDescription.toLatin1().constData() << endl;
            }
            return;
        }
    }

    ++eval_failed_tests;
    ++eval_new_failed_tests;
    cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
    cerr << "\tError: user function not found: " << functionName.toLatin1().constData() << endl;
}

static void checkUserFunctionInterpreted(const char* file, int line, const char* msg,
                                         const QString& functionName,
                                         const QString& expectedInterpreted)
{
    ++eval_total_tests;

    const auto userFunctions = eval->getUserFunctions();
    for (const auto& function : userFunctions) {
        if (function.name() == functionName) {
            if (function.interpretedExpression() != expectedInterpreted) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
                cerr << "\tInterpreted: "
                     << function.interpretedExpression().toUtf8().constData() << endl
                     << "\tExpected   : " << expectedInterpreted.toUtf8().constData() << endl;
            }
            return;
        }
    }

    ++eval_failed_tests;
    ++eval_new_failed_tests;
    cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
    cerr << "\tError: user function not found: " << functionName.toLatin1().constData() << endl;
}

static void checkUserVariableDescription(const char* file, int line, const char* msg,
                                         const QString& variableName,
                                         const QString& expectedDescription)
{
    ++eval_total_tests;

    if (!eval->hasVariable(variableName)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tError: user variable not found: " << variableName.toLatin1().constData() << endl;
        return;
    }

    const auto variable = eval->getVariable(variableName);
    if (variable.description() != expectedDescription) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tDescription: " << variable.description().toLatin1().constData() << endl
             << "\tExpected   : " << expectedDescription.toLatin1().constData() << endl;
    }
}

void test_constants()
{
    CHECK_EVAL("1", "1");
    CHECK_EVAL_FAIL("pi()");
    CHECK_EVAL_FAIL("e()");
    CHECK_EVAL_FAIL("ans()");
    CHECK_EVAL_FAIL("sqrt(pi())");
    CHECK_EVAL_FAIL("cos(pi()");
}

void test_exponentiation()
{
    CHECK_EVAL("2e10", "20000000000");
    CHECK_EVAL("2E10", "20000000000");
    CHECK_EVAL("2e-10", "0.0000000002");
    CHECK_EVAL("2E0xa", "20000000000");
    CHECK_EVAL("2e-0xa", "0.0000000002");
    CHECK_EVAL("0b10b10", "8");
    CHECK_EVAL("0b10B10", "8");
    CHECK_EVAL("0b10b-10", "0.5");
    CHECK_EVAL("0b10b0d2", "8");
    CHECK_EVAL("0b10b-0d2", "0.5");
    CHECK_EVAL("0o2o10", "33554432");
    CHECK_EVAL("0o2O10", "33554432");
    CHECK_EVAL("0o2C10", "33554432");
    CHECK_EVAL("0o2o-10", "0.00000011920928955078");
    CHECK_EVAL("0o2o0d8", "33554432");
    CHECK_EVAL("0o2o-0d8", "0.00000011920928955078");
    CHECK_EVAL("0x2h10", "36893488147419103232");
    CHECK_EVAL("0x2H10", "36893488147419103232");
    CHECK_EVAL("0x2h-10", "0.00000000000000000011");
    CHECK_EVAL("0x2h0d16", "36893488147419103232");
    CHECK_EVAL("0x2h-0d16", "0.00000000000000000011");
    CHECK_EVAL_FAIL("0x2o11");
    CHECK_EVAL_FAIL("0b2C11");
    CHECK_EVAL_FAIL("0o2b11");
}

void test_unary()
{
    CHECK_EVAL("-0", "0");
    CHECK_EVAL("-1", "-1");
    CHECK_EVAL("--1", "1");
    CHECK_EVAL("---1", "-1");
    CHECK_EVAL("----1", "1");
    CHECK_EVAL("-----1", "-1");
    CHECK_EVAL("------1", "1");

    CHECK_EVAL("-ABS(1)", "-1");
    CHECK_EVAL("-ABS(-2)", "-2");
    CHECK_EVAL("--ABS(-3)", "3");
    CHECK_EVAL("---ABS(-4)", "-4");

    // Operator ^ has higher precedence than unary minus.
    CHECK_EVAL("-1^0", "-1");
    CHECK_EVAL("-1^1", "-1");
    CHECK_EVAL("-1^2", "-1");
    CHECK_EVAL("-1^3", "-1");

    CHECK_EVAL("-2^0", "-1");
    CHECK_EVAL("-2^1", "-2");
    CHECK_EVAL("-2^2", "-4");
    CHECK_EVAL("-2^3", "-8");
    CHECK_EVAL("-2^4", "-16");
    CHECK_EVAL("-2^5", "-32");
    CHECK_EVAL("-2^6", "-64");
    CHECK_EVAL("-2^7", "-128");
    CHECK_EVAL("-2^8", "-256");
    CHECK_EVAL("-2^9", "-512");
    CHECK_EVAL("-2^10", "-1024");

    CHECK_EVAL("(5-7)^2",  "4");
    CHECK_EVAL("-(5-7)^2", "-4");

    CHECK_EVAL("-cos(pi)^3", "1");
    CHECK_EVAL("1*(-cos(pi)^2)", "-1");

    CHECK_EVAL("3^3^3", "7625597484987");

    CHECK_EVAL("1/-1^2", "-1");
    CHECK_EVAL("1*-1^2", "-1");

    // Factorial has higher precedence than unary minus.
    CHECK_EVAL("-1!", "-1");
    CHECK_EVAL("-2!", "-2");
    CHECK_EVAL("-3!", "-6");

    // Unicode minus sign
    CHECK_EVAL("−1", "-1");
    CHECK_EVAL("−(2*3)", "-6");
    CHECK_EVAL("2*−1", "-2");
    CHECK_EVAL("2e−1", "0.2");

    CHECK_INTERPRETED("−pi", "-pi");
    CHECK_INTERPRETED("--pi", "pi");
    CHECK_INTERPRETED("---pi", "-pi");
    CHECK_INTERPRETED("----pi", "pi");
}

void test_binary()
{
    // See http://en.wikipedia.org/wiki/Empty_product.
    CHECK_EVAL("0^0", "NaN");

    CHECK_EVAL("1^0", "1");
    CHECK_EVAL("1^1", "1");
    CHECK_EVAL("1^2", "1");
    CHECK_EVAL("1^3", "1");

    CHECK_EVAL("2^0", "1");
    CHECK_EVAL("2^1", "2");
    CHECK_EVAL("2^2", "4");
    CHECK_EVAL("2^3", "8");
    CHECK_EVAL("2^4", "16");
    CHECK_EVAL("2^5", "32");
    CHECK_EVAL("2^6", "64");
    CHECK_EVAL("2^7", "128");
    CHECK_EVAL("2^8", "256");
    CHECK_EVAL("2^9", "512");
    CHECK_EVAL("2^10", "1024");
    CHECK_EVAL("(-13)^(1/3)", "-2.3513346877207574895");
    CHECK_EVAL("(-13)^(1/9)", "-1.32975454563978597291");
    CHECK_EVAL("(-13)^(1/27)", "-1.0996567925533755264");
    CHECK_EVAL("(-13)^(1/3.123)", "2.27347485100383238372");
    CHECK_EVAL("(-13)^(1/9.123)", "1.32465488702830785593");
    CHECK_EVAL("(-13)^(1/27.123)", "1.09918315510346486963");

    CHECK_EVAL("0+0", "0");
    CHECK_EVAL("1+0", "1");
    CHECK_EVAL("0+1", "1");
    CHECK_EVAL("1+1", "2");

    CHECK_EVAL("0-0", "0");
    CHECK_EVAL("1-0", "1");
    CHECK_EVAL("0-1", "-1");
    CHECK_EVAL("1-1", "0");

    CHECK_EVAL("0−0", "0");
    CHECK_EVAL("1−0", "1");
    CHECK_EVAL("0−1", "-1");
    CHECK_EVAL("1−1", "0");

    CHECK_EVAL("2*3", "6");
    CHECK_EVAL("2×3", "6");
    CHECK_EVAL("2⋅3", "6"); // U+22C5 Dot operator.
    CHECK_EVAL("3*2", "6");
    CHECK_EVAL("3×2", "6");
    CHECK_EVAL("3⋅2", "6");

    CHECK_EVAL("10/2", "5");
    CHECK_EVAL("10÷2", "5");
    CHECK_EVAL("10⧸2", "5"); // U+29F8 Big solidus.
    CHECK_EVAL("2/10", "0.2");
    CHECK_EVAL("2÷10", "0.2");
    CHECK_EVAL("2⧸10", "0.2");
    CHECK_EVAL("1/(1/(1/0.2425-4)-8)-12", "0");
    CHECK_EVAL("1/(1/(1/(2425/10000)-4)-8)-12", "0");
    CHECK_EVAL("1/(1/(1/(1/(1/(1/0.25636-3)-1)-9)-12)-1)-48", "0");
    CHECK_EVAL("1/(1/(1/(1/(1/(1/(25636/100000)-3)-1)-9)-12)-1)-48", "0");

    // Check that parentheses are added in unit conversion results when needed
    CHECK_EVAL("1 meter -> 10 meter", "0.1 (10 meter)");
    CHECK_EVAL("1 meter -> .1 meter", "10 (.1 meter)");
    CHECK_EVAL("1 meter -> -1 meter", "-1 (-1 meter)");
    CHECK_EVAL("1 meter -> 0xa meter", "0.1 (0xa meter)");
    CHECK_EVAL("1 meter second -> 10 meter second", "0.1 (10 meter second)");
    CHECK_EVAL("1 meter second -> meter 10 second", "0.1 meter 10 second");
    CHECK_EVAL("1 meter second -> meter second 10", "0.1 (meter second 10)");
    CHECK_EVAL("1 meter -> meter + meter", "0.5 (meter + meter)");
    CHECK_EVAL("1 meter -> meter - 2meter", "-1 (meter - 2meter)");
    CHECK_EVAL("1 meter -> meter", "1 meter");
    CHECK_EVAL("1 (10 meter) -> meter", "10 meter");
    CHECK_EVAL("1 meter in meter", "1 meter");
    CHECK_EVAL("1 meter IN meter", "1 meter");
    CHECK_EVAL("50 yard + 2 foot in centi meter", "4632.96 centi meter");
    CHECK_EVAL("10 meter in (1 yard + 2 foot)", "6.56167979002624671916 (1 yard + 2 foot)");
    CHECK_EVAL(QString::fromUtf8("1 meter −> centi meter"), "100 centi meter");
}

void test_bitwise_complement_operator()
{
    CHECK_EVAL("~~~~1", "1");
    CHECK_EVAL("~-~1", "-3");
    CHECK_EVAL("-~-1", "0");
    CHECK_EVAL("~~-1", "-1");
    CHECK_EVAL("(~2)^3", "-27");
    CHECK_EVAL("~(2^3)", "-9");
    CHECK_EVAL("~2^3", "-9");
    CHECK_EVAL("~(-5)", "4");
    CHECK_EVAL("-~(-1)", "0");
    CHECK_EVAL("-5 & 3", "3");
    CHECK_EVAL("-5 | 3", "-5");
    CHECK_EVAL("-5 ^ 3", "-125");
    CHECK_EVAL("~0 >> 1", "-1");
    CHECK_EVAL("~0 << 1", "-2");
    CHECK_EVAL("-1 >> 1", "-1");
    CHECK_EVAL("-8 >> 2", "-2");
    CHECK_EVAL("-9 >> 2", "-3");
    CHECK_EVAL("~1 & 1", "0");
    CHECK_EVAL("~1 | 1", "-1");
    CHECK_EVAL("1 | 2 & 3", "3");
    CHECK_EVAL("(1 | 2) & 3", "3");
    CHECK_EVAL("1 | (2 & 3)", "3");
    CHECK_EVAL("2*~3", "-8");
    CHECK_EVAL("~3*2", "-8");
    CHECK_EVAL("~(3*2)", "-7");
    CHECK_EVAL("~2^3&7", "7");
    CHECK_EVAL("~(2^3&7)", "-1");
    CHECK_EVAL("(~2^3)&7", "7");
    CHECK_EVAL("~(~(~1))", "-2");
    CHECK_EVAL("~(~(~(~1)))", "1");
    CHECK_EVAL("~5", "-6");
    CHECK_EVAL("~(~5)", "5");
    CHECK_EVAL("~(~(~5))", "-6");
    CHECK_EVAL("~(2^3) + ~3! * (~1 & 3) << 1", "-46");

    // Regression: keep unary minus and bitwise complement/function-not consistent.
    CHECK_EVAL("not(-1)", "0");
    CHECK_EVAL("~(-1)", "0");
    CHECK_EVAL("-not(-1)", "0");
    CHECK_EVAL("-~(-1)", "0");

    CHECK_EVAL("not(not(not(not(1))))", "1");
    CHECK_EVAL("not(-not(1))", "-3");
    CHECK_EVAL("-not(-1)", "0");
    CHECK_EVAL("not(not(-1))", "-1");
    CHECK_EVAL("(not(2))^3", "-27");
    CHECK_EVAL("not(2^3)", "-9");
    CHECK_EVAL("not(2^3)", "-9");
    CHECK_EVAL("not(-5)", "4");
    CHECK_EVAL("not(0) >> 1", "-1");
    CHECK_EVAL("not(0) << 1", "-2");
    CHECK_EVAL("not(1) & 1", "0");
    CHECK_EVAL("not(1) | 1", "-1");
    CHECK_EVAL("2*not(3)", "-8");
    CHECK_EVAL("not(3)*2", "-8");
    CHECK_EVAL("not(3*2)", "-7");
    CHECK_EVAL("not(2^3&7)", "-1");
    CHECK_EVAL("not(2^3&7)", "-1");
    CHECK_EVAL("(not(2^3))&7", "7");
    CHECK_EVAL("not(not(not(1)))", "-2");
    CHECK_EVAL("not(not(not(not(1))))", "1");
    CHECK_EVAL("not(5)", "-6");
    CHECK_EVAL("not(not(5))", "5");
    CHECK_EVAL("not(not(not(5)))", "-6");
    CHECK_EVAL("not(2^3) + not(3!) * (not(1) & 3) << 1", "-46");
}

void test_divide_by_zero()
{
    CHECK_DIV_BY_ZERO("1/0");
    CHECK_DIV_BY_ZERO("1 / sin 0");
    CHECK_DIV_BY_ZERO("1/sin(pi)");
    CHECK_DIV_BY_ZERO("1/sin (pi");
    CHECK_DIV_BY_ZERO("1/sin  pi");
    CHECK_DIV_BY_ZERO("1 / cos (pi/2)");
    CHECK_DIV_BY_ZERO("1 / cos(pi/2");
    CHECK_DIV_BY_ZERO("1/cos(pi/2)");
    CHECK_DIV_BY_ZERO("1/tan(0)");
    CHECK_DIV_BY_ZERO("1/tan 0");
    CHECK_DIV_BY_ZERO("-1/trunc(0.123)");
    CHECK_DIV_BY_ZERO("1/round(0.456)");
    CHECK_DIV_BY_ZERO("-1/binompmf(1;10;0)");
    CHECK_DIV_BY_ZERO("-345.3 / binompmf (1; 10; 0)");
    CHECK_DIV_BY_ZERO("mod(1;0)");
    CHECK_DIV_BY_ZERO("emod(1;0)");
    CHECK_DIV_BY_ZERO("powmod(2;10;0)");
}

void test_radix_char()
{
    // Backup current settings
    Settings* settings = Settings::instance();
    char radixCharacter = settings->radixCharacter();

    settings->setRadixCharacter('*');

    CHECK_EVAL("1+.5", "1.5");
    CHECK_EVAL("1+,5", "1.5");
    CHECK_EVAL(".5*,5", "0.25");
    CHECK_EVAL("1.01+2,02", "3.03");

    CHECK_EVAL("0b1.0 + 1", "2");
    CHECK_EVAL("0b1.1 * 1", "1.5");
    CHECK_EVAL("0b01.010 / 1", "1.25");
    CHECK_EVAL("-0b.010 - 1", "-1.25");

    CHECK_EVAL("0o.7 + 1", "1.875");
    CHECK_EVAL("-0o.7 + 1", "0.125");

    CHECK_EVAL("0x.f + 1", "1.9375");
    CHECK_EVAL("-0x.f + 1", "0.0625");

    CHECK_EVAL("1/.1", "10"); // ISSUE 151
    CHECK_EVAL("1/,1", "10"); // ISSUE 151

    // Test automatic detection of radix point when multiple choices are possible
    CHECK_EVAL("1,234.567", "1234.567");
    CHECK_EVAL("1.234,567", "1234.567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "1234567.89");
    CHECK_EVAL("1.234.567,89", "1234567.89");
    CHECK_EVAL("1,234.567,89", "1234.56789");
    CHECK_EVAL("1.234,567.89", "1234.56789");

    settings->setRadixCharacter('.');

    CHECK_EVAL("1+0.5", "1.5");
    CHECK_EVAL("1+0,5", "6");
    CHECK_EVAL("1/.1", "10");
    CHECK_EVAL("1/,1", "1");
    CHECK_EVAL("1,234.567", "1234.567");
    CHECK_EVAL("1.234,567", "1.234567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "1234567.89");
    CHECK_EVAL("1.234.567,89", "123456789");
    CHECK_EVAL("1,234.567,89", "1234.56789");
    CHECK_EVAL("1.234,567.89", "123456789");

    settings->setRadixCharacter(',');

    CHECK_EVAL("1+0.5", "6");
    CHECK_EVAL("1+0,5", "1.5");
    CHECK_EVAL("1/.1", "1");
    CHECK_EVAL("1/,1", "10");
    CHECK_EVAL("1,234.567", "1.234567");
    CHECK_EVAL("1.234,567", "1234.567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "123456789");
    CHECK_EVAL("1.234.567,89", "1234567.89");
    CHECK_EVAL("1,234.567,89", "123456789");
    CHECK_EVAL("1.234,567.89", "1234.56789");

    // Restore old settings
    settings->setRadixCharacter(radixCharacter);
}

void test_thousand_sep()
{
    // Common separators.
    CHECK_EVAL("12_345.678_9", "12345.6789");
    CHECK_EVAL("1234_5.67_89", "12345.6789");
    CHECK_EVAL("1234_56", "123456");
    CHECK_EVAL("_123456", "123456");
    CHECK_EVAL("123456_", "123456");
    CHECK_EVAL("123___456", "123456");
    CHECK_EVAL("._123456", "0.123456");

    CHECK_EVAL("12 345.678 9", "12345.6789");
//    CHECK_EVAL("12'345.678'9", "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12·345.678·9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12٫345.678٫9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12٬345.678٬9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12˙345.678˙9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12⎖345.678⎖9"), "12345.6789");

    // Punctuation/symbol separators.
    CHECK_EVAL("12$345.678~9", "12345.6789");
    CHECK_EVAL("12`345.678@9", "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12€345.678€9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12£345.678£9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12¥345.678¥9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12₩345.678₩9"), "12345.6789");

    // Prefix/suffix separators are still accepted.
    CHECK_EVAL("$ 1234.567", "1234.567");
    CHECK_EVAL("1234.567 $", "1234.567");
    CHECK_EVAL(QString::fromUtf8("€ 1234.567"), "1234.567");
    CHECK_EVAL(QString::fromUtf8("1234.567 ¥"), "1234.567");
    CHECK_EVAL("$-10", "-10");
    CHECK_EVAL("$+10", "10");

    // Unicode letters between digits must not be silently ignored.
    CHECK_EVAL_FAIL(QString::fromUtf8("12天2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12é2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12ä2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12ß2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12Ж2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12π2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12Ω2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12क2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12م2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12あ2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12한2"));

    CHECK_EVAL_FAIL(QString::fromUtf8("1天234"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12天345.67"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12.34天56"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12e3天4"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0x1天2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0b10天01"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0o12天34"));
}

void test_sexagesimal()
{
    // Backup current settings
    Settings* settings = Settings::instance();
    char angleUnit = settings->angleUnit;
    char resultFormat = settings->resultFormat;
    int resultPrecision = settings->resultPrecision;

    settings->angleUnit = 'd';
    settings->resultFormat = 's';
    settings->resultPrecision = 2;
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT_FAIL(":");
    CHECK_EVAL_FORMAT_FAIL("::");
    CHECK_EVAL_FORMAT_FAIL("0:::");
    CHECK_EVAL_FORMAT_FAIL("1.2:34.56");
    CHECK_EVAL_FORMAT_FAIL("12:3.4:56.78");

    CHECK_EVAL_FORMAT("0:", "0:00:00");
    CHECK_EVAL_FORMAT("::56", "0:00:56.00");
    CHECK_EVAL_FORMAT("56 second", "0:00:56.00");
    CHECK_EVAL_FORMAT(":34", "0:34:00.00");
    CHECK_EVAL_FORMAT(":34:", "0:34:00.00");
    CHECK_EVAL_FORMAT(":34.5:", "0:34:30.00");
    CHECK_EVAL_FORMAT("34.5 minute", "0:34:30.00");
    CHECK_EVAL_FORMAT("12:", "12:00:00.00");
    CHECK_EVAL_FORMAT("12::", "12:00:00.00");
    CHECK_EVAL_FORMAT("12.3:", "12:18:00.00");
    CHECK_EVAL_FORMAT("12.3 hour", "12:18:00.00");
    CHECK_EVAL_FORMAT("12:34", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34:", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34.", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34.5", "12:34:30.00");
    CHECK_EVAL_FORMAT("12:34:56", "12:34:56.00");
    CHECK_EVAL_FORMAT("12:34:56.", "12:34:56.00");
    CHECK_EVAL_FORMAT("12:34:56.78", "12:34:56.78");
    CHECK_EVAL("1:30 / 4:00", "0.375");
    CHECK_EVAL("(1:30) / (4:00)", "0.375");

    CHECK_EVAL_FORMAT("-0:", "0:00:00");
    CHECK_EVAL_FORMAT("-::56", "-0:00:56.00");
    CHECK_EVAL_FORMAT("-:34", "-0:34:00.00");
    CHECK_EVAL_FORMAT("-12:", "-12:00:00.00");
    CHECK_EVAL_FORMAT("-12:34", "-12:34:00.00");
    CHECK_EVAL_FORMAT("-12:34.5", "-12:34:30.00");
    CHECK_EVAL_FORMAT("-12:34:56", "-12:34:56.00");
    CHECK_EVAL_FORMAT("-12:34:56.78", "-12:34:56.78");

    CHECK_EVAL_FORMAT_FAIL("°");
    CHECK_EVAL_FORMAT_FAIL("°'");
    CHECK_EVAL_FORMAT_FAIL("°0\"");
    CHECK_EVAL_FORMAT_FAIL("1.2°34.56");
    CHECK_EVAL_FORMAT_FAIL("12°3.4'56.78\"");

    CHECK_EVAL_FORMAT_FAIL("56\"7");
    CHECK_EVAL_FORMAT_FAIL("56.78\"9");
    CHECK_EVAL_FORMAT_FAIL("34'\"5");
    CHECK_EVAL_FORMAT_FAIL("12°'\"3");
    CHECK_EVAL_FORMAT_FAIL("12°34'56.78\"9");

    CHECK_EVAL_FORMAT("0", "0°00'00");
    CHECK_EVAL_FORMAT("°'56", "0°00'56.00");
    CHECK_EVAL_FORMAT("'56", "0°00'56.00");
    CHECK_EVAL_FORMAT("56\"", "0°00'56.00");
    CHECK_EVAL_FORMAT("56 arcsecond", "0°00'56.00");
    CHECK_EVAL_FORMAT("56.78\"", "0°00'56.78");
    CHECK_EVAL_FORMAT("°34", "0°34'00.00");
    CHECK_EVAL_FORMAT("34'", "0°34'00.00");
    CHECK_EVAL_FORMAT("34.5'", "0°34'30.00");
    CHECK_EVAL_FORMAT("34'\"", "0°34'00.00");
    CHECK_EVAL_FORMAT("34.5'\"", "0°34'30.00");
    CHECK_EVAL_FORMAT("34.5 arcminute", "0°34'30.00");
    CHECK_EVAL_FORMAT("34'56", "0°34'56.00");
    CHECK_EVAL_FORMAT("12°", "12°00'00.00");
    CHECK_EVAL_FORMAT("12°'", "12°00'00.00");
    CHECK_EVAL_FORMAT("12°'\"", "12°00'00.00");
    CHECK_EVAL_FORMAT("12.3°", "12°18'00.00");
    CHECK_EVAL_FORMAT("12.3°'", "12°18'00.00");
    CHECK_EVAL_FORMAT("12.3°'\"", "12°18'00.00");
    CHECK_EVAL_FORMAT("12°34", "12°34'00.00");
    CHECK_EVAL_FORMAT("12°34'", "12°34'00.00");
    CHECK_EVAL_FORMAT("12°34.", "12°34'00.00");
    CHECK_EVAL_FORMAT("12°34.5", "12°34'30.00");
    CHECK_EVAL_FORMAT("12°34.5'", "12°34'30.00");
    CHECK_EVAL_FORMAT("12°34'56", "12°34'56.00");
    CHECK_EVAL_FORMAT("12°34'56.", "12°34'56.00");
    CHECK_EVAL_FORMAT("12°34'56.78", "12°34'56.78");
    CHECK_EVAL_FORMAT("12°34'56.78\"", "12°34'56.78");

    CHECK_EVAL_FORMAT("-0", "0°00'00");
    CHECK_EVAL_FORMAT("-°'56", "-0°00'56.00");
    CHECK_EVAL_FORMAT("-56\"", "-0°00'56.00");
    CHECK_EVAL_FORMAT("-°34", "-0°34'00.00");
    CHECK_EVAL_FORMAT("-34'", "-0°34'00.00");
    CHECK_EVAL_FORMAT("-12°", "-12°00'00.00");
    CHECK_EVAL_FORMAT("-12.3°", "-12°18'00.00");
    CHECK_EVAL_FORMAT("-12°34", "-12°34'00.00");
    CHECK_EVAL_FORMAT("-12°34.5", "-12°34'30.00");
    CHECK_EVAL_FORMAT("-12°34'56", "-12°34'56.00");
    CHECK_EVAL_FORMAT("-12°34'56.78", "-12°34'56.78");
    CHECK_EVAL_FORMAT("-12°34'56.78\"", "-12°34'56.78");

    settings->angleUnit = 'g';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("100.5", "90°27'00.00");
    CHECK_EVAL_FORMAT("-200.3", "-180°16'12.00");
    CHECK_EVAL_FORMAT("12.3 degree", "12°18'00.00");

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("pi", "180°00'00.00");
    CHECK_EVAL_FORMAT("-pi", "-180°00'00.00");
    CHECK_EVAL_FORMAT("12.3 degree", "12°18'00.00");

    settings->angleUnit = 't';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("0.25", "90°00'00.00");
    CHECK_EVAL_FORMAT("-0.5", "-180°00'00.00");
    CHECK_EVAL_FORMAT("12.3 degree", "12°18'00.00");

    settings->resultPrecision = 32;

    CHECK_EVAL_FORMAT("::56.78901234567890123456789012345678", "0:00:56.78901234567890123456789012345678");
    CHECK_EVAL_FORMAT(":34.56789012345678901234567890123456", "0:34:34.07340740740734074074073407407360");
    CHECK_EVAL_FORMAT("12.34567890123456789012345678901234:", "12:20:44.44404444444440444444444044442400");

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("56.78901234567890123456789012345678\"", "0°00'56.78901234567890123456789012345678");
    CHECK_EVAL_FORMAT("34.56789012345678901234567890123456'", "0°34'34.07340740740734074074073407407360");
    CHECK_EVAL_FORMAT("12.34567890123456789012345678901234°", "12°20'44.44404444444440444444444044442400");

    // Restore old settings
    settings->angleUnit = angleUnit;
    settings->resultFormat = resultFormat;
    settings->resultPrecision = resultPrecision;
    Evaluator::instance()->initializeAngleUnits();
}

void test_rational_format()
{
    Settings* settings = Settings::instance();
    const char angleUnit = settings->angleUnit;
    const char resultFormat = settings->resultFormat;
    const int resultPrecision = settings->resultPrecision;
    const bool complexNumbers = settings->complexNumbers;
    const char resultFormatComplex = settings->resultFormatComplex;
    const bool complexMode = DMath::complexMode;

    settings->resultFormat = 'r';
    settings->resultPrecision = -1;
    settings->complexNumbers = false;
    DMath::complexMode = false;
    eval->initializeBuiltInVariables();
    const QString dotOperator(UnicodeChars::DotOperator);
    const QString piSymbol = QStringLiteral("pi");
    const auto piOver = [&](int denominator) {
        return piSymbol + slash + QString::number(denominator);
    };
    const auto nPiOver = [&](int numerator, int denominator) {
        return QString::number(numerator) + dotOperator + piSymbol
            + slash + QString::number(denominator);
    };
    const auto nPi = [&](int numerator) {
        return QString::number(numerator) + dotOperator + piSymbol;
    };
    const auto signedPiOver = [&](int numerator, int denominator) -> QString {
        if (numerator == 1)
            return piOver(denominator);
        if (numerator == -1)
            return QStringLiteral("-") + piOver(denominator);
        return nPiOver(numerator, denominator);
    };

    ++eval_total_tests;
    QString direct = NumberFormatter::format(Quantity(HNumber("0.5")), 'r');
    direct.replace(QString::fromUtf8("−"), "-");
    const QString expectedDirect = QStringLiteral("1")
        + slash
        + QStringLiteral("2");
    if (direct != expectedDirect) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tdirect NumberFormatter rational formatting\t[NEW]" << endl
             << "\tResult   : " << direct.toLatin1().constData() << endl
             << "\tExpected : " << expectedDirect.toLatin1().constData() << endl;
    }

    CHECK_EVAL_FORMAT_HAS_SLASH("1/3");
    CHECK_EVAL_FORMAT_HAS_SLASH("2/4");
    CHECK_EVAL_FORMAT_HAS_SLASH("-10/4");
    CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH("2/4");
    CHECK_EVAL_FORMAT("0", "0");
    CHECK_EVAL_FORMAT_HAS_SLASH("0.1");
    CHECK_EVAL_FORMAT_HAS_SLASH("1000001/1000000");
    CHECK_EVAL_FORMAT_HAS_SLASH("0.5 meter");
    CHECK_EVAL_FORMAT_HAS_SLASH("-0.5 meter");
    CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH("0.5 meter");
    CHECK_EVAL_FORMAT_HAS_SLASH("1/1000000");
    CHECK_EVAL_FORMAT_HAS_SLASH("355/113");

    // Must fall back to float for denominator overflow / non-rational / huge numerator.
    CHECK_EVAL_FORMAT_NO_SLASH("1/1000001");
    CHECK_EVAL_FORMAT_NO_SLASH("sqrt(2)");
    CHECK_EVAL_FORMAT_NO_SLASH("0.333333333333333333333333333333");
    CHECK_EVAL_FORMAT_NO_SLASH("3000000000");
    CHECK_EVAL_FORMAT_NO_SLASH("exp(1)");

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("arcsin(-1)", signedPiOver(-1, 2));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-sqrt(3)/2)", signedPiOver(-1, 3));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-sqrt(2)/2)", signedPiOver(-1, 4));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-1/2)", signedPiOver(-1, 6));
    CHECK_EVAL_FORMAT_EXACT("arcsin(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("arcsin(1/2)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(2)/2)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(3)/2)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("arcsin(1)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("arccos(-1)", piSymbol);
    CHECK_EVAL_FORMAT_EXACT("arccos(-sqrt(3)/2)", nPiOver(5, 6));
    CHECK_EVAL_FORMAT_EXACT("arccos(-sqrt(2)/2)", nPiOver(3, 4));
    CHECK_EVAL_FORMAT_EXACT("arccos(-1/2)", nPiOver(2, 3));
    CHECK_EVAL_FORMAT_EXACT("arccos(0)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("arccos(1/2)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("arccos(sqrt(2)/2)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arccos(sqrt(3)/2)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arccos(1)", "0");
    CHECK_EVAL_FORMAT_EXACT("arctan(-sqrt(3))", signedPiOver(-1, 3));
    CHECK_EVAL_FORMAT_EXACT("arctan(-1)", signedPiOver(-1, 4));
    CHECK_EVAL_FORMAT_EXACT("arctan(-sqrt(3)/3)", signedPiOver(-1, 6));
    CHECK_EVAL_FORMAT_EXACT("arctan(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("arctan(sqrt(3)/3)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arctan(1)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arctan(sqrt(3))", piOver(3));
    CHECK_EVAL_FORMAT_NO_SLASH("sin(1)");
    CHECK_EVAL_FORMAT_NO_SLASH("cos(1)");
    CHECK_EVAL_FORMAT_NO_SLASH("tan(1)");
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(3)/22)", "0.07881114207211010205");
    CHECK_EVAL_FORMAT_NO_SLASH("arcsin(0.1)");
    CHECK_EVAL_FORMAT_NO_SLASH("arccos(0.1)");
    CHECK_EVAL_FORMAT_NO_SLASH("arctan(0.1)");
    CHECK_EVAL_FORMAT_EXACT("sin(pi/4)", QStringLiteral("sqrt(2)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("cos(pi/6)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("tan(pi/3)", QStringLiteral("sqrt(3)"));
    CHECK_EVAL_FORMAT_EXACT("tan(pi/6)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("3"));

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("sin(45)", QStringLiteral("sqrt(2)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("cos(30)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("tan(60)", QStringLiteral("sqrt(3)"));
    CHECK_EVAL_FORMAT_EXACT("tan(30)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("3"));
    CHECK_EVAL_FORMAT_EXACT("radians(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("radians(30)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("radians(45)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("radians(60)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("radians(90)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("radians(120)", nPiOver(2, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(135)", nPiOver(3, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(150)", nPiOver(5, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(180)", piSymbol);
    CHECK_EVAL_FORMAT_EXACT("radians(210)", nPiOver(7, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(225)", nPiOver(5, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(240)", nPiOver(4, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(270)", nPiOver(3, 2));
    CHECK_EVAL_FORMAT_EXACT("radians(300)", nPiOver(5, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(315)", nPiOver(7, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(330)", nPiOver(11, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(360)", nPi(2));

    settings->complexNumbers = true;
    settings->resultFormatComplex = 'c';
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();
    CHECK_EVAL_FORMAT("1+j", "1+1j");
    CHECK_EVAL_FORMAT_NO_SLASH("1+j");

    // Explicit non-decimal format should still take precedence.
    CHECK_EVAL_FORMAT("hex(31)", "0x1F");

    settings->angleUnit = angleUnit;
    settings->resultFormat = resultFormat;
    settings->resultPrecision = resultPrecision;
    settings->complexNumbers = complexNumbers;
    settings->resultFormatComplex = resultFormatComplex;
    DMath::complexMode = complexMode;
    eval->initializeBuiltInVariables();
    Evaluator::instance()->initializeAngleUnits();
}

void test_function_basic()
{
    CHECK_EVAL("ABS(0)", "0");
    CHECK_EVAL("ABS(1)", "1");
    CHECK_EVAL("ABS(-1)", "1");
    CHECK_EVAL("ABS(--1)", "1");
    CHECK_EVAL("ABS(---1)", "1");
    CHECK_EVAL("ABS((1))", "1");
    CHECK_EVAL("ABS((-1))", "1");

    CHECK_EVAL("ABS(1/2)", "0.5");
    CHECK_EVAL("ABS(-1/2)", "0.5");
    CHECK_EVAL("ABS(-1/-2)", "0.5");
    CHECK_EVAL("ABS(1/-2)", "0.5");

    CHECK_EVAL("INT(0)", "0");
    CHECK_EVAL("INT(1)", "1");
    CHECK_EVAL("INT(-1)", "-1");
    CHECK_EVAL("INT(0.5)", "0");
    CHECK_EVAL("INT(-0.75)", "0");
    CHECK_EVAL("INT(-0.9999*1)", "0");
    CHECK_EVAL("INT(0.9999*1)", "0");
    CHECK_EVAL("INT(2.1)", "2");
    CHECK_EVAL("INT(-3.4)", "-3");

    CHECK_EVAL("log(0.123; 0.1234)", "0.99845065797473594741");
    CHECK_EVAL("lg(0.00000000001)", "-11");
    CHECK_EVAL("lg(1e-3)", "-3");
    CHECK_EVAL("lb(0.00000000001)", "-36.54120904376098582657");
    CHECK_EVAL("lb(32)", "5");
    CHECK_EVAL("ln(100)", "4.60517018598809136804");
    CHECK_EVAL("ln(4.0)", "1.38629436111989061883");

    CHECK_EVAL("0!", "1");
    CHECK_EVAL("1!", "1");
    CHECK_EVAL("2!", "2");
    CHECK_EVAL("3!", "6");
    CHECK_EVAL("4!", "24");
    CHECK_EVAL("5!", "120");
    CHECK_EVAL("6!", "720");
    CHECK_EVAL("7!", "5040");
    CHECK_EVAL("(1+1)!^2", "4");

    CHECK_EVAL("(-27)^(1/3)", "-3");
    CHECK_EVAL("(-27)^(-1/3)", "-0.33333333333333333333");
    CHECK_EVAL("(-1)^(1/3)", "-1");
    CHECK_EVAL("(-1)^(1/7)", "-1");
    CHECK_EVAL("(-1)^(2/3)", "1");
    CHECK_EVAL("(-1)^(1/2)", "NaN");

    CHECK_EVAL_PRECISE("exp((1)/2) + exp((1)/2)", "3.29744254140025629369730157562832714330755220142030");

    CHECK_EVAL("√(81)", "9");
    CHECK_EVAL("∛(-27)", "-3");
    CHECK_INTERPRETED("sqrt(81)", "√(81)");
    CHECK_INTERPRETED("cbrt(-27)", "∛(-27)");
    CHECK_INTERPRETED("√(81)", "√(81)");
    CHECK_INTERPRETED("∛(-27)", "∛(-27)");

    // Test functions composition
    CHECK_EVAL("log(10;log(10;1e100))", "2");
    CHECK_EVAL("log(10;abs(-100))", "2");
    CHECK_EVAL("abs(log(10;100))", "2");
    CHECK_EVAL("abs(abs(-100))", "100");
    CHECK_EVAL("sum(10;abs(-100);1)", "111");
    CHECK_EVAL("sum(abs(-100);10;1)", "111");
    CHECK_EVAL("sum(10;1;abs(-100))", "111");

    CHECK_EVAL("ieee754_half_decode(0x7E00)", "NaN");
    CHECK_EVAL("hex(ieee754_half_encode(1))", "0x3C00");
    CHECK_EVAL("hex(ieee754_half_encode(0.99951171875))", "0x3BFF");
    CHECK_EVAL("hex(ieee754_half_encode(0.999755859375))", "0x3C00");
}

void test_function_trig()
{
    CHECK_EVAL_PRECISE("pi", "3.14159265358979323846264338327950288419716939937511");

    CHECK_EVAL("sin(0)", "0");
    CHECK_EVAL("cos(0)", "1");
    CHECK_EVAL("tan(0)", "0");

    CHECK_EVAL("sin(pi)", "0");
    CHECK_EVAL("cos(pi)", "-1");
    CHECK_EVAL("tan(pi)", "0");

    CHECK_EVAL("sin(-pi)", "0");
    CHECK_EVAL("cos(-pi)", "-1");
    CHECK_EVAL("tan(-pi)", "0");

    CHECK_EVAL("sin(--pi)", "0");
    CHECK_EVAL("cos(--pi)", "-1");
    CHECK_EVAL("tan(--pi)", "0");

    CHECK_EVAL("sin(pi/2)", "1");
    CHECK_EVAL("cos(pi/2)", "0");

    CHECK_EVAL("sin(-pi/2)", "-1");
    CHECK_EVAL("cos(-pi/2)", "0");

    CHECK_EVAL("sin(-pi/2) + sin(pi/2)", "0");
    CHECK_EVAL("sin(-pi/2) - sin(pi/2)", "-2");
    CHECK_EVAL("cos(-pi/2) + cos(pi/2)", "0");
    CHECK_EVAL("cos(-pi/2) - cos(pi/2)", "0");

    CHECK_EVAL("arcsin(sin(1))", "1");
    CHECK_EVAL("arccos(cos(1))", "1");
    CHECK_EVAL("arctan(tan(1))", "1");
    CHECK_EVAL("arcsin(0)", "0");
    CHECK_EVAL("arccos(1)", "0");
    CHECK_EVAL("arctan(0)", "0");

    CHECK_EVAL("degrees(0)", "0");
    CHECK_EVAL("degrees(pi/2)", "90");
    CHECK_EVAL("degrees(pi)", "180");
    CHECK_EVAL("degrees(3*pi/2)", "270");
    CHECK_EVAL("degrees(2*pi)", "360");

    CHECK_EVAL("radians(0)", "0");
    CHECK_EVAL("radians(90)/pi", "0.5");
    CHECK_EVAL("radians(180)/pi", "1");
    CHECK_EVAL("radians(270)/pi", "1.5");
    CHECK_EVAL("radians(360)/pi", "2");

    CHECK_EVAL("gradians(0)", "0");
    CHECK_EVAL("gradians(pi/2)", "100");
    CHECK_EVAL("gradians(pi)", "200");
    CHECK_EVAL("gradians(3*pi/2)", "300");
    CHECK_EVAL("gradians(2*pi)", "400");

    CHECK_EVAL("turns(0)", "0");
    CHECK_EVAL("turns(pi/2)", "0.25");
    CHECK_EVAL("turns(pi)", "0.5");
    CHECK_EVAL("turns(3*pi/2)", "0.75");
    CHECK_EVAL("turns(2*pi)", "1");
}

void test_function_stat()
{
    CHECK_EVAL_FAIL("MIN(0)");
    CHECK_EVAL("MIN(0; 1)", "0");
    CHECK_EVAL("MIN(0; 2)", "0");
    CHECK_EVAL("MIN(-1; 0)", "-1");
    CHECK_EVAL("MIN(-1; 1)", "-1");
    CHECK_EVAL("MIN(-0.01; 0)", "-0.01");
    CHECK_EVAL("MIN(0; 1; 2)", "0");
    CHECK_EVAL("MIN(-1; 0; 1; 2)", "-1");
    CHECK_EVAL("MIN(-2; -1; 0; 1; 2)", "-2");

    CHECK_EVAL_FAIL("MAX(0)");
    CHECK_EVAL("MAX(0; 1)", "1");
    CHECK_EVAL("MAX(0; 2)", "2");
    CHECK_EVAL("MAX(-1; 0)", "0");
    CHECK_EVAL("MAX(-1; 1)", "1");
    CHECK_EVAL("MAX(0.01; 0)", "0.01");
    CHECK_EVAL("MAX(0; 1; 2)", "2");
    CHECK_EVAL("MAX(-1; 0; 1; 2)", "2");
    CHECK_EVAL("MAX(-2; -1; 0; 1; 2)", "2");

    CHECK_EVAL_FAIL("SUM(1)");
    CHECK_EVAL("SUM(100;1)", "101");
    CHECK_EVAL("SUM(-100;1)", "-99");
    CHECK_EVAL("SUM(0;0;0)", "0");
    CHECK_EVAL("SUM(100;-1)", "99");
    CHECK_EVAL("SUM(-100;-1)", "-101");
    CHECK_EVAL("SUM(1;2;3;4;5;6)", "21");
    CHECK_EVAL("SUM(1;-2;3;-4;5;-6)", "-3");

    CHECK_EVAL_FAIL("SIGMA(1;10)");
    CHECK_EVAL_FAIL("SIGMA(1;2;3;4)");
    CHECK_EVAL_FAIL("SIGMA(1.5;10;n)");
    CHECK_EVAL_FAIL("SIGMA(1 meter;10;n)");
    CHECK_EVAL_FAIL("SIGMA(1;10;n+unknown_var)");
    CHECK_EVAL("SIGMA(1;1;n)", "1");
    CHECK_EVAL("SIGMA(3;3;2+n)", "5");
    CHECK_EVAL("SIGMA(1;4;3)", "12");
    CHECK_EVAL("SIGMA(-2;2;n)", "0");
    CHECK_EVAL("SIGMA(1;10;2+n)", "75");
    CHECK_EVAL("SIGMA(10;1;n)", "55");
    CHECK_EVAL("SIGMA(-1;-4;n)", "-10");
    CHECK_EVAL("SIGMA(1;4;n^2)", "30");
    CHECK_EVAL("SIGMA(1;4;1/n)", "2.08333333333333333333");
    CHECK_EVAL("SIGMA(1;3;n meter)", "NaN");
    CHECK_EVAL("SIGMA(1;3;n^2+n)", "20");
    CHECK_INTERPRETED("sigma(1;10;n+1)", "sigma(1; 10; n+1)");
    CHECK_EVAL("SIGMA(1;3;SIGMA(1;2;1))", "6");
    CHECK_EVAL_FAIL("SIGMA(1;3;SIGMA(1;n;1))");
    CHECK_EVAL("f(k)=k^2", "NaN");
    CHECK_EVAL_FAIL("SIGMA(1;4;f(n))");
    CHECK_EVAL("x=10", "10");
    CHECK_EVAL("SIGMA(1;3;n+x)", "36");
    CHECK_EVAL("x", "10");
    CHECK_EVAL("n=123", "123");
    CHECK_EVAL("SIGMA(1;3;n)", "6");
    CHECK_EVAL("n", "123");

    CHECK_EVAL_FAIL("PRODUCT(-1)");
    CHECK_EVAL("PRODUCT(100;0)", "0");
    CHECK_EVAL("PRODUCT(100;1)", "100");
    CHECK_EVAL("PRODUCT(-100;1)", "-100");
    CHECK_EVAL("PRODUCT(-100;-1)", "100");
    CHECK_EVAL("PRODUCT(1;1;1)", "1");
    CHECK_EVAL("PRODUCT(1;2;3;4;5;6)", "720");
    CHECK_EVAL("PRODUCT(1;-2;3;-4;5;-6)", "-720");

    CHECK_EVAL_FAIL("AVERAGE(0)");
    CHECK_EVAL("AVERAGE(0;0)", "0");
    CHECK_EVAL("AVERAGE(0;0;0)", "0");
    CHECK_EVAL("AVERAGE(0;1)", "0.5");
    CHECK_EVAL("AVERAGE(0;1;2)", "1");
    CHECK_EVAL("AVERAGE(0;1;0)*3", "1");
    CHECK_EVAL("AVERAGE(1;1;1)", "1");
    CHECK_EVAL("AVERAGE(2;2;2)", "2");
    CHECK_EVAL("AVERAGE(3;3;3)", "3");
    CHECK_EVAL("AVERAGE(0.25;0.75)", "0.5");
    CHECK_EVAL("AVERAGE(2.25;4.75)", "3.5");
    CHECK_EVAL("AVERAGE(1/3;2/3)", "0.5");

    CHECK_EVAL_FAIL("GEOMEAN(-1e20;0;-1)");
    CHECK_EVAL_FAIL("GEOMEAN(5)");
    CHECK_EVAL("GEOMEAN(1;1)", "1");
    CHECK_EVAL("GEOMEAN(1;4)", "2");
    CHECK_EVAL("GEOMEAN(4;9)", "6");
    CHECK_EVAL("GEOMEAN(3.6;8.1)", "5.4");
    CHECK_EVAL("GEOMEAN(3;4;18)", "6");
    CHECK_EVAL("GEOMEAN(1;1;1)", "1");
    CHECK_EVAL("GEOMEAN(1;1;1;1)", "1");
    CHECK_EVAL_FAIL("GEOMEAN(1;1;1;-1)");

    CHECK_EVAL("VARIANCE(1;-1)", "1");
    CHECK_EVAL("VARIANCE(5 meter; 13 meter)", "16 meter²");
    // for complex tests of VARIANCE see test_complex

    CHECK_EVAL("int(rand())", "0");
    CHECK_EVAL("rand(0)", "0");
    CHECK_EVAL_FAIL("rand(1;2)");
    CHECK_EVAL_FAIL("rand(-1)");
    CHECK_EVAL_FAIL("rand(1.5)");
    CHECK_EVAL_FAIL("rand(1 meter)");
    CHECK_EVAL_FAIL("rand(1000)");

    CHECK_EVAL("randint(0)", "0");
    CHECK_EVAL("randint(5;5)", "5");
    CHECK_EVAL("randint(-3;-3)", "-3");
    CHECK_EVAL("int(randint(9)/10)", "0");
    CHECK_EVAL("int(randint(-9)/10)", "0");
    CHECK_EVAL("int(randint(3;1)/10)", "0");
    CHECK_EVAL_FAIL("randint()");
    CHECK_EVAL_FAIL("randint(1;2;3)");
    CHECK_EVAL_FAIL("randint(1.5)");
    CHECK_EVAL_FAIL("randint(1;2.5)");
    CHECK_EVAL_FAIL("randint(1 meter)");
}

void test_function_logic()
{
    CHECK_EVAL_FAIL("and(1)");
    CHECK_EVAL_FAIL("or(2)");
    CHECK_EVAL_FAIL("xor(3)");
    CHECK_EVAL_FAIL("popcount()");
    CHECK_EVAL_FAIL("popcount(1;2)");
    CHECK_EVAL_FAIL("popcount(1 meter)");
    CHECK_EVAL_FAIL("popcount(2^300)");

    CHECK_EVAL("and(0;0)", "0");
    CHECK_EVAL("and(0;1)", "0");
    CHECK_EVAL("and(1;0)", "0");
    CHECK_EVAL("and(1;1)", "1");

    CHECK_EVAL("or(0;0)", "0");
    CHECK_EVAL("or(0;1)", "1");
    CHECK_EVAL("or(1;0)", "1");
    CHECK_EVAL("or(1;1)", "1");

    CHECK_EVAL("xor(0;0)", "0");
    CHECK_EVAL("xor(0;1)", "1");
    CHECK_EVAL("xor(1;0)", "1");
    CHECK_EVAL("xor(1;1)", "0");

    CHECK_EVAL("popcount(0)", "0");
    CHECK_EVAL("popcount(1)", "1");
    CHECK_EVAL("popcount(2^254)", "1");
    CHECK_EVAL("popcount(2^255-1)", "255");
    CHECK_EVAL("popcount(-2^255)", "1");
    CHECK_EVAL("popcount(15)", "4");
    CHECK_EVAL("popcount(0xF0F0)", "8");
    CHECK_EVAL("popcount(3.9)", "2");
    CHECK_EVAL("popcount(-3.9)", "255");
    CHECK_EVAL("popcount(0.9)", "0");
    CHECK_EVAL("popcount(-0.9)", "256");
    CHECK_EVAL("popcount(-1)", "256");
    CHECK_EVAL("popcount(and(0x123456789ABCDEF0; not(0x123456789ABCDEF0)))", "0");
    CHECK_EVAL("popcount(or(0x123456789ABCDEF0; not(0x123456789ABCDEF0)))", "256");
    CHECK_EVAL("popcount(123456789) + popcount(not(123456789))", "256");
    CHECK_EVAL("popcount(-123456789) + popcount(not(-123456789))", "256");
}

void test_function_discrete()
{
    CHECK_EVAL("idiv(10;3)", "3");
    CHECK_EVAL("idiv(-10;3)", "-3");
    CHECK_EVAL("idiv(10;-3)", "-3");
    CHECK_EVAL("idiv(-10;-3)", "3");

    CHECK_EVAL("mod(10;3)", "1");
    CHECK_EVAL("mod(-10;3)", "-1");
    CHECK_EVAL("mod(10;-3)", "1");
    CHECK_EVAL("mod(-10;-3)", "-1");

    CHECK_EVAL("emod(10;3)", "1");
    CHECK_EVAL("emod(-10;3)", "2");
    CHECK_EVAL("emod(10;-3)", "-2");
    CHECK_EVAL("emod(-10;-3)", "-1");
    CHECK_EVAL("emod(-1;360)", "359");
    CHECK_EVAL("emod(360;360)", "0");
    CHECK_EVAL("emod(361;360)", "1");
    CHECK_EVAL("emod(-360;360)", "0");
    CHECK_EVAL("emod(-361;360)", "359");
    CHECK_EVAL("emod(721;360)", "1");
    CHECK_EVAL("emod(-721;360)", "359");
    CHECK_EVAL("emod(0;360)", "0");
    CHECK_EVAL("emod(180;360)", "180");
    CHECK_EVAL("emod(-180;360)", "180");
    CHECK_EVAL("emod(1;-360)", "-359");
    CHECK_EVAL("emod(359;-360)", "-1");
    CHECK_EVAL("emod(360;-360)", "0");
    CHECK_EVAL("emod(-1;-360)", "-1");
    CHECK_EVAL("emod(-361;-360)", "-1");
    CHECK_EVAL("emod(2.5;2)", "0.5");
    CHECK_EVAL("emod(-2.5;2)", "1.5");

    CHECK_EVAL("powmod(2;10;7)", "2");
    CHECK_EVAL("powmod(3;233;353)", "248");
    CHECK_EVAL("powmod(-3;233;353)", "105");
    CHECK_EVAL("powmod(123456789;0;97)", "1");
    CHECK_EVAL("powmod(123456789;1234567;1)", "0");
    CHECK_EVAL("powmod(123456789;1234567;-97)", "-80");
    CHECK_EVAL_FAIL("powmod(2;-1;5)");
    CHECK_EVAL_FAIL("powmod(2;3.5;5)");
    CHECK_EVAL_FAIL("powmod(2.5;3;5)");

    CHECK_EVAL("gcd(12;18)", "6");
    CHECK_EVAL("gcd(36;56;210)", "2");
    CHECK_EVAL("gcd(28;120;126)", "2");
    CHECK_EVAL("lcm(12;18)", "36");
    CHECK_EVAL("lcm(36;56;210)", "2520");
    CHECK_EVAL("lcm(0;5)", "0");
    CHECK_EVAL("lcm(-9;27)", "27");
    CHECK_EVAL("round(5.5e-10;2)", "0");
    CHECK_EVAL("round(5.5*10^-10;2)", "0");
    CHECK_EVAL("round(0.00000000055;2)", "0");

    CHECK_EVAL("ncr(-3;-1)", "0");
    CHECK_EVAL("ncr(-3;0)", "1");
    CHECK_EVAL("ncr(-3;1)", "-3");
    CHECK_EVAL("ncr(-3;2)", "6");
    CHECK_EVAL("ncr(-3;3)", "-10");
    CHECK_EVAL("ncr(-3;4)", "15");
    CHECK_EVAL("ncr(-3;5)", "-21");
    CHECK_EVAL("ncr(-2;-1)", "0");
    CHECK_EVAL("ncr(-2;0)", "1");
    CHECK_EVAL("ncr(-2;1)", "-2");
    CHECK_EVAL("ncr(-2;2)", "3");
    CHECK_EVAL("ncr(-2;3)", "-4");
    CHECK_EVAL("ncr(-2;4)", "5");
    CHECK_EVAL("ncr(-2;5)", "-6");

    CHECK_EVAL("ncr(-1;-1)", "0");
    CHECK_EVAL("ncr(-1;0)", "1");
    CHECK_EVAL("ncr(-1;1)", "-1");
    CHECK_EVAL("ncr(-1;2)", "1");
    CHECK_EVAL("ncr(-1;3)", "-1");
    CHECK_EVAL("ncr(-1;4)", "1");
    CHECK_EVAL("ncr(-1;5)", "-1");

    CHECK_EVAL("ncr(0;-1)", "0");
    CHECK_EVAL("ncr(0;0)", "1");
    CHECK_EVAL("ncr(0;1)", "0");

    CHECK_EVAL("ncr(1;-1)", "0");
    CHECK_EVAL("ncr(1;0)", "1");
    CHECK_EVAL("ncr(1;1)", "1");
    CHECK_EVAL("ncr(1;2)", "0");

    CHECK_EVAL("ncr(2;-1)", "0");
    CHECK_EVAL("ncr(2;0)", "1");
    CHECK_EVAL("ncr(2;1)", "2");
    CHECK_EVAL("ncr(2;2)", "1");
    CHECK_EVAL("ncr(2;3)", "0");

    CHECK_EVAL("ncr(3;-1)", "0");
    CHECK_EVAL("ncr(3;0)", "1");
    CHECK_EVAL("ncr(3;1)", "3");
    CHECK_EVAL("ncr(3;2)", "3");
    CHECK_EVAL("ncr(3;3)", "1");
    CHECK_EVAL("ncr(3;4)", "0");

    CHECK_EVAL("ncr(4;-1)", "0");
    CHECK_EVAL("ncr(4;0)", "1");
    CHECK_EVAL("ncr(4;1)", "4");
    CHECK_EVAL("ncr(4;2)", "6");
    CHECK_EVAL("ncr(4;3)", "4");
    CHECK_EVAL("ncr(4;4)", "1");
    CHECK_EVAL("ncr(4;5)", "0");
}

void test_function_simplified()
{
    /* Tests for standard functions */
    CHECK_EVAL("abs 123", "123");
    CHECK_EVAL("abs -123", "123");       /* (issue #600) */
    CHECK_EVAL("10 + abs 123", "133");
    CHECK_EVAL("10 + abs -123", "133");  /* (issue #600) */
    CHECK_EVAL("abs 123 + 10", "133");
    CHECK_EVAL("abs -123 + 10", "133");  /* (issue #600) */
    CHECK_EVAL("10 * abs 123", "1230");
    CHECK_EVAL("abs 123 * 10", "1230");
    /* Tests for user functions (issue #600, cf. discussion) */
    CHECK_USERFUNC_SET("func2(x) = abs(x)");
    CHECK_EVAL("func2 123", "123");
    CHECK_EVAL("func2 -123", "123");
    CHECK_EVAL("10 + func2 123", "133");
    CHECK_EVAL("10 + func2 -123", "133");
    CHECK_EVAL("func2 123 + 10", "133");
    CHECK_EVAL("func2 -123 + 10", "133");
    CHECK_EVAL("10 * func2 123", "1230");
    CHECK_EVAL("func2 123 * 10", "1230");
    CHECK_USERFUNC_SET("f(x) = 5 meter + x"); /* (issue #656)  */
    CHECK_EVAL("f(2 meter)", "7 meter");      /* (issue #656)  */
    CHECK_EVAL_FAIL("f(2)");                  /* (issue #656)  */
    /* Tests for priority management (issue #451) */
    CHECK_EVAL("lg 10^2", "2");
    CHECK_EVAL("frac 3!",  "0");
    CHECK_EVAL("-lg 10", "-1");
    CHECK_EVAL("2*-lg 10", "-2");
    CHECK_EVAL("--lg 10", "1");
    CHECK_EVAL("-ln 10", "-2.30258509299404568402");
    CHECK_EVAL("-lb 8", "-3");
    CHECK_EVAL("-sin 1", "-0.84147098480789650665");
    CHECK_EVAL("-cos pi", "1");
    CHECK_EVAL("-tan 1", "-1.55740772465490223051");
    CHECK_EVAL("-arcsin 1", "-1.57079632679489661923");
    CHECK_EVAL("-arccos 0", "-1.57079632679489661923");
    CHECK_EVAL("-arctan 1", "-0.78539816339744830962");
    CHECK_EVAL("2*-sin 1", "-1.68294196961579301331");
    CHECK_EVAL("--sin 1", "0.84147098480789650665");
    CHECK_EVAL("10 + -cos pi", "11");
}

void test_auto_fix_parentheses()
{
    CHECK_AUTOFIX("sin(1)", "sin(1)");
    CHECK_AUTOFIX("sin(1", "sin(1)");

    CHECK_AUTOFIX("x+(8-2", "x+(8-2)");
    CHECK_AUTOFIX("x+(8-2)", "x+(8-2)");

    CHECK_AUTOFIX("x+(8-(2*1", "x+(8-(2*1))");
    CHECK_AUTOFIX("x+(8-(2*1)", "x+(8-(2*1))");
    CHECK_AUTOFIX("x+(8-(2*1))", "x+(8-(2*1))");

    CHECK_AUTOFIX("x + sin (pi", "x + sin (pi)");
}

void test_auto_fix_ans()
{
    CHECK_AUTOFIX("sin", "sin(ans)");
    CHECK_AUTOFIX("cos", "cos(ans)");
    CHECK_AUTOFIX("tan", "tan(ans)");
    CHECK_AUTOFIX("abs", "abs(ans)");
    CHECK_AUTOFIX("exp", "exp(ans)");
    CHECK_AUTOFIX("lg", "lg(ans)");
    CHECK_AUTOFIX("sqrt", "√(ans)");
    CHECK_AUTOFIX("cbrt", "∛(ans)");
}

void test_auto_fix_trailing_equal()
{
    CHECK_AUTOFIX("1+2=", "1+2");
    CHECK_AUTOFIX("1==", "1");
    CHECK_AUTOFIX("1/3====", "1/3");
    CHECK_AUTOFIX("sin(x+y)=", "sin(x+y)");
    CHECK_AUTOFIX("1+2+", "1+2");
    CHECK_AUTOFIX("1++++", "1");
    CHECK_AUTOFIX("2**", "2");
    CHECK_AUTOFIX("3/", "3");
    CHECK_AUTOFIX("5+cos pi (", "5+cos pi");

    ++eval_total_tests;
    const QString fixedTrailingPlus = eval->autoFix(QString::fromUtf8("1+2+"));
    eval->setExpression(fixedTrailingPlus);
    const Quantity trailingPlusResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix trailing plus eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedTrailingPlus.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(trailingPlusResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("3")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix trailing plus eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : 3" << endl
                 << "\tAutoFix  : " << fixedTrailingPlus.toUtf8().constData() << endl;
        }
    }
}

void test_auto_fix_untouch()
{
    CHECK_AUTOFIX("sin(x)", "sin(x)");
    CHECK_AUTOFIX("sin((x/y))", "sin((x/y))");
    CHECK_AUTOFIX("ans", "ans");
    CHECK_AUTOFIX("sin(ans)", "sin(ans)");
    CHECK_AUTOFIX("tan(ans)", "tan(ans)");
    CHECK_AUTOFIX("x=1.2", "x=1.2");
    CHECK_AUTOFIX("1/sin pi", "1/sin pi");
}

void test_auto_fix_powers()
{
    CHECK_AUTOFIX("3¹", "3^1");
    CHECK_AUTOFIX("3⁻¹", "3^(-1)");
    CHECK_AUTOFIX("3¹²³⁴⁵⁶⁷⁸⁹", "3^123456789");
    CHECK_AUTOFIX("3²⁰", "3^20");
    CHECK_AUTOFIX("cos²(pi)", "cos(pi)^2");
    CHECK_AUTOFIX("cos² (pi)", "cos(pi)^2");
    CHECK_AUTOFIX("7 + 3²⁰ * 4", "7 + 3^20 * 4");
    CHECK_AUTOFIX("2×pi", "2⋅pi");
    CHECK_AUTOFIX("2×a", "2⋅a");
    CHECK_AUTOFIX("2   ×    pi   pi", "2⋅pi⋅pi");
    CHECK_AUTOFIX("2          ×pi×  pi", "2⋅pi⋅pi");
    CHECK_AUTOFIX("2×sin(33×3×sin(23)×cos(−pi))×sin(23234)×23⧸2−sin(−12) − 12−12",
                  "2⋅sin(33×3⋅sin(23)⋅cos(−pi))⋅sin(23234)⋅23⧸2−sin(−12) − 12−12");
    CHECK_AUTOFIX("2          ×pi×  pi + 2^12.000−2",
                  "2⋅pi⋅pi + 2^12.000−2");
    CHECK_AUTOFIX("1 meter in meter", "1 meter in meter");
    CHECK_AUTOFIX("1 meter IN meter", "1 meter IN meter");
    CHECK_AUTOFIX("sqrt(16)+cbrt(27)", "√(16)+∛(27)");
    CHECK_AUTOFIX("asqrt(16)+cbrtfoo(27)", "asqrt(16)+cbrtfoo(27)");

    // Selection text copied from result display may contain medium spaces and
    // paragraph separators; auto-fix should still produce a valid expression.
    ++eval_total_tests;
    const QString selectionText = QString::fromUtf8(
        "−1 ⋅ (2²) ⋅ 69\u2029− 39 × 2⁵ − 828 ⋅ (2²) + 39");
    const QString fixedSelectionText = eval->autoFix(selectionText);
    eval->setExpression(fixedSelectionText);
    Quantity selectionResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix display selection text eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedSelectionText.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(selectionResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("-4797")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix display selection text eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : -4797" << endl
                 << "\tAutoFix  : " << fixedSelectionText.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedFunctionPowerSelection =
        eval->autoFix(QString::fromUtf8("1+cos²(pi)"));
    eval->setExpression(fixedFunctionPowerSelection);
    const Quantity fixedFunctionPowerResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix function superscript eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedFunctionPowerSelection.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedFunctionPowerResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("2")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix function superscript eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : 2" << endl
                 << "\tAutoFix  : " << fixedFunctionPowerSelection.toUtf8().constData() << endl;
        }
    }
}

void test_auto_fix_shift_add_sub()
{
    CHECK_AUTOFIX("0x4f<<8+0x4c", "(0x4f << 8) + 0x4c");
    CHECK_AUTOFIX("0x4f >> 8 + 0x4c", "(0x4f >> 8) + 0x4c");
    CHECK_AUTOFIX("0x4f<<8-0x4c", "(0x4f << 8) - 0x4c");

    // Keep explicit RHS grouping untouched.
    CHECK_AUTOFIX("0x4f<<(8+0x4c)", "0x4f<<(8+0x4c)");
}

void test_comments()
{
    CHECK_EVAL("ncr(3;3) ? this is because foo",  "1");
    CHECK_EVAL("? this is a comment-only line", "NaN");
    CHECK_EVAL("   ? this is an indented comment-only line", "NaN");
    CHECK_EVAL("21", "21");
    CHECK_EVAL("? foo", "NaN");
    CHECK_EVAL("1+ans", "22");
}

void test_comment_and_description_edge_cases()
{
    // Autofix should normalize spacing before comments while preserving comment text.
    CHECK_AUTOFIX("x=1?foo", "x=1 ? foo");
    CHECK_AUTOFIX("x=1    ?    foo bar", "x=1 ? foo bar");
    CHECK_AUTOFIX("1+2 ? foo ? bar", "1+2 ? foo ? bar");

    // Interpreted and display-interpreted output should preserve comment tails.
    CHECK_INTERPRETED("2×pi ? calc area", "2⋅pi ? calc area");
    CHECK_DISPLAY_INTERPRETED(
        "2*3?c",
        QStringLiteral("2") + space + QString(UnicodeChars::MultiplicationSign)
            + space + QStringLiteral("3 ? c"));

    // Explicit empty description should be stored as empty.
    CHECK_EVAL("vardesc2 = 1 ? ", "1");
    CHECK_USERVAR_DESC("vardesc2", "");
    CHECK_USERFUNC_SET("funcdesc2(x) = x + 1 ? ");
    CHECK_USERFUNC_DESC("funcdesc2", "");

    // Description text should preserve punctuation and additional question marks.
    CHECK_EVAL("vardesc3 = 3 ? unit m/s ? approx", "3");
    CHECK_USERVAR_DESC("vardesc3", "unit m/s ? approx");
}

void test_user_functions()
{
    // Check user functions can be defined and used
    CHECK_USERFUNC_SET("func1(a;b) = a * b + 10");
    CHECK_USERFUNC_SET("g(x) = cos x^-pi");
    CHECK_USERFUNC_INTERPRETED("g", "cos(x^(-pi))");
    CHECK_EVAL("func1(2;5)", "20"); // = 2 * 5 + 10
    // Check some expected error conditions
    CHECK_EVAL_FAIL("func1()");
    CHECK_EVAL_FAIL("func1(2)");
    CHECK_EVAL_FAIL("func1(2;5;1)");

    // Check user functions can call other user functions
    CHECK_USERFUNC_SET("func2(a;b) = func1(a;b) - 10");
    CHECK_EVAL("func2(2;5)", "10"); // = (2 * 5 + 10) - 10

    // Check user functions can be redefined
    CHECK_USERFUNC_SET("func2(a;b) = 10 + func1(a;b)");
    CHECK_EVAL("func2(2;5)", "30"); // = 10 + (2 * 5 + 10)

    // Check user functions can refer to other user functions not defined
    CHECK_USERFUNC_SET("func2(a;b) = 10 * a + func3(a;b)");
    CHECK_EVAL_FAIL("func2(2;5)");
    CHECK_USERFUNC_SET("func3(a;b) = a - b");
    CHECK_EVAL("func2(2;5)", "17"); // = 10 * 2 + (2 - 5)

    // Check user functions can call builtin functions
    CHECK_USERFUNC_SET("func2(a;b) = sum(func1(a;b);5)");
    CHECK_EVAL("func2(2;5)", "25"); // = sum((2 * 5 + 10);5)

    // Check recursive user functions are not allowed
    CHECK_USERFUNC_SET("func_r(a) = a * func_r(a)");
    CHECK_EVAL_FAIL("func_r(1)");
    CHECK_USERFUNC_SET("func_r1(a) = a * func_r2(a)");
    CHECK_USERFUNC_SET("func_r2(a) = a + func_r1(a)");
    CHECK_EVAL_FAIL("func_r1(1)");

    // Check user functions can refer to user variables
    CHECK_USERFUNC_SET("func1(a;b) = a * b - var1");
    CHECK_EVAL_FAIL("func1(2;5)");
    CHECK_EVAL("var1 = 5", "5");
    CHECK_EVAL("func1(2;5)", "5");  // = 2 * 5 - 5

    // Check conflicts between the names of user functions arguments and user variables
    CHECK_EVAL("arg1 = 10", "10");
    CHECK_USERFUNC_SET("func1(arg1;arg2) = arg1 * arg2");
    CHECK_EVAL("func1(2;5)", "10"); // = 2 * 5 (not 10 * 5)

    // Check user functions names can not mask builtin functions
    CHECK_USERFUNC_SET_FAIL("sum(a;b) = a * b");

    // Check user functions names can not mask user variables
    CHECK_EVAL("var1 = 5", "5");
    CHECK_USERFUNC_SET_FAIL("var1(a;b) = a * b");

    // Check user functions can take no argument
    CHECK_EVAL("func1() = 2 * 10", "20");
    // NOTE: we use CHECK_EVAL to define the function, because its result is known at definition
    //       time. CHECK_USERFUNC_SET expects NaN, which is what is returned when the function body
    //       contains at least one component whose value can not be known at definition time
    //       (e.g., reference to user function arguments or to user/builtin functions).
    CHECK_EVAL("func1()", "20");    // = 2 * 5

    // Check optional descriptions on user function definitions.
    CHECK_USERFUNC_SET("funcdesc(x) = x + 1 ? Calculate Foo");
    CHECK_USERFUNC_DESC("funcdesc", "Calculate Foo");
    CHECK_EVAL("funcdesc(2)", "3");
    CHECK_USERFUNC_SET(
        QStringLiteral("testshift(x)=1") + space + QStringLiteral("<<")
            + space + QStringLiteral("x+0"));
    CHECK_EVAL("testshift(8)", "256");

    // Check redefining a function without description clears it.
    CHECK_USERFUNC_SET("funcdesc(x) = x + 2");
    CHECK_USERFUNC_DESC("funcdesc", "");

    // Check optional descriptions on user variable definitions.
    CHECK_EVAL("vardesc = 7 ? Lucky seven", "7");
    CHECK_USERVAR_DESC("vardesc", "Lucky seven");
    CHECK_EVAL("vardesc", "7");

    // Check redefining a variable without description clears it.
    CHECK_EVAL("vardesc = 8", "8");
    CHECK_USERVAR_DESC("vardesc", "");
}

void test_complex()
{
    // Check for basic complex number processing
    CHECK_EVAL("1j", "1j");
    CHECK_EVAL("0.1j", "0.1j");
    CHECK_EVAL(".1j", "0.1j");
    CHECK_EVAL("1E12j", "1000000000000j");
    CHECK_EVAL("0.1E12j", "100000000000j");
    CHECK_EVAL("1E-12j", "0.000000000001j");
    CHECK_EVAL("0.1E-12j", "0.0000000000001j");
    // Check for some bugs introduced by first versions of complex number processing
    CHECK_EVAL("0.1", "0.1");
    // Check for basic complex number evaluation
    CHECK_EVAL("(1+1j)*(1-1j)", "2");
    CHECK_EVAL("(1+1j)*(1+1j)", "2j");


    CHECK_EVAL("VARIANCE(1j;-1j)", "1");
    CHECK_EVAL("VARIANCE(1j;-1j;1;-1)", "1");
    CHECK_EVAL("VARIANCE(2j;-2j;1;-1)", "2.5");
}

void test_angle_mode(Settings* settings)
{
    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(pi)", "0");
    CHECK_EVAL("arcsin(-1)", "-1.57079632679489661923");
    CHECK_EVAL("arccos(0)", "1.57079632679489661923");
    CHECK_EVAL("tan(arctan(1))", "1");
    CHECK_EVAL("tan(arctan2(1;1))", "1");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL("sin(1j)", "1.17520119364380145688j");
    CHECK_EVAL("arcsin(-2)", "-1.57079632679489661923+1.31695789692481670863j");
    CHECK_EVAL("radian","1");
    CHECK_EVAL("degree","0.01745329251994329577");
    CHECK_EVAL("gradian","0.01570796326794896619");
    CHECK_EVAL("gon","0.01570796326794896619");
    CHECK_EVAL("turn","6.28318530717958647693");

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(180)", "0");
    CHECK_EVAL("arcsin(-1)", "-90");
    CHECK_EVAL("arccos(0)", "90");
    CHECK_EVAL("arctan(1)", "45");
    CHECK_EVAL("arctan2(1;1)", "45");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-90+75.4561292902168920041j");
    CHECK_EVAL("radian","57.2957795130823208768");
    CHECK_EVAL("degree","1");
    CHECK_EVAL("gradian","0.9");
    CHECK_EVAL("gon","0.9");
    CHECK_EVAL("turn","360");
    CHECK_EVAL_KNOWN_ISSUE("arcsin(0.25)", "14.47751218592992387877", 781);

    settings->angleUnit = 'g';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(200)", "0");
    CHECK_EVAL("arcsin(-1)", "-100");
    CHECK_EVAL("arccos(0)", "100");
    CHECK_EVAL("arctan(1)", "50");
    CHECK_EVAL("arctan2(1;1)", "50");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-100+83.84014365579654667122j");
    CHECK_EVAL("radian","63.66197723675813430755");
    CHECK_EVAL("degree","1.11111111111111111111");
    CHECK_EVAL("gradian","1");
    CHECK_EVAL("gon","1");
    CHECK_EVAL("turn","400");

    settings->angleUnit = 't';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(0.5)", "0");
    CHECK_EVAL("arcsin(-1)", "-0.25");
    CHECK_EVAL("arccos(0)", "0.25");
    CHECK_EVAL("arctan(1)", "0.125");
    CHECK_EVAL("arctan2(1;1)", "0.125");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-0.25+0.20960035913949136668j");
    CHECK_EVAL("radian","0.15915494309189533577");
    CHECK_EVAL("degree","0.00277777777777777778");
    CHECK_EVAL("gradian","0.0025");
    CHECK_EVAL("gon","0.0025");
    CHECK_EVAL("turn","1");
}

void test_implicit_multiplication()
{
    CHECK_EVAL("a = 5", "5");
    CHECK_EVAL("5a", "25");
    CHECK_EVAL("5.a", "25");
    CHECK_EVAL("5.0a", "25");
    CHECK_EVAL("5e2a", "2500");
    CHECK_EVAL_FAIL("a5");
    CHECK_EVAL("a 5", "25");
    CHECK_EVAL("2a^3", "250");
    CHECK_EVAL("b=2", "2");
    CHECK_EVAL_FAIL("ab");
    CHECK_EVAL("a b", "10");
    CHECK_EVAL("eps = 10", "10");
    CHECK_EVAL("5 eps", "50");
    CHECK_EVAL("Eren = 1001", "1001");
    CHECK_EVAL("7 Eren", "7007");
    CHECK_EVAL("Eren 5", "5005");
    CHECK_EVAL("f() = 123", "123");
    CHECK_EVAL("2f()", "246");
    CHECK_EVAL("5   5", "55");
    CHECK_INTERPRETED("a*b", "a⋅b");
    CHECK_INTERPRETED("a*(b)", "a⋅b");
    CHECK_INTERPRETED("sin(pi)*cos(pi)", "sin(pi)⋅cos(pi)");
    CHECK_INTERPRETED("f()*a", "f()⋅a");
    CHECK_INTERPRETED("sqrt(4)*a", "√(4)⋅a");
    CHECK_INTERPRETED("a*2", "a⋅2");
    CHECK_INTERPRETED("2*a", "2⋅a");
    CHECK_INTERPRETED("2*3", "2×3");

    // Check implicit multiplication between numbers fails
    // CHECK_EVAL_FAIL("10.   0.2");
    CHECK_EVAL_FAIL("10 0x10");
    CHECK_EVAL_FAIL("10 #10");
    CHECK_EVAL_FAIL("0b104");
    CHECK_EVAL_FAIL("0b10 4");
    CHECK_EVAL_FAIL("0b10 0x4");
    CHECK_EVAL_FAIL("0o109");
    CHECK_EVAL_FAIL("0o10 9");
    CHECK_EVAL_FAIL("0o10 0x9");
    // CHECK_EVAL_FAIL("12.12.12");
    CHECK_EVAL_FAIL("12e12.12");
    CHECK_EVAL("0b10a", "10");
    CHECK_EVAL("0o2a", "10");
    CHECK_EVAL("5(5)", "25");

    CHECK_EVAL("a sin(pi/2)", "5");
    CHECK_EVAL("a sqrt(4)",   "10");
    CHECK_EVAL("a sqrt(a^2)", "25");
    CHECK_INTERPRETED("2×sin pi", "2⋅sin(pi)");
    CHECK_INTERPRETED("a sin(pi/2)", "a⋅sin(pi/2)");
    CHECK_INTERPRETED("a sqrt(4)", "a⋅√(4)");
    CHECK_INTERPRETED("a sqrt(a^2)", "a⋅√(a^2)");

    /* Tests issue 538 */
    /* 3 sin (3 pi) was evaluated but not 3 sin (3) */
    CHECK_EVAL("3 sin (3 pi)", "0");
    CHECK_EVAL("3 sin (3)",    "0.4233600241796016663");
    CHECK_INTERPRETED("3 sin (3 pi)", "3⋅sin(3⋅pi)");
    CHECK_INTERPRETED("3 sin (3)", "3⋅sin(3)");

    CHECK_EVAL("2 (2 + 1)", "6");
    CHECK_EVAL("2 (a)", "10");
    CHECK_INTERPRETED("2 (2 + 1)", "2⋅(2+1)");
    CHECK_INTERPRETED("2 (a)", "2⋅a");
    CHECK_INTERPRETED("(1+2)(3+4)", "(1+2)⋅(3+4)");
    CHECK_INTERPRETED("(-1+2)(3-4)", "(-1+2)⋅(3-4)");

    /* Tests issue 598 */
    CHECK_EVAL("2(a)^3", "250");
    CHECK_INTERPRETED("2(a)^3", "2⋅a^3");

    CHECK_EVAL("6/2(2+1)", "9");
    CHECK_INTERPRETED("6/2(2+1)", "(6/2)⋅(2+1)");
    CHECK_INTERPRETED("1/2 sqrt(3)", "(1/2)⋅√(3)");
    CHECK_INTERPRETED("1/2(2+3)", "(1/2)⋅(2+3)");
    CHECK_INTERPRETED("2/3(4/5)", "(2/3)⋅((4/5))");
    CHECK_INTERPRETED("10\\3(2)", "(10\\3)⋅2");
    CHECK_INTERPRETED("-2(3+4)", "-(2⋅(3+4))");
    CHECK_INTERPRETED("~2(3)", "~(2⋅3)");
    CHECK_INTERPRETED("2^2(2)", "2^2⋅2");
    CHECK_INTERPRETED("2^2(2)(2)", "2^2⋅(2⋅2)");
    CHECK_INTERPRETED("2^2(2*2)", "2^2⋅(2×2)");
    CHECK_INTERPRETED("2^2(2)+3", "2^2⋅2+3");
    CHECK_INTERPRETED("2 sin pi cos pi + 2", "2⋅sin(pi)⋅cos(pi)+2");
    CHECK_INTERPRETED("1 meter second^-1", "1⋅meter⋅second^(-1)");
    CHECK_INTERPRETED("1/second^2", "1/second^2");
    CHECK_INTERPRETED("1/2^3", "1/2^3");
    CHECK_INTERPRETED("2^12!", "2^(12!)");
    CHECK_INTERPRETED("2^12.1!", "2^(12.1!)");
    CHECK_INTERPRETED("1/2!", "1/(2!)");
    CHECK_INTERPRETED("2^12-2", "2^12-2");
    CHECK_INTERPRETED("2^12.-2", "2^12-2");
    CHECK_INTERPRETED("2^12.000-2", "2^12-2");
    CHECK_INTERPRETED("2^12.12", "2^(12.12)");
    CHECK_INTERPRETED("2^12.000-2+1/(1×2^3×3)-2^12!+2^12.1!",
                      "2^12-2+1/(1⋅(2^3)⋅3)-2^(12!)+2^(12.1!)");
    CHECK_INTERPRETED(QString::fromUtf8("pi  −−−−−3"), "pi-3");
    CHECK_INTERPRETED(QString::fromUtf8("pi  −−−−−−3"), "pi+3");
    CHECK_INTERPRETED("1/(1×2^3×3)", "1/(1⋅(2^3)⋅3)");
    CHECK_INTERPRETED("x=1/2 sqrt(3)", "x=(1/2)⋅√(3)");
    CHECK_INTERPRETED("g(t)=t/2 sqrt(3)", "g(t)=(t/2)⋅√(3)");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2",
                      "sin(23)⋅cos(232323)⋅pi⋅pi⋅2");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2×cos pi×pi×23",
                      "sin(23)⋅cos(232323)⋅pi⋅pi⋅2⋅cos(pi)⋅pi⋅23");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2×cos pi×pi×23  23 × 323",
                      "sin(23)⋅cos(232323)⋅pi⋅pi⋅2⋅cos(pi)⋅pi⋅2323×323");
    CHECK_EVAL("2^2(2)", "8");
    CHECK_EVAL("2^2(2)(2)", "16");
    CHECK_EVAL("2^2(2*2)", "16");
    CHECK_EVAL("2^2(2)+3", "11");
    CHECK_EVAL_FAIL("pi (2)");

    CHECK_EVAL_KNOWN_ISSUE("2(7.5−0.5)+4(12−0.75)", "59", 1155);
    CHECK_EVAL_KNOWN_ISSUE("2+2(5)", "12", 1155);
}

void test_display_interpreted_spacing()
{
    const QString dot(QString::fromUtf8("⋅"));
    const QString multiplication = QString(UnicodeChars::MultiplicationSign);
    const QString unicodeMinusSign(UnicodeChars::MinusSign);
    const QString dotSpaced = space + dot + space;
    const QString plus = space + QStringLiteral("+") + space;
    const QString minus = space + unicodeMinusSign + space;
    const QString times = space + multiplication + space;
    const QString divide = space + QStringLiteral("/") + space;
    const QString integerDivide = space + QStringLiteral("\\") + space;
    const QString equals = space + QStringLiteral("=") + space;
    const QString shiftLeft = space + QStringLiteral("<<") + space;
    const QString shiftRight = space + QStringLiteral(">>") + space;

    CHECK_DISPLAY_INTERPRETED(
        "sin(2⋅pi+3)-1+2×3⋅sin(pi)",
        QString::fromUtf8("sin(2")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("3)")
            + minus
            + QStringLiteral("1")
            + plus
            + QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("sin(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2×sin pi"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 sin pi cos pi + 2"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 sin pi cos pi + 2 × 23 / cos pi − sin −pi^2"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + plus
            + QStringLiteral("2")
            + times
            + QStringLiteral("23")
            + divide
            + QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("sin((")
            + unicodeMinusSign
            + QString::fromUtf8("pi)²)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("23/2+10\\3"),
        QStringLiteral("23")
            + divide
            + QStringLiteral("2")
            + plus
            + QStringLiteral("10")
            + integerDivide
            + QStringLiteral("3"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1<<2"),
        QStringLiteral("1")
            + shiftLeft
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1>>2"),
        QStringLiteral("1")
            + shiftRight
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("sin -12"),
        QString::fromUtf8("sin(") + unicodeMinusSign + QStringLiteral("12)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1 meter second^-1"),
        QStringLiteral("1")
            + dotSpaced
            + QStringLiteral("meter")
            + dotSpaced
            + QString::fromUtf8("second⁻¹"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/second^2"),
        QStringLiteral("1")
            + divide
            + QString::fromUtf8("second²"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("cos(pi)^2"),
        QStringLiteral("cos")
            + QString::fromUtf8("²")
            + QStringLiteral("(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/2^3"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("2³"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12!"),
        QStringLiteral("2^(12!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.1!"),
        QStringLiteral("2^(12.1!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/2!"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("(2!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.000-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.000-2+1/(1×2^3×3)-2^12!+2^12.1!"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2")
            + plus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(1")
            + dotSpaced
            + QStringLiteral("(2³)")
            + dotSpaced
            + QStringLiteral("3)")
            + minus
            + QStringLiteral("2^(12!)")
            + plus
            + QStringLiteral("2^(12.1!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/(1×2^3×3)"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("(1")
            + dotSpaced
            + QStringLiteral("(2³)")
            + dotSpaced
            + QStringLiteral("3)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^123"),
        QString::fromUtf8("2¹²³"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.12"),
        QStringLiteral("2^(12.12)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*e*e-1"),
        QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("e²")
            + minus
            + QStringLiteral("1"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*2*2+pi*pi^2+average(2;3;4)"),
        QString::fromUtf8("2³")
            + plus
            + QString::fromUtf8("pi³")
            + plus
            + QStringLiteral("average(2;3;4)")
            + plus
            + QStringLiteral("1"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("1+2*2*2+π*π^2+average(2;3;4)"),
        QString::fromUtf8("2³")
            + plus
            + QString::fromUtf8("π³")
            + plus
            + QStringLiteral("average(2;3;4)")
            + plus
            + QStringLiteral("1"));
    CHECK_EVAL("foo1=2", "2");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("foo1 foo1 + foo1 foo1 foo1 foo1 foo1 × 2 × foo1^4"),
        QString::fromUtf8("foo1²")
            + plus
            + QString::fromUtf8("foo1⁹")
            + times
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("sin(pi)*sin(pi)"),
        QString::fromUtf8("sin²(pi)"));
    CHECK_USERFUNC_SET("f(x)=x");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("f(pi)*f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("f(pi)⋅f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("f(pi) ⋅ f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("sin(pi)*2*sin(pi)^2"),
        QString::fromUtf8("sin³(pi)")
            + dotSpaced
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+pi pi pi/2+3"),
        QStringLiteral("0.5")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QStringLiteral("4"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*3/pi*pi*pi/2+3"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("4"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*3/pi*pi*pi*pi/2+3"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("4"));
    CHECK_EVAL("abc()=2", "2");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+(2*3/abc())*abc()*abc()/2+3*pi*pi*2+1"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("abc()")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+(2*3/abc())*4*abc()*abc()/2+3*pi*pi*2+1"),
        QStringLiteral("12")
            + dotSpaced
            + QStringLiteral("abc()")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3+1+2*3/e*4*e*e/2+3*pi*pi*2+1+cos(pi)^8/cos(pi)^2"),
        QStringLiteral("12")
            + dotSpaced
            + QStringLiteral("e")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QString::fromUtf8("cos⁶(pi)")
            + plus
            + QStringLiteral("7"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3*pi*pi*pi/pi*pi*pi/pi+1+e^5/e^2-23-1/(cos(pi)*cos(pi))"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QString::fromUtf8("cos²(pi)")
            + minus
            + QStringLiteral("20"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3*pi*pi*pi/pi*pi*pi/pi+1+e^5/e^2-23-1/(5.5+3*cos(pi)*cos(pi)+2)"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(3")
            + dotSpaced
            + QString::fromUtf8("cos²(pi)")
            + plus
            + QStringLiteral("7.5)")
            + minus
            + QStringLiteral("20"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1-5.5*2+3*pi*pi*pi/pi/pi+1+e^5/e^2-23-1/(5.67+4*cos(pi)*cos(pi)+2-8/2+1)+9/3+2"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(4")
            + dotSpaced
            + QString::fromUtf8("cos²(pi)")
            + plus
            + QStringLiteral("4.67)")
            + minus
            + QStringLiteral("27"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+cos(-pi^2)+pi+3+pi"),
        QStringLiteral("cos(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²)")
            + plus
            + QStringLiteral("2")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("5"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1+1e78+cos(pi)-1e78-1"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-1e7+cos(pi)-1e7+1e7-1+1e7"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-100+cos(pi)-100+100-1+100"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-e+cos(pi)-e+e-1+e"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-2*3*2"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + times
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*3*(-2)"),
        QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + times
            + QStringLiteral("(")
            + QString(UnicodeChars::MinusSign)
            + QStringLiteral("2)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("−1/2/cos pi −1.345 sin(−pi^2+2.5/cos(e))  × 4 sin(−pi^2+2.5/cos(e))+ 4 tan(2) ×5 tan(2) ×6 tan 2 ×7 −1.23"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + slash
            + QStringLiteral("cos(")
            + QStringLiteral("pi)")
            + minus
            + QStringLiteral("1.345")
            + dotSpaced
            + QStringLiteral("sin(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + dotSpaced
            + QStringLiteral("4")
            + dotSpaced
            + QStringLiteral("sin(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + plus
            + QStringLiteral("4")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("5")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("6")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("7")
            + minus
            + QStringLiteral("1.23"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("−1/2/cos pi −1.345 sin(−pi^2+2.5/cos(e))  × 4 sin(−pi^2+2.5/cos(e))+ 4 tan(2) ×5 tan(2) ×6 tan 2 ×7 −1.23"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + slash
            + QStringLiteral("cos(")
            + QStringLiteral("pi)")
            + minus
            + QStringLiteral("5.38")
            + dotSpaced
            + QStringLiteral("sin²(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + plus
            + QStringLiteral("840")
            + dotSpaced
            + QStringLiteral("tan³(2)")
            + minus
            + QStringLiteral("1.23"));
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("-1+1e78+cos(pi)-1e78-1"));
    Quantity largeCancellationBase = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tlarge cancellation base eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString simplifiedLargeCancellationExpr =
            Evaluator::simplifyInterpretedExpression(eval->interpretedExpression());
        eval->setExpression(simplifiedLargeCancellationExpr);
        Quantity largeCancellationSimplified = eval->evalNoAssign();
        const QString simplifiedValue = DMath::format(largeCancellationSimplified, Format::Fixed());
        if (!eval->error().isEmpty() || simplifiedValue != QLatin1String("-3")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tlarge cancellation simplified eval\t[NEW]" << endl
                 << "\tSimplified expression: " << simplifiedLargeCancellationExpr.toUtf8().constData() << endl
                 << "\tResult   : " << simplifiedValue.toUtf8().constData() << endl
                 << "\tExpected : -3" << endl;
        }
    }
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("12×1/2/3/4×23+23−sin(2)+23 cos(23)"),
        QStringLiteral("12")
            + times
            + QStringLiteral("1")
            + divide
            + QStringLiteral("2")
            + divide
            + QStringLiteral("3")
            + divide
            + QStringLiteral("4")
            + times
            + QStringLiteral("23")
            + plus
            + QStringLiteral("23")
            + minus
            + QStringLiteral("sin(2)")
            + plus
            + QStringLiteral("23")
            + dotSpaced
            + QStringLiteral("cos(23)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("x=1/2 sqrt(3)"),
        QStringLiteral("x")
            + equals
            + QStringLiteral("(1")
            + divide
            + QStringLiteral("2)")
            + dotSpaced
            + QStringLiteral("√(3)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("g(t)=t/2 sqrt(3)"),
        QStringLiteral("g(t)")
            + equals
            + QStringLiteral("(t")
            + divide
            + QStringLiteral("2)")
            + dotSpaced
            + QStringLiteral("√(3)"));
}

void test_format()
{
    CHECK_EVAL("bin(123)", "0b1111011");
    CHECK_EVAL("oct(123)", "0o173");
    CHECK_EVAL("hex(123)", "0x7B");

    CHECK_EVAL("binpad(15)", "0b00001111");
    CHECK_EVAL("hexpad(15)", "0x0F");
    CHECK_EVAL("octpad(15)", "0o017");

    CHECK_EVAL("binpad(1536)", "0b0000011000000000");
    CHECK_EVAL("hexpad(1536)", "0x0600");
    CHECK_EVAL("octpad(1536)", "0o003000");

    CHECK_EVAL("binpad(1536; 32)", "0b00000000000000000000011000000000");
    CHECK_EVAL("hexpad(1536; 32)", "0x00000600");
    CHECK_EVAL("octpad(1536; 32)", "0o00000003000");

    CHECK_EVAL("binpad(-5)", "-0b00000101");
    CHECK_EVAL("hexpad(-255; 16)", "-0x00FF");
    CHECK_EVAL("octpad(-9)", "-0o011");

    // Explicit widths below current representation should not truncate.
    CHECK_EVAL("binpad(255; 7)", "0b11111111");
    CHECK_EVAL("hexpad(255; 4)", "0xFF");
    CHECK_EVAL("octpad(511; 8)", "0o777");

    // Width rounding per radix digit size.
    CHECK_EVAL("binpad(5; 9)", "0b000000101");
    CHECK_EVAL("hexpad(5; 9)", "0x005");
    CHECK_EVAL("octpad(5; 9)", "0o005");
    CHECK_EVAL("octpad(9; 10)", "0o0011");

    // Zero and byte boundary transitions.
    CHECK_EVAL("binpad(0)", "0");
    CHECK_EVAL("hexpad(0)", "0");
    CHECK_EVAL("octpad(0)", "0");
    CHECK_EVAL("binpad(256)", "0b0000000100000000");
    CHECK_EVAL("hexpad(256)", "0x0100");
    CHECK_EVAL("octpad(256)", "0o000400");

    CHECK_EVAL_FAIL("binpad(1.5)");
    CHECK_EVAL_FAIL("binpad(1 meter)");
    CHECK_EVAL_FAIL("hexpad(1+j)");
    CHECK_EVAL_FAIL("hexpad(10; 0)");
    CHECK_EVAL_FAIL("octpad(10; -8)");
    CHECK_EVAL_FAIL("binpad(10; 3.5)");
    CHECK_EVAL_FAIL("binpad(10; 1e1000)");

    CHECK_EVAL("polar(3+4j)", "5 * exp(j*0.92729521800161223243)");
}


void test_datetime()
{
    // NOTE We cannot test datetime() with only 1 argument as it depends on current timezone (and DST?)

    CHECK_EVAL("datetime(1538242871;    0)", "20180929.174111");
    CHECK_EVAL("datetime(1514761200;    1)", "20180101.000000");
    CHECK_EVAL("datetime(1514764800;   -1)", "20171231.230000");
    CHECK_EVAL("datetime(1551464695;13.75)", "20190302.080955");  // GMT +13:45
    CHECK_EVAL("datetime(1551464695; -3.5)", "20190301.145455");  // GMT -03:30
}

void test_epoch()
{
    // NOTE We avoid testing epoch() with only 1 argument because it depends on local timezone (and DST).
    CHECK_EVAL("epoch(20180929.174111; 0)", "1538242871");
    CHECK_EVAL("epoch(20180101.000000; 1)", "1514761200");
    CHECK_EVAL("epoch(20171231.230000; -1)", "1514764800");
    CHECK_EVAL("epoch(20190302.080955; 13.75)", "1551464695");
    CHECK_EVAL("epoch(20190301.145455; -3.5)", "1551464695");
}

void test_expression_operator_normalization()
{
    const QString normalized = EditorUtils::normalizeExpressionOperators(
        QString::fromUtf8("1＋2 8/4÷2 "
                          "9-4 9－4 9﹣4 9‐4 9‑4 9–4 9—4 9―4 9⁃4 "
                          "2∗3·4⋅5∙6*7⨉8⨯9✕10✖11"));
    const std::string normalizedStd = normalized.toStdString();
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizeExpressionOperators",
                           normalizedStd,
                           "1+2 8⧸4⧸2 "
                           "9−4 9−4 9−4 9−4 9−4 9−4 9−4 9−4 9−4 "
                           "2×3×4×5×6×7×8×9×10×11",
                           eval_failed_tests, eval_new_failed_tests);

    const QStringList parsed = EditorUtils::parsePastedExpressions(
        QString::fromUtf8("1＋2\n"
                          "8/4\n"
                          "10÷2\n"
                          "9-4\n"
                          "9－4\n"
                          "9﹣4\n"
                          "9‐4\n"
                          "9‑4\n"
                          "9–4\n"
                          "9—4\n"
                          "9―4\n"
                          "9⁃4\n"
                          "2∗3\n"
                          "4·5\n\n"
                          "6✖7"));
    if (parsed.size() != 15
        || parsed.at(0) != QString::fromUtf8("1+2")
        || parsed.at(1) != QString::fromUtf8("8⧸4")
        || parsed.at(2) != QString::fromUtf8("10⧸2")
        || parsed.at(3) != QString::fromUtf8("9−4")
        || parsed.at(4) != QString::fromUtf8("9−4")
        || parsed.at(5) != QString::fromUtf8("9−4")
        || parsed.at(6) != QString::fromUtf8("9−4")
        || parsed.at(7) != QString::fromUtf8("9−4")
        || parsed.at(8) != QString::fromUtf8("9−4")
        || parsed.at(9) != QString::fromUtf8("9−4")
        || parsed.at(10) != QString::fromUtf8("9−4")
        || parsed.at(11) != QString::fromUtf8("9−4")
        || parsed.at(12) != QString::fromUtf8("2×3")
        || parsed.at(13) != QString::fromUtf8("4×5")
        || parsed.at(14) != QString::fromUtf8("6×7")) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tparsePastedExpressions\t[NEW]" << endl
             << "\tResult   : " << parsed.join("|").toUtf8().constData() << endl
             << "\tExpected : 1+2|8⧸4|10⧸2|"
                "9−4|9−4|9−4|9−4|9−4|9−4|9−4|9−4|9−4|"
                "2×3|4×5|6×7" << endl;
    } else {
        ++eval_total_tests;
    }

    const QString editorNormalized = EditorUtils::normalizeExpressionOperatorsForEditorInput(
        QString::fromUtf8("2⋅3 4·5 6*7 8/4 10÷2 9⧸3 𝜋 𝝅 𝞹 𝛑 sqrt cbrt asqrt cbrtfoo"));
    const std::string editorNormalizedStd = editorNormalized.toStdString();
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizeExpressionOperatorsForEditorInput",
                           editorNormalizedStd,
                           "2⋅3 4×5 6×7 8/4 10/2 9/3 π π π π √ ∛ asqrt cbrtfoo",
                           eval_failed_tests, eval_new_failed_tests);

    const QStringList parsedForEditor = EditorUtils::parsePastedExpressionsForEditorInput(
        QString::fromUtf8("2⋅3\n"
                          "4·5\n"
                          "6*7\n"
                          "8/4\n"
                          "10÷2\n"
                          "9⧸3\n"
                          "𝜋+1\n"
                          "𝝅+1\n"
                          "𝞹+1\n"
                          "𝛑+1\n"
                          "sqrt(4)\n"
                          "cbrt(8)\n"
                          "asqrt(4)"));
    if (parsedForEditor.size() != 13
        || parsedForEditor.at(0) != QString::fromUtf8("2⋅3")
        || parsedForEditor.at(1) != QString::fromUtf8("4×5")
        || parsedForEditor.at(2) != QString::fromUtf8("6×7")
        || parsedForEditor.at(3) != QString::fromUtf8("8/4")
        || parsedForEditor.at(4) != QString::fromUtf8("10/2")
        || parsedForEditor.at(5) != QString::fromUtf8("9/3")
        || parsedForEditor.at(6) != QString::fromUtf8("π+1")
        || parsedForEditor.at(7) != QString::fromUtf8("π+1")
        || parsedForEditor.at(8) != QString::fromUtf8("π+1")
        || parsedForEditor.at(9) != QString::fromUtf8("π+1")
        || parsedForEditor.at(10) != QString::fromUtf8("√(4)")
        || parsedForEditor.at(11) != QString::fromUtf8("∛(8)")
        || parsedForEditor.at(12) != QString::fromUtf8("asqrt(4)")) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tparsePastedExpressionsForEditorInput\t[NEW]" << endl
             << "\tResult   : " << parsedForEditor.join("|").toUtf8().constData() << endl
             << "\tExpected : 2⋅3|4×5|6×7|8/4|10/2|9/3|π+1|π+1|π+1|π+1|√(4)|∛(8)|asqrt(4)" << endl;
    } else {
        ++eval_total_tests;
    }

    const QString piDisplay = UnicodeChars::normalizePiForDisplay(
        QString::fromUtf8("pi 𝜋 𝝅 𝞹 𝛑 alpha_pi beta pi2 pi² -pi +pi/pi"));
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizePiForDisplay",
                           piDisplay.toStdString(),
                           "π π π π π alpha_pi beta pi2 π² -π +π/π",
                           eval_failed_tests, eval_new_failed_tests);

    ++eval_total_tests;
    eval->setExpression(QString::fromUtf8("2pi pi^2"));
    eval->evalUpdateAns();
    const QString threadExpressionDisplay = UnicodeChars::normalizePiForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression()));
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizePiForDisplay thread expression",
                           threadExpressionDisplay.toStdString(),
                           "2 ⋅ π ⋅ π²",
                           eval_failed_tests, eval_new_failed_tests);

    ++eval_total_tests;
    QString ignoredTrailing;
    const bool ignoreTrailingPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1+2+"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingPlus || ignoredTrailing != QString::fromUtf8("1+2")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing plus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingPlus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1+2" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1++++"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiPlus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi plus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiPlus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("2-------"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiMinus || ignoredTrailing != QString::fromUtf8("2")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 2" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSingleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSingleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSingleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDoubleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1--"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDoubleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken double minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDoubleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingTripleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1---"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingTripleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken triple minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingTripleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingUnicodeMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(
            QString::fromUtf8("1−−−"),
            &ignoredTrailing);
    if (!ignoreTrailingUnicodeMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken unicode minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingUnicodeMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMixedPlusMinusA =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1++--++-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMixedPlusMinusA || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken mixed +/- A\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMixedPlusMinusA ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMixedPlusMinusB =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1--+++-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMixedPlusMinusB || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken mixed +/- B\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMixedPlusMinusB ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSingleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1*"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSingleStar || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single star\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSingleStar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDoubleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1**"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDoubleStar || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken double star\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDoubleStar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingCaret =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1^"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingCaret || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken caret\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingCaret ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingBackslash =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3\\"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingBackslash || ignoredTrailing != QString::fromUtf8("3")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing backslash\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingBackslash ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 3" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSinglePar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("4("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSinglePar || ignoredTrailing != QString::fromUtf8("4")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSinglePar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 4" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiPar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5(((("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiPar || ignoredTrailing != QString::fromUtf8("5")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiPar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 5" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDiv =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3/"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDiv || ignoredTrailing != QString::fromUtf8("3")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing slash\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDiv ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 3" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingPar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5+cos pi ("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingPar || ignoredTrailing != QString::fromUtf8("5+cos pi")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing open parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingPar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 5+cos pi" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1+2+*"),
                                                             &ignoredTrailing);
    if (rejectTrailingStar) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject +*\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingDoubleDiv =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3//"),
                                                             &ignoredTrailing);
    if (rejectTrailingDoubleDiv) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject //\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectInvalidParen =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5+cos pi ()"),
                                                             &ignoredTrailing);
    if (rejectInvalidParen) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject ()\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingTripleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1***"),
                                                             &ignoredTrailing);
    if (rejectTrailingTripleStar) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject ***\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectClosingParen =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1)"),
                                                             &ignoredTrailing);
    if (rejectClosingParen) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject )\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectClosingParens =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1)))"),
                                                             &ignoredTrailing);
    if (rejectClosingParens) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject )))\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectOnlyPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("+"),
                                                             &ignoredTrailing);
    if (rejectOnlyPlus) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject +\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectOnlyMixedPlusMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("-+-+-++--+"),
                                                             &ignoredTrailing);
    if (rejectOnlyMixedPlusMinus) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject mixed +/- only\t[NEW]" << endl;
    }
}

void test_session_history_limit()
{
    Settings* settings = Settings::instance();
    const int oldMaxHistoryEntries = settings->maxHistoryEntries;

    settings->maxHistoryEntries = 3;
    Session session;
    session.addHistoryEntry(HistoryEntry("1+1", Quantity(2)));
    session.addHistoryEntry(HistoryEntry("2+2", Quantity(4)));
    session.addHistoryEntry(HistoryEntry("3+3", Quantity(6)));
    session.addHistoryEntry(HistoryEntry("4+4", Quantity(8)));
    session.addHistoryEntry(HistoryEntry("5+5", Quantity(10)));

    ++eval_total_tests;
    const QList<HistoryEntry> history = session.historyToList();
    if (history.size() != 3 || history.at(0).expr() != "3+3" || history.at(2).expr() != "5+5") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on add\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = 2;
    session.applyHistoryLimit();

    ++eval_total_tests;
    const QList<HistoryEntry> trimmedHistory = session.historyToList();
    if (trimmedHistory.size() != 2 || trimmedHistory.at(0).expr() != "4+4" || trimmedHistory.at(1).expr() != "5+5") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on apply limit\t[NEW]" << endl;
    }

    QJsonObject json;
    QJsonArray histEntries;
    for (int i = 1; i <= 4; ++i) {
        QJsonObject entry;
        HistoryEntry(QString::number(i), Quantity(i)).serialize(entry);
        histEntries.append(entry);
    }
    json["version"] = QString(SPEEDCRUNCH_VERSION);
    json["history"] = histEntries;

    Session loaded;
    loaded.deSerialize(json, false);

    ++eval_total_tests;
    const QList<HistoryEntry> loadedHistory = loaded.historyToList();
    if (loadedHistory.size() != 2 || loadedHistory.at(0).expr() != "3" || loadedHistory.at(1).expr() != "4") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on deserialize\t[NEW]" << endl;
    }

    QJsonObject jsonWithCommentTail;
    QJsonArray histWithCommentTail;
    {
        QJsonObject entry;
        HistoryEntry("21", Quantity(21)).serialize(entry);
        histWithCommentTail.append(entry);
    }
    {
        QJsonObject entry;
        HistoryEntry("? foo", CMath::nan()).serialize(entry);
        histWithCommentTail.append(entry);
    }
    jsonWithCommentTail["version"] = QString(SPEEDCRUNCH_VERSION);
    jsonWithCommentTail["history"] = histWithCommentTail;

    Session loadedWithCommentTail;
    loadedWithCommentTail.deSerialize(jsonWithCommentTail, false);

    ++eval_total_tests;
    if (!loadedWithCommentTail.hasVariable("ans")
        || DMath::format(loadedWithCommentTail.getVariable("ans").value(), Format::Fixed()) != "21") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tans recovery on deserialize with comment tail\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = 0;
    Session unlimited;
    for (int i = 0; i < 4; ++i)
        unlimited.addHistoryEntry(HistoryEntry(QString("u%1").arg(i), Quantity(i)));

    ++eval_total_tests;
    if (unlimited.historyToList().size() != 4) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory unlimited mode\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = oldMaxHistoryEntries;
}

void test_session_deserialize_without_history()
{
    Session source;
    Session restored;
    Session* const previousSession = const_cast<Session*>(Evaluator::instance()->session());

    Evaluator::instance()->setSession(&source);
    Evaluator::instance()->initializeBuiltInVariables();

    source.addVariable(Variable("persistedVar", Quantity(42), Variable::UserDefined,
                                "Saved variable"));
    source.addUserFunction(UserFunction("persistedFunc", QStringList() << "x", "x+1"));
    source.addHistoryEntry(HistoryEntry("1+1", Quantity(2)));

    QJsonObject json;
    source.serialize(json);
    json.remove("history");

    Evaluator::instance()->setSession(&restored);
    restored.deSerialize(json, false);

    ++eval_total_tests;
    const bool hasVar = restored.hasVariable("persistedVar")
        && DMath::format(restored.getVariable("persistedVar").value(), Format::Fixed()) == "42";
    const bool hasVarDescription = restored.hasVariable("persistedVar")
        && restored.getVariable("persistedVar").description() == "Saved variable";
    const bool hasFunc = restored.hasUserFunction("persistedFunc");
    const bool hasNoHistory = restored.historyToList().isEmpty();
    if (!hasVar || !hasVarDescription || !hasFunc || !hasNoHistory) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsession deserialize without history keeps vars/functions\t[NEW]" << endl;
    }

    Evaluator::instance()->setSession(previousSession);
    if (previousSession)
        Evaluator::instance()->initializeBuiltInVariables();
}

void test_function_usage_tooltip()
{
    auto checkTooltip = [](const QString& expression,
                           int cursorPosition,
                           const QString& expected,
                           const char* label)
    {
        ++eval_total_tests;
        const QString actual = FunctionTooltipUtils::activeFunctionUsageTooltip(
            eval, expression, cursorPosition);
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tActual   : " << actual.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    };

    checkTooltip(
        "gcd(",
        4,
        "<b>gcd</b>(<b>n<sub>1</sub></b>; n<sub>2</sub>; ...)",
        "tooltip shows built-in usage with HTML subscript"
    );

    checkTooltip(
        "gcd(3;",
        6,
        "<b>gcd</b>(n<sub>1</sub>; <b>n<sub>2</sub></b>; ...)",
        "tooltip highlights second parameter after first separator"
    );

    checkTooltip(
        "gcd(3;4;",
        8,
        "<b>gcd</b>(n<sub>1</sub>; n<sub>2</sub>; <b>...</b>)",
        "tooltip highlights variadic placeholder when typing further arguments"
    );

    checkTooltip(
        "gcd(abs(3);",
        11,
        "<b>gcd</b>(n<sub>1</sub>; <b>n<sub>2</sub></b>; ...)",
        "tooltip tracks outer function in nested calls"
    );

    checkTooltip(
        "lcm(",
        4,
        "<b>lcm</b>(<b>n<sub>1</sub></b>; n<sub>2</sub>; ...)",
        "tooltip shows usage for lcm"
    );

    checkTooltip(
        "2+3",
        3,
        "",
        "no tooltip outside function calls"
    );

    eval->setExpression("fff(pao;barro;gis)=pao+barro+gis");
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsetup user function for tooltip\t[NEW]" << endl
             << "\tError: " << eval->error().toUtf8().constData() << endl;
        return;
    }

    checkTooltip(
        "fff(1;",
        6,
        "<b>fff</b>(pao;<b>barro</b>;gis)",
        "tooltip highlights current user-function parameter"
    );
}

void test_grouped_numeric_literal_display_format()
{
    Settings* settings = Settings::instance();
    const int oldDigitGrouping = settings->digitGrouping;
    const bool oldDigitGroupingIntegerPartOnly = settings->digitGroupingIntegerPartOnly;

    settings->digitGrouping = 1;
    settings->digitGroupingIntegerPartOnly = false;

    ++eval_total_tests;
    const QString groupedLiteral = NumberFormatter::formatNumericLiteralForDisplay(QStringLiteral("234936"));
    if (groupedLiteral != QStringLiteral("234 936")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tgroup numeric literal for display\t[NEW]" << endl
             << "\tResult   : " << groupedLiteral.toUtf8().constData() << endl
             << "\tExpected : 234 936" << endl;
    }

    eval->setExpression("234234+234+234+234");
    const Quantity value = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate grouped dedup baseline\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString groupedResult =
            NumberFormatter::formatNumericLiteralForDisplay(NumberFormatter::format(value));
        if (groupedResult != groupedLiteral) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tgrouped literal matches formatted result\t[NEW]" << endl
                 << "\tLiteral  : " << groupedLiteral.toUtf8().constData() << endl
                 << "\tResult   : " << groupedResult.toUtf8().constData() << endl;
        }
    }

    settings->digitGrouping = oldDigitGrouping;
    settings->digitGroupingIntegerPartOnly = oldDigitGroupingIntegerPartOnly;
}

void test_non_informative_numeric_simplified_row_suppression()
{
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress simplified line for 1+2-3/2",
        QStringLiteral("1+2-3/2"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress simplified line for 1+.5-1.5+3",
        QStringLiteral("1+.5-1.5+3"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic simplification",
        QStringLiteral("2*e*e-1"), false);
}


int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    Settings* settings = Settings::instance();
    settings->angleUnit = 'r';
    settings->setRadixCharacter('.');
    settings->complexNumbers = false;
    DMath::complexMode = false;

    eval = Evaluator::instance();

    eval->initializeBuiltInVariables();

    test_constants();
    test_exponentiation();
    test_unary();
    test_binary();
    test_bitwise_complement_operator();

    test_divide_by_zero();
    test_radix_char();

    test_thousand_sep();
    test_sexagesimal();
    test_rational_format();

    test_function_basic();
    test_function_trig();
    test_function_stat();
    test_function_logic();
    test_function_discrete();
    test_function_simplified();

    test_auto_fix_parentheses();
    test_auto_fix_ans();
    test_auto_fix_trailing_equal();
    test_auto_fix_powers();
    test_auto_fix_shift_add_sub();
    test_auto_fix_untouch();

    test_comments();
    test_comment_and_description_edge_cases();

    test_user_functions();

    test_implicit_multiplication();
    test_display_interpreted_spacing();

    settings->complexNumbers = true;
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();
    test_complex();
    test_format();
    test_datetime();
    test_epoch();
    test_expression_operator_normalization();
    test_session_history_limit();
    test_session_deserialize_without_history();
    test_function_usage_tooltip();
    test_grouped_numeric_literal_display_format();
    test_non_informative_numeric_simplified_row_suppression();

    test_angle_mode(settings);

    if (!eval_failed_tests)
        return 0;

    cout << eval_total_tests  << " total, "
         << eval_failed_tests << " failed, "
         << eval_new_failed_tests << " new" << endl;
    return eval_new_failed_tests;
}
