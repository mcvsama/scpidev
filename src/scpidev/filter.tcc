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

#ifndef SCPIDEV__FILTER_TCC__INCLUDED
#define SCPIDEV__FILTER_TCC__INCLUDED

// Standard:
#include <cstddef>
#include <array>
#include <cmath>


namespace scpidev {

template<std::size_t L>
	inline
	Filter<L>::Filter (double initial_value):
		_z (initial_value)
	{
		compute_window();
		_history.resize (kLength, initial_value);
	}


template<std::size_t L>
	inline double
	Filter<L>::process (double input)
	{
		if (!std::isfinite (input))
			return _z;

		_history.push_back (input);

		_z = 0.0;
		for (std::size_t i = 0; i < _history.size(); ++i)
			_z += _history[i] * _window[i];
		_z /= _history.size() - 1;
		// Correct by window energy:
		_z *= 2.0;

		return _z;
	}


template<std::size_t L>
	inline void
	Filter<L>::reset (double value)
	{
		_z = value;
		std::fill (_history.begin(), _history.end(), value);
	}


template<std::size_t L>
	inline void
	Filter<L>::compute_window()
	{
		// Hann window function:
		std::size_t N = _window.size();
		for (std::size_t n = 0; n < N; ++n)
			_window[n] = 0.5 * (1.0 - std::cos (2.0 * M_PI * n / (N - 1)));
	}

} // namespace scpidev

#include "filter.tcc"

#endif

