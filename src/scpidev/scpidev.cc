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
#include <atomic>
#include <queue>
#include <mutex>
#include <thread>

// Linux:
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

// Qt:
#include <QFile>
#include <QTextStream>
#include <QSemaphore>
#include <QDir>

// Boost:
#include <boost/optional.hpp>

// SCPIDev:
#include <scpidev/filter.h>
#include <scpidev/scpi_device.h>
#include <scpidev/utils.h>
#include <utility/file_db.h>


using namespace scpidev;

constexpr char kVoltmeterIP[] = "11.0.0.100";
constexpr char kAmmeterIP[] = "11.0.0.101";
constexpr char kOutputDir[] = "scpidev.log";

constexpr float kNPLC = 1;
constexpr double kAutoZeroPeriodSeconds = 10;
constexpr double kACFrequencyHz = 50.0;
constexpr double kTotalVoltmeterBurdenResitanceOhms = 0.025666;

std::atomic<bool> g_quit_signal { false };


/**
 * Single sample from all DMMs.
 */
class Sample
{
  public:
	uint64_t	number						= 0;
	uint64_t	timing_errors				= 0;
	QString		timing_errors_s;
	double		start_timestamp				= 0.0;
	double		initiate_timestamp			= 0.0;
	double		auto_zero_timestamp			= 0.0;
	double		dt							= 0.0;
	double		max_dt						= 0.0;
	std::size_t	filter_taps					= 0;

	// Measurements:
	double		power						= 0.0;
	double		energy						= 0.0;
	double		voltage_error				= 0.0;
	double		voltage_corrected			= 0.0;
	double		power_corrected				= 0.0;
	double		energy_corrected			= 0.0;
	double		voltage_corrected_filtered	= 0.0;
	double		current_filtered			= 0.0;
	double		power_corrected_filtered	= 0.0;
	double		energy_corrected_filtered	= 0.0;

	// Voltmeter:
	double		voltage					 	= 0.0;
	double		voltmeter_temperature		= 0.0;

	// Ammeter:
	double		current						= 0.0;
	double		ammeter_temperature			= 0.0;
};


void
configure_voltmeter (SCPIDevice& voltmeter)
{
	constexpr const char kVoltmeterSystemIdentifier[] = "AT34461A";
	constexpr const char kVoltmeterHostname[] = "A-34461A-09358";
	constexpr const char kVoltmeterLabel[] = "Voltage";
	constexpr const char kVoltmeterRange[] = "100";

	voltmeter.send ("ABORT");
	voltmeter.ask ("*IDN?");

	verify (voltmeter, "SYSTEM:IDENTIFY?", kVoltmeterSystemIdentifier);
	verify (voltmeter, "UNIT:TEMP?", "C");
	verify (voltmeter, "SYSTEM:COMMUNICATE:LAN:HOSTNAME?", "\"" + QString (kVoltmeterHostname) + "\"");

	voltmeter.ask ("CALIBRATION:DATE?; TIME?");
	voltmeter.ask ("CALIBRATION:TEMPERATURE?");

	voltmeter.send ("SYSTEM:LABEL \"" + QString (kVoltmeterLabel) + "\"");
	// Fixed range:
	voltmeter.send ("CONFIGURE:VOLTAGE:DC " + QString (kVoltmeterRange));
	// Zeroing will be done manually every couple of samples by the script.
	voltmeter.send ("SENSE:VOLTAGE:DC:ZERO:AUTO OFF");
	// Aperture:
	voltmeter.send ("SENSE:VOLTAGE:DC:NPLC " + QString::number (kNPLC));
	// Impedance: 10 MΩ
	voltmeter.send ("SENSE:VOLTAGE:DC:IMPEDANCE:AUTO OFF");
	// Trigger: 1, auto-delay, internal trigger
	voltmeter.send ("TRIGGER:COUNT 1");
	voltmeter.send ("TRIGGER:DELAY:AUTO ON");
	voltmeter.send ("TRIGGER:SOURCE IMMEDIATE");
	// Samples at a time:
	voltmeter.send ("SAMPLE:COUNT 1");

	voltmeter.flush();
}


