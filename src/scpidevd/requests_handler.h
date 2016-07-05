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

#ifndef SCPIDEVD__REQUESTS_HANDLER_H__INCLUDED
#define SCPIDEVD__REQUESTS_HANDLER_H__INCLUDED

// Standard:
#include <cstddef>


class RequestsHandler
{
  public:
	class Request
	{
	  public:
		double timestamp			= 0.0;
	};

	class Response
	{
	  public:
		double previous_sample_dt	= 0.0;
		double next_sample_dt		= 0.0;
		double sample_timestamp		= 0.0;
		double energy_J				= 0.0;
	};

  public:
	/**
	 * Add new connection.
	 * This object takes ownership of the socket argument.
	 */
	Response
	handle_request (Request const& request);
};

#endif

