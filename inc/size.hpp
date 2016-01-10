/****************************************************************************
**
** Copyright (C) 2016 Philip Seeger
** This file is part of CapacityTester.
**
** CapacityTester is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** CapacityTester is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with CapacityTester. If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#ifndef SIZE_HPP
#define SIZE_HPP

#include <QString>
#include <QPair>
#include <QStringList>

class Size
{
public:

    enum Format
    {
        Condensed       = 1 << 0,
        Standard        = 1 << 1,
        Full            = 1 << 2,
        Binary          = 1 << 3,
        Decimal         = 1 << 4,
        BinaryPrefix    = 1 << 5,
    };

    static inline QStringList
    unitNames()
    {
        return QStringList()
            << "Kilo"
            << "Mega"
            << "Giga"
            << "Tera"
            << "Peta"
            << "Exa"
            << "Zetta"
            << "Yotta";
    };

    static inline QList<uchar>
    unitSymbols()
    {
        QList<uchar> symbols;
        foreach (QString name, unitNames())
            symbols << name.at(0).toLatin1();
        return symbols;
    };

    Size(qint64 bytes = 0);

    qint64
    bytes() const;

    qlonglong
    toLongLong() const;

    operator qint64() const;

    QString
    formatted(int format = Standard | Binary) const;

private:

    QPair<qint64, uchar>
    valuePrefixPair(bool use_decimal_power = false) const;

    qint64
    _bytes;

};

#endif