void
configure_ammeter (SCPIDevice& ammeter)
{
	constexpr const char kAmmeterSystemIdentifier[] = "AT34461A";
	constexpr const char kAmmeterHostname[] = "K-34461A-18230";
	constexpr const char kAmmeterLabel[] = "Current";
	constexpr const char kAmmeterRange[] = "10";

	ammeter.send ("ABORT");
	ammeter.ask ("*IDN?");

	verify (ammeter, "SYSTEM:IDENTIFY?", kAmmeterSystemIdentifier);
	verify (ammeter, "UNIT:TEMP?", "C");
	verify (ammeter, "SYSTEM:COMMUNICATE:LAN:HOSTNAME?", "\"" + QString (kAmmeterHostname) + "\"");

	ammeter.ask ("CALIBRATION:DATE?; TIME?");
	ammeter.ask ("CALIBRATION:TEMPERATURE?");

	ammeter.send ("SYSTEM:LABEL \"" + QString (kAmmeterLabel) + "\"");

	// Fixed range:
	ammeter.send ("CONFIGURE:CURRENT:DC " + QString (kAmmeterRange));
	// Zeroing will be done manually every couple of samples by the script.
	ammeter.send ("SENSE:CURRENT:DC:ZERO:AUTO OFF");
	// Aperture:
	ammeter.send ("SENSE:CURRENT:DC:NPLC " + QString::number (kNPLC));
	// Trigger: 1, auto-delay, internal trigger
	ammeter.send ("TRIGGER:COUNT 1");
	ammeter.send ("TRIGGER:DELAY:AUTO ON");
	ammeter.send ("TRIGGER:SOURCE IMMEDIATE");
	// Samples at a time:
	ammeter.send ("SAMPLE:COUNT 1");

	ammeter.flush();
}


void
catch_sigint (int)
{
	g_quit_signal.store (true);
}


/**
 * Thread for communication with DMMs.
 */
void
measure_function (SCPIDevice& voltmeter, SCPIDevice& ammeter, std::queue<Sample>& samples_todo, std::mutex& samples_mutex, QSemaphore& samples_semaphore)
{
	if (setpriority(PRIO_PROCESS, 0, -20) == -1)
		std::cout << "Could not set 'nice' to -20." << std::endl;

	voltmeter.send ("DISPLAY:TEXT \"Configuring for test...\"");
	voltmeter.flush();
	ammeter.send ("DISPLAY:TEXT \"Configuring for test...\"");
	ammeter.flush();

	configure_voltmeter (voltmeter);
	configure_ammeter (ammeter);

	std::cout << "TCP warmup..." << std::endl;
	auto kTCPWarmupMessageCommand = "DISPLAY:TEXT \"     TCP warmup...     \"";
	voltmeter.send (kTCPWarmupMessageCommand);
	voltmeter.flush();
	ammeter.send (kTCPWarmupMessageCommand);
	ammeter.flush();

	double initial_voltage = 0.0;
	double initial_current = 0.0;

	// Initial burst of packets for TCP to adapt:
	for (int i = 0; i < kACFrequencyHz / kNPLC; ++i)
	{
		voltmeter.send ("INITIATE");
		ammeter.send ("INITIATE");

		initial_voltage = voltmeter.ask ("FETCH?").toDouble();
		initial_current = ammeter.ask ("FETCH?").toDouble();
	}

	auto kTestMessageCommand = "DISPLAY:TEXT \"Test in progress (voltage)...\"";
	voltmeter.send (kTestMessageCommand);
	ammeter.send (kTestMessageCommand);

	// Reset:
	voltmeter.send ("ABORT");
	ammeter.send ("ABORT");
	// Start measuring:
	voltmeter.send ("INITIATE");
	ammeter.send ("INITIATE");

	double start_timestamp = now();
	double auto_zero_timestamp = start_timestamp - kAutoZeroPeriodSeconds - 1.0;

	double voltmeter_temperature = 0.0;
	double ammeter_temperature = 0.0;

	double energy = 0.0;
	double energy_corrected = 0.0;
	double energy_corrected_filtered = 0.0;

	double prev_initiate_timestamp = start_timestamp;
	int timing_errors = 0;
	QString timing_errors_s = "0";
	double max_dt = 0.0;
	uint64_t samples_number = 0;

	double initiate_timestamp;
	double dt;

	constexpr std::size_t filter_taps = 25 / kNPLC;
	Filter<filter_taps> voltage_corrected_filter (initial_voltage);
	Filter<filter_taps> current_filter (initial_current);

	while (!g_quit_signal.load())
	{
		Sample sample;
		sample.number = ++samples_number;
		sample.voltage = voltmeter.ask ("FETCH?").toDouble();
		sample.current = ammeter.ask ("FETCH?").toDouble();

		// Auto-zero and temperature read:
		if (initiate_timestamp - auto_zero_timestamp >= kAutoZeroPeriodSeconds)
		{
			voltmeter.send ("SENSE:VOLTAGE:DC:ZERO:AUTO ONCE");
			ammeter.send ("SENSE:CURRENT:DC:ZERO:AUTO ONCE");

			voltmeter_temperature = voltmeter.ask ("SYSTEM:TEMPERATURE?").toDouble();
			ammeter_temperature = ammeter.ask ("SYSTEM:TEMPERATURE?").toDouble();

			prev_initiate_timestamp = now();
			auto_zero_timestamp = prev_initiate_timestamp;
		}
		else if (dt > 2.0 * kNPLC / kACFrequencyHz)
		{
			timing_errors += 1;
			timing_errors_s = erroneous (QString::number (timing_errors));
		}

		sample.voltmeter_temperature = voltmeter_temperature;
		sample.ammeter_temperature = ammeter_temperature;
		sample.timing_errors = timing_errors;
		sample.timing_errors_s = timing_errors_s;

		// Initiate single measurement:
		voltmeter.send ("INITIATE");
		voltmeter.flush();
		ammeter.send ("INITIATE");
		ammeter.flush();

		// Timestamp @ INITIATE command:
		initiate_timestamp = now();
		dt = initiate_timestamp - prev_initiate_timestamp;
		max_dt = std::max (dt, max_dt);

		// Calculations:
		sample.power = sample.voltage * sample.current;
		energy += sample.power * dt;
		sample.energy = energy;
		// Corrections:
		sample.voltage_error = sample.current * kTotalVoltmeterBurdenResitanceOhms;
		sample.voltage_corrected = sample.voltage - sample.voltage_error;
		sample.power_corrected = sample.voltage_corrected * sample.current;
		energy_corrected += sample.power_corrected * dt;
		sample.energy_corrected = energy_corrected;
		// Filtering:
		sample.voltage_corrected_filtered = voltage_corrected_filter.process (sample.voltage_corrected);
		sample.current_filtered = current_filter.process (sample.current);
		sample.power_corrected_filtered = sample.voltage_corrected_filtered * sample.current_filtered;
		energy_corrected_filtered += sample.power_corrected_filtered * dt;
		sample.energy_corrected_filtered = energy_corrected_filtered;

		sample.dt = dt;
		sample.max_dt = max_dt;
		sample.start_timestamp = start_timestamp;
		sample.initiate_timestamp = initiate_timestamp;
		sample.auto_zero_timestamp = auto_zero_timestamp;
		sample.filter_taps = filter_taps;

		{
			std::lock_guard<std::mutex> lock (samples_mutex);
			samples_todo.push (sample);
		}
		samples_semaphore.release (1);

		prev_initiate_timestamp = initiate_timestamp;
	}
}


