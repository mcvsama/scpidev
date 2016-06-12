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

#ifndef SCPIDEV__FILTER_H__INCLUDED
#define SCPIDEV__FILTER_H__INCLUDED

// Standard:
#include <cstddef>
#include <array>

// Boost:
#include <boost/circular_buffer.hpp>


namespace scpidev {

/**
 * Implements moving-average with Hann window.
 *
 * \param	pLength
 *			Number of taps in the filter.
 */
template<std::size_t pLength>
	class Filter
	{
	  public:
		static constexpr std::size_t kLength = pLength;

	  public:
		/**
		 * \param	initial_value
		 *			Initial output value.
		 */
		Filter (double initial_value = 0.0);

		/**
		 * Process single sample and return smoothed value.
		 */
		double
		process (double input);

		/**
		 * Reset filter to given value.
		 */
		void
		reset (double value);

	  private:
		void
		compute_window();

	  private:
		std::array<double, kLength>		_window;
		// Previous result:
		double							_z;
		// Previous input samples:
		boost::circular_buffer<double>	_history;
	};

} // namespace scpidev

#endif

#include "filter.tcc"

