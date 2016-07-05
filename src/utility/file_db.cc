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

// Standard:
#include <cstddef>

// Qt:
#include <QDateTime>
#include <QTime>

// Local:
#include "file_db.h"


FileDB::FileDB (QDir location):
	_location (location)
{
	_location.mkpath (".");
}


std::shared_ptr<QFile>
FileDB::get_file_for_timestamp (double unix_timestamp)
{
	std::shared_ptr<QFile>& output_log = _files[0];

	auto start_of_day = QDateTime::fromMSecsSinceEpoch (1000.0 * unix_timestamp);
	start_of_day.setTime (QTime());

	if (!output_log)
	{
		QString filestamp = start_of_day.toString (Qt::ISODate);
		output_log = std::make_shared<QFile> (_location.absolutePath() + "/samples." + filestamp + ".csv");

		output_log->open (QIODevice::Append);
		output_log->write ("#timestamp,#voltage,#voltmeter_temperature,#current,#ammeter_temperature,#power,"
						   "#energy,#voltage_corrected,#power_corrected,#energy_corrected,#voltage_corrected_filtered,"
						   "#current_filtered,#power_corrected_filtered,#energy_corrected_filtered\n");
		output_log->flush();
	}

	return output_log;
}