/**
 * Log single sample to an output file.
 */
void
log_sample (Sample const& sample, FileDB& file_db)
{
	auto output_log = file_db.get_file_for_timestamp (sample.initiate_timestamp);

	output_log->write (QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14\n")
					   .arg (sample.initiate_timestamp, 0, 'f', 6)
					   .arg (sample.voltage, 0, 'f', 9)
					   .arg (sample.voltmeter_temperature, 0, 'f', 3)
					   .arg (sample.current, 0, 'f', 9)
					   .arg (sample.ammeter_temperature, 0, 'f', 3)
					   .arg (sample.power, 0, 'f', 18)
					   .arg (sample.energy, 0, 'f', 18)
					   .arg (sample.voltage_corrected, 0, 'f', 18)
					   .arg (sample.power_corrected, 0, 'f', 18)
					   .arg (sample.energy_corrected, 0, 'f', 18)
					   .arg (sample.voltage_corrected_filtered, 0, 'f', 9)
					   .arg (sample.current_filtered, 0, 'f', 6)
					   .arg (sample.power_corrected_filtered, 0, 'f', 18)
					   .arg (sample.energy_corrected_filtered, 0, 'f', 18)
					   .toUtf8());
}


/**
 * Thread for writing log file and updating screen (on stdout).
 */
