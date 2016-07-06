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
#include <iostream>
#include <memory>
#include <cstdint>
#include <atomic>
#include <cctype>

// Linux:
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

// Boost:
#include <boost/lexical_cast.hpp>

// Qt:
#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>

// SCPIDevD:
#include <scpidevd/json_protocol.h>
#include <scpidevd/requests_handler.h>
#include <utility/unix_signaller.h>


constexpr uint16_t kTcpListenPort = 5026;

std::unique_ptr<UnixSignaller> g_unix_signaller;


void
catch_sigint (int signum)
{
	g_unix_signaller->post (signum);
	// Restore original handler, so if user sends SIGINT twice, the second one will forcefully stop
	// the process:
	::signal (SIGINT, SIG_DFL);
}


int main (int argc, char** argv)
{
	try {
		auto event_loop = std::make_unique<QCoreApplication> (argc, argv);
		RequestsHandler requests_handler;
		JSONProtocol json_protocol (requests_handler);
		auto server = std::make_unique<QTcpServer>();

		if (!server->listen (QHostAddress::Any, kTcpListenPort))
			throw std::runtime_error ("could not bind to port " + boost::lexical_cast<std::string> (kTcpListenPort) +
									  "; reason: " + server->errorString().toStdString());

		std::cout << "Listening on port " << server->serverAddress().toString().toStdString() << ":" << server->serverPort()
				  << ", max " << server->maxPendingConnections() << " connections." << std::endl;

		QObject::connect (server.get(), &QTcpServer::newConnection, [&] {
			auto socket = server->nextPendingConnection();
			std::cout << "Connection from " << socket->peerAddress().toString().toStdString() << ":" << socket->peerPort() << "." << std::endl;
			json_protocol.new_connection (socket);
		});

		QObject::connect (server.get(), &QTcpServer::acceptError, [&](QAbstractSocket::SocketError error) {
			std::cout << "Failed to accept connection (error code: " << error << ")." << std::endl;
		});

		g_unix_signaller = std::make_unique<UnixSignaller>();
		::signal (SIGINT, catch_sigint);

		QObject::connect (g_unix_signaller.get(), &UnixSignaller::got_signal, [&](int signum) {
			if (signum == SIGINT)
				event_loop->quit();
		});

		event_loop->exec();

		std::cout << "\nQuitting.\n";
	}
	catch (std::exception& e)
	{
		std::cout << "Fatal error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

