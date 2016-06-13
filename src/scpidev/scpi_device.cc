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

// Local:
#include "scpi_device.h"


namespace scpidev {

SCPIDevice::SCPIDevice (QString const& name, QHostAddress const& ip_address, uint16_t tcp_port, QString const& log_filename):
	_name (name),
	_log_file (log_filename)
{
	_log_file.open (QIODevice::ReadWrite);
	_log_stream.setDevice (&_log_file);
	_log_stream << "Opening device '" << _name << "'" << "\n";

	_socket.connectToHost (ip_address, tcp_port, QIODevice::ReadWrite);
	_socket.waitForConnected();
	_socket.setSocketOption (QAbstractSocket::LowDelayOption, 1);

	_stream.setDevice (&_socket);
}


SCPIDevice::~SCPIDevice()
{
	// Force-push all pending data:
	_socket.flush();
}


void
SCPIDevice::send (QString const& command)
{
	_log_stream << _name << " << " << command << "\n";
	_stream << command << "\n";
	_stream.flush();
}


QString
SCPIDevice::ask()
{
	while (!_socket.canReadLine())
		_socket.waitForReadyRead();

	auto result = _stream.readLine().trimmed();
	_log_stream << _name << " >> " << result << "\n";
	_log_stream.flush();

	return result;
}


QString
SCPIDevice::ask (QString const& command)
{
	send (command);
	return ask();
}


void
SCPIDevice::flush()
{
	_socket.flush();
}

} // namespace scpidev