void
log_function (std::queue<Sample>& samples_todo, std::mutex& samples_mutex, QSemaphore& samples_semaphore)
{
	FileDB file_db { QDir (kOutputDir) };

	do {
		samples_semaphore.acquire (1);
		std::vector<Sample> samples;

		{
			std::lock_guard<std::mutex> lock (samples_mutex);
			while (!samples_todo.empty())
			{
				samples.push_back (samples_todo.front());
				samples_todo.pop();
			}
		}

		for (auto& sample: samples)
			log_sample (sample, file_db);

		if (!samples.empty())
		{
			// Only display the latest sample:
			auto& sample = samples.back();

			QString out;
			out += "\x1B[H\x1B[2J";
			out += QString ("now = %1 s   elapsed = %2 s   since last autozero = %3 s\n").arg (bold ("%-.3f", sample.initiate_timestamp))
				.arg (bold ("%+6.1f", sample.initiate_timestamp - sample.start_timestamp)).arg (bold ("%+6.1f", sample.initiate_timestamp - sample.auto_zero_timestamp));
			out += QString (" dt = %1 s            max dt = %2 s         timing errors = %3\n")
				.arg (bold ("%+.3f", sample.dt)).arg (bold ("%+.3f", sample.max_dt)).arg (sample.timing_errors_s);
			out += QString (" queue = %1\n").arg (samples_semaphore.available());
			out += "\n";
			out += QString ("    PLC/sample                        = %1\n").arg (kNPLC);
			out += QString ("    Voltmeter-motherboard resistance  = %1 Ω\n").arg (kTotalVoltmeterBurdenResitanceOhms);
			out += "\n";
			out += QString ("    Voltmeter temperature             = %1°C\n").arg (green ("%.3f", sample.voltmeter_temperature));
			out += QString ("    Ammeter temperature               = %1°C\n").arg (green ("%.3f", sample.ammeter_temperature));
			out += QString ("    Samples                           = %1\n").arg (sample.number);
			out += "\n";
			out += QString ("    Raw measurements:\n");
			out += QString ("        U           = %1 V\n").arg (hs (sample.voltage));
			out += QString ("        I           = %1 A\n").arg (important (hs (sample.current)));
			out += QString ("        P           = %1 W\n").arg (hs (sample.power));
			out += QString ("       ∫P dt        = %1 Ws = %2 Wh\n").arg (hs (sample.energy)).arg (hs (sample.energy / 3600.0));
			out += "\n";
			out += QString ("    Corrected measurements:\n");
			out += QString ("        U           = %1 V (error = %2 V)\n").arg (important (hs (sample.voltage_corrected))).arg (hs (sample.voltage_error));
			out += QString ("        P           = %1 W (error = %2 W)\n").arg (important (hs (sample.power_corrected))).arg (hs (sample.power - sample.power_corrected));
			out += QString ("       ∫P dt        = %1 Ws = %2 Wh\n").arg (hs (sample.energy_corrected)).arg (important (hs (sample.energy_corrected / 3600.0)));
			out += "\n";
			out += QString ("    Filtered measurements (%1 taps, Hann):\n").arg (sample.filter_taps);
			out += QString ("        U           = %1 V\n").arg (important (ls (sample.voltage_corrected_filtered)));
			out += QString ("        I           = %1 A\n").arg (important (ls (sample.current_filtered)));
			out += QString ("        P           = %1 W\n").arg (important (ls (sample.power_corrected_filtered)));

			std::cout << out.toStdString() << std::flush;
		}
	} while (!g_quit_signal.load());
}


int main()
{
	if (!g_quit_signal.is_lock_free())
		throw std::runtime_error ("this platform or binary doesn't have lock-free atomics");

	std::cout << "Connecting..." << std::endl;
	SCPIDevice voltmeter ("voltmeter", QHostAddress (kVoltmeterIP), 5025, "log.v");
	SCPIDevice ammeter ("ammeter", QHostAddress (kAmmeterIP), 5025, "log.a");

	::signal (SIGINT, catch_sigint);

	std::cout << "Press C-c to stop.\n" << std::endl;
	std::cout << "Configuring for test..." << std::endl;
	QSemaphore samples_semaphore;
	std::queue<Sample> samples_queue;
	std::mutex samples_mutex;

	std::thread measure_thread (measure_function,
								std::ref (voltmeter), std::ref (ammeter),
								std::ref (samples_queue), std::ref (samples_mutex),
								std::ref (samples_semaphore));

	std::thread log_thread (log_function,
							std::ref (samples_queue), std::ref (samples_mutex), std::ref (samples_semaphore));

	measure_thread.join();
	log_thread.join();

	std::cout << "\nQuitting.\n";

	voltmeter.send ("DISPLAY:TEXT:CLEAR");
	ammeter.send ("DISPLAY:TEXT:CLEAR");

	return EXIT_SUCCESS;
}

