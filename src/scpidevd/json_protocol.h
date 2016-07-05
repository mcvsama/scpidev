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

#ifndef SCPIDEVD__CONNECTIONS_HANDLER_H__INCLUDED
#define SCPIDEVD__CONNECTIONS_HANDLER_H__INCLUDED

// Standard:
#include <cstddef>

// Local:
#include "requests_handler.h"


class JSONProtocol
{
  public:
	class UnexpectedEnd: public std::runtime_error
	{
	  public:
		// Ctor:
		UnexpectedEnd():
			std::runtime_error ("unexpected end of encoded string")
		{ }
	};

	class InvalidHexCode: public std::runtime_error
	{
	  public:
		// Ctor:
		InvalidHexCode (QString hex_code):
			std::runtime_error ("invalid hex code " + hex_code.toStdString())
		{ }
	};

  private:
	class Buffers
	{
	  public:
		QString input;
		QString output;
	};

  public:
	// Ctor:
	JSONProtocol (RequestsHandler&);

	// Dtor:
	~JSONProtocol();

	/**
	 * Add new connection.
	 * This object takes ownership of the socket argument.
	 */
	void
	new_connection (QTcpSocket* socket);

	/**
	 * Percent-decode string inline.
	 */
	static void
	percent_decode (QString& string);

  private:
	void
	handle_request (QTcpSocket&, Buffers&);

	/**
	 * Write output buffers to the socket.
	 * Called-back when socket is ready for writing.
	 */
	void
	write_output (QTcpSocket&, Buffers&, int64_t bytes_written = 0);

	void
	delete_connection (QTcpSocket&);

  private:
	std::map<QTcpSocket*, Buffers>	_buffers;
	RequestsHandler&				_requests_handler;
};

#endif

