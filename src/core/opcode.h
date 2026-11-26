// SPDX-FileCopyrightText: 2015, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_OPCODE_H
#define CORE_OPCODE_H

#include<QString>


class Opcode
{
public:
    enum  Type { Nop, Load, Ref, Function, List, Add, Sub, Neg, BNot, Mul, Div, Pow,
           Fact, Modulo, IntDiv, LSh, RSh, BAnd, BOr, Conv, Percent, Phasor,
           UnitRef, Unit, DivUnit, Diff, Integrate, Limit };

    Type type;
    // Calculus opcodes use index to point to the closing Function opcode.
    // DivUnit uses index to record whether the divisor is negated.
    unsigned index;

    // Holds source text when an opcode needs it.
    QString text;

    Opcode() : type(Nop), index(0) {}
    Opcode(Type t) : type(t), index(0) {}
    Opcode(Type t, QString txt) : type(t), index(0), text(txt) {}
    Opcode(Type t, unsigned i): type(t), index(i) {}

    bool isCalculus() const { return type == Diff || type == Integrate || type == Limit; }

    int operandCount() const
    {
        switch (type) {
        case Nop: case Load: case Ref: case UnitRef: case Diff: case Integrate: case Limit:
            return 0;
        case Neg: case BNot: case Fact: case Percent:
            return 1;
        case DivUnit:
            return 3;
        case Function:
            return static_cast<int>(index) + 1;
        case List:
            return static_cast<int>(index);
        default:
            return 2;
        }
    }
};

#endif // CORE_OPCODE_H
