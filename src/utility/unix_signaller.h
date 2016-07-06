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

#ifndef UTILITY__UNIX_SIGNALLER_H__INCLUDED
#define UTILITY__UNIX_SIGNALLER_H__INCLUDED

// Standard:
#include <cstddef>
#include <memory>

// Qt:
#include <QObject>
#include <QSocketNotifier>


/**
 * Attach to UNIX signal handlers and send related Qt signals.
 */
class UnixSignaller: public QObject
{
	Q_OBJECT

  public:
	// Ctor
	UnixSignaller (QObject* parent = nullptr);

	// Dtor
	~UnixSignaller();

	/**
	 * Execute this function from within UNIX signal handler.
	 * This function is asynchronous-safe and will cause a Qt signal
	 * to be emmited.
	 */
	void
	post (int signum);

  signals:
	/**
	 * Emmited when post (int) is called.
	 */
	void
	got_signal (int signum);

  private:
	// [0] is for reading, [1] is for writing:
	int					_pipes[2];
	QSocketNotifier*	_notifier;
};

#endif

