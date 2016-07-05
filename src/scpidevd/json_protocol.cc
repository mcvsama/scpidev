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
#include <iostream> // XXX
#include <functional>

// Qt:
#include <QTcpSocket>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>

// Local:
#include "json_protocol.h"


JSONProtocol::JSONProtocol (RequestsHandler& requests_handler):
	_requests_handler (requests_handler)
{
}


JSONProtocol::~JSONProtocol()
{
	// Delete all sockets to disconnect signals:
	for (auto& pair: _buffers)
		delete pair.first;
}


void
JSONProtocol::new_connection (QTcpSocket* socket)
{
	auto& buffers = _buffers[socket];
	buffers.input.reserve (100);
	buffers.output.reserve (300);

	QObject::connect (socket, &QTcpSocket::readyRead, std::bind (&JSONProtocol::handle_request, this, std::ref (*socket), std::ref (buffers)));
	QObject::connect (socket, &QTcpSocket::bytesWritten, std::bind (&JSONProtocol::write_output, this, std::ref (*socket), std::ref (buffers), std::placeholders::_1));
	QObject::connect (socket, &QTcpSocket::aboutToClose, std::bind (&JSONProtocol::delete_connection, this, std::ref (*socket)));
}


void
JSONProtocol::handle_request (QTcpSocket& socket, Buffers& buffers)
{
	// All newline and percent characters are %-encoded (newline is %0a, '%' is "%25").
	// All other characters may be %-encoded.
	// Newline literal (0xa, \n) separates requests.
	// Same rules apply for responses from server.

	buffers.input += socket.readAll();

	int p = 0;
	int n = 0;

	for (; (n = buffers.input.indexOf ('\n', p)) != -1; p = n + 1)
	{
		QString request_str = buffers.input.mid (p, n - p - 1);
		percent_decode (request_str);

		QString error_message;

		try {
			QJsonParseError error;
			// Try to read a valid JSON:
			QJsonDocument json_doc = QJsonDocument::fromJson (request_str.toUtf8(), &error);

			if (error.error != QJsonParseError::NoError)
				throw "parse error: " + error.errorString();

			RequestsHandler::Request request;

			// Process JSON:
			// Format: { get: { timestamp: xxx } }
			if (!json_doc.isObject())
				throw QString ("expected top-level object");

			QJsonObject json_obj = json_doc.object();
			auto it_get = json_obj.find ("get");

			if (it_get == json_obj.end())
				throw QString ("invalid request (missing 'get')");

			if (!it_get.value().isObject())
				throw QString ("invalid request ('get' is not object)");

			QJsonObject inner_obj = it_get.value().toObject();
			auto it_timestamp = inner_obj.find ("timestamp");

			if (it_timestamp == inner_obj.end())
				throw QString ("invalid request (missing 'timestamp')");

			if (!it_timestamp.value().isDouble())
				throw QString ("invalid request ('timestamp' is not numeric)");

			request.timestamp = it_timestamp.value().toDouble();
			RequestsHandler::Response response = _requests_handler.handle_request (request);

			QJsonObject result_json_object {
				{ "result", QJsonObject {
					{ "previous-sample-dt", response.previous_sample_dt },
					{ "next-sample-dt", response.next_sample_dt },
					{ "interpolated-sample", QJsonObject {
						{ "timestamp", response.sample_timestamp },
						{ "energy.J", response.energy_J },
					} }
				} }
			};

			buffers.output += QJsonDocument (result_json_object).toJson (QJsonDocument::Compact);
		}
		catch (QString const& message)
		{
			error_message = message;
		}
		catch (std::exception const& e)
		{
			error_message = e.what();
		}
		catch (...)
		{
			error_message = "unknown exception occured";
		}

		if (!error_message.isEmpty())
		{
			// { error: { message: "" } }
			QJsonObject result_json_object {
				{ "error", QJsonObject {
					{ "message", error_message }
				} }
			};
			buffers.output += QJsonDocument (result_json_object).toJson (QJsonDocument::Compact);
		}

		// Initiate writing of output buffers:
		write_output (socket, buffers);
	}

	buffers.input = buffers.input.mid (p);
}


void
JSONProtocol::write_output (QTcpSocket& socket, Buffers& buffers, int64_t)
{
	if (!buffers.output.isEmpty())
	{
		auto n = socket.write (buffers.output.toUtf8());
		buffers.output = buffers.output.mid (n);
	}
}


void
JSONProtocol::delete_connection (QTcpSocket& socket)
{
	// TODO why not called?
	std::cout << "closed\n";
	_buffers.erase (&socket);
	delete &socket;
}


void
JSONProtocol::percent_decode (QString& string)
{
	int s = 0;
	int t = 0;
	int trim_to = string.size();

	while (s < string.size())
	{
		if (string[s] == '%')
		{
			if (s + 2 >= string.size())
				throw UnexpectedEnd();

			char c1 = string[s + 1].toLower().toLatin1();
			char c2 = string[s + 2].toLower().toLatin1();

			if (!std::isxdigit (c1) || !std::isxdigit (c2))
				throw InvalidHexCode (string.mid (s, 3));

			int i1 = std::isdigit (c1) ? (c1 - '0') : (c1 - 'a' + 10);
			int i2 = std::isdigit (c2) ? (c2 - '0') : (c2 - 'a' + 10);

			string[t] = 16 * i1 + i2;
			s += 2;
			trim_to -= 2;
		}
		else
			string[t] = string[s];

		s += 1;
		t += 1;
	}

	string.resize (trim_to);
}

