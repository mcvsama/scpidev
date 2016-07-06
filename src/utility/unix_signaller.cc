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
#include <stdexcept>
#include <unistd.h>
#include <errno.h>

// Qt:
#include <QObject>

// Local:
#include "unix_signaller.h"


UnixSignaller::UnixSignaller (QObject* parent):
	QObject (parent)
{
	if (::pipe (_pipes) != 0)
		throw std::runtime_error (std::string ("couldn't allocate pipe for UnixSignaller: ") + ::strerror (errno));

	_notifier = new QSocketNotifier (_pipes[0], QSocketNotifier::Read, this);

	QObject::connect (_notifier, &QSocketNotifier::activated, [&](int) {
		uint8_t signum;
		::read (_pipes[0], &signum, sizeof (signum));
		emit got_signal (signum);
	});
}


UnixSignaller::~UnixSignaller()
{
	::close (_pipes[0]);
	::close (_pipes[1]);
}


void
UnixSignaller::post (int signum)
{
	uint8_t signum_b = signum;
	::write (_pipes[1], &signum_b, sizeof (signum_b));
	::fdatasync (_pipes[1]);
}

