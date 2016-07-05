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

#ifndef UTILITY__FILE_DB_H__INCLUDED
#define UTILITY__FILE_DB_H__INCLUDED

// Standard:
#include <cstddef>
#include <memory>

// Qt:
#include <QDir>
#include <QFile>


class FileDB
{
  public:
	/**
	 * Ctor
	 *
	 * \param	location
	 * 			Location of CSV files.
	 */
	explicit FileDB (QDir location);

	/**
	 * Return QFile to use for given timestamp.
	 */
	std::shared_ptr<QFile>
	get_file_for_timestamp (double unix_timestamp);

  private:
	QDir	_location;
	// Key is the beginning of the day UNIX timestamp:
	std::map<uint64_t, std::shared_ptr<QFile>>
			_files;
};

#endif

