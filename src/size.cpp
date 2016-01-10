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

#include "size.hpp"

Size::Size(qint64 bytes)
    : _bytes(bytes)
{
}

qint64
Size::bytes()
const
{
    return _bytes;
}

qlonglong
Size::toLongLong()
const
{
    return bytes();
}

Size::operator qint64()
const
{
    return bytes();
}

QString
Size::formatted(int format)
const
{
    //https://xkcd.com/394/

    //Get size and prefix separately
    QPair<qint64, uchar> pair =
        valuePrefixPair(format & Decimal ? true : false);
    qint64 number = pair.first;
    uchar symbol = pair.second;

    //Numeric value
    QString formatted = QString::number(number);

    //Condensed: 123B or 123M (no whitespace)
    //Standard: 123 B or 123 MB (whitespace)
    //Standard + BinaryPrefix: 123 MiB
    //Full: 123 byte or 123 Megabyte
    //Full + BinaryPrefix: 123 Mebibyte (lol)
    if (format & Condensed)
    {
        //Short form (one letter only)
        if (!symbol)
        {
            //B
            formatted += "B";
        }
        else
        {
            //X (no B)
            formatted += symbol;
        }
    }
    else if (format & Standard)
    {
        //Standard form (123 MB)
        if (!symbol)
        {
            //B
            formatted += " B";
        }
        else
        {
            //XB
            formatted += " ";
            formatted += symbol;
            if (format & BinaryPrefix) formatted += "i";
            formatted += "B";
        }
    }
    else if (format & Full)
    {
        //Full form
        if (!symbol)
        {
            formatted += " Byte";
        }
        else
        {
            //M -> Mega
            int power_index = unitSymbols().indexOf(symbol);
            QString name = unitNames().at(power_index);

            //Mega -> Mebi (lol)
            if (format & BinaryPrefix)
            {
                int replace_count = name.length() > 3 ? 2 : 1; //exa -> exbi
                name.chop(replace_count);
                name += "bi";
            }

            //123 Megabyte
            formatted += " ";
            formatted += name + "byte";
        }
    }

    return formatted;
}

QPair<qint64, uchar>
Size::valuePrefixPair(bool use_decimal_power)
const
{
    QPair<qint64, uchar> pair;

    int multiplier = 1024; //nobody uses 1000
    if (use_decimal_power)
        multiplier = 1000;

    qint64 bytes = _bytes;
    qint64 value = bytes;
    uchar unit_prefix = 0; //empty for (< 1024) bytes
    foreach (uchar current_prefix, unitSymbols())
    {
        if (value < multiplier)
            break;

        value /= multiplier;
        unit_prefix = current_prefix;
    }

    pair.first = value;
    pair.second = unit_prefix;

    return pair;
}

