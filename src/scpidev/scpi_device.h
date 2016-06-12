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

#ifndef SCPIDEV__SCPI_DEVICE_H__INCLUDED
#define SCPIDEV__SCPI_DEVICE_H__INCLUDED

// Standard:
#include <cstddef>

// Qt:
#include <QHostAddress>
#include <QFile>
#include <QTcpSocket>
#include <QTextStream>


namespace scpidev {

class SCPIDevice
{
  public:
	/**
	 * \param	name
	 *			Device identifier to use in logs.
	 * \param	log
	 *			Log stream to use.
	 */
	SCPIDevice (QString const& name, QHostAddress const& ip_address, uint16_t tcp_port, QString const& log_filename);

	// Dtor
	~SCPIDevice();

	/**
	 * Send command or semicolon-separated SCPI commands.
	 * Automatically appends newline at the end.
	 */
	void
	send (QString const& command);

	/**
	 * Return single line result from the SCPI device.
	 */
	QString
	ask();

	/**
	 * Equivalent to send() and then ask().
	 */
	QString
	ask (QString const& command);

	/**
	 * Force-flush output TCP buffer.
	 */
	void
	flush();

  private:
	QString			_name;
	QTcpSocket		_socket;
	// Depends on _socket:
	QTextStream		_stream;
	QFile			_log_file;
	// Depends on _log_file:
	QTextStream		_log_stream;
};

} // namespace scpidev

#endif

