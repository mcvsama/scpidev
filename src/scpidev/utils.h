/* vim:ts=4
 *
 * Copyleft 2012…2016  Michał Gawron
 * Marduk Unix Labs, http://mulabs.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Visit http://www.gnu.org/licenses/gpl-3.0.html for more information on licensing.
 */

#ifndef SCPIDEV__UTILS_H__INCLUDED
#define SCPIDEV__UTILS_H__INCLUDED

// Standard:
#include <cstddef>
#include <iostream>

// Boost:
#include <boost/format.hpp>

// Qt:
#include <QDateTime>


namespace scpidev {

inline void
verify (SCPIDevice& scpi_device, QString const& command, QString const& expected_result)
{
	auto actual_result = scpi_device.ask (command);
	if (actual_result != expected_result)
		std::cerr << "Failed to verify " << command.toStdString() << " is " << expected_result.toStdString() << ", result is " << actual_result.toStdString() << std::endl;
}


inline QString
important (QString const& str)
{
	return "\x1B[1;39;44m" + str + "\x1B[0m";
}


inline QString
erroneous (QString const& str)
{
	return "\x1B[1;39;41m" + str + "\x1B[0m";
}


template<class Value>
	inline QString
	green (QString const& format, Value&& value)
	{
		return QString::fromStdString ("\x1B[0;32;49m" + (boost::format (format.toStdString()) % value).str() + "\x1B[0m");
	}


template<class Value>
	inline QString
	bold (QString const& format, Value&& value)
	{
		return QString::fromStdString ("\x1B[1;39;49m" + (boost::format (format.toStdString()) % value).str() + "\x1B[0m");
	}


double
now()
{
	return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}


QString
hs (double value)
{
	return QString::fromStdString ((boost::format ("%+11.6f") % value).str());
}


QString
ls (double value)
{
	return QString::fromStdString ((boost::format ("%+8.3f") % value).str());
}

} // namespace scpidev

#endif

