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
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

// Qt:
#include <QFile>
#include <QTextStream>

// Local:
#include "scpi_device.h"
#include "filter.h"
#include "utils.h"


using namespace scpidev;

constexpr char kVoltmeterIP[] = "11.0.0.100";
constexpr char kAmmeterIP[] = "11.0.0.101";

constexpr float kNPLC = 1;
constexpr double kAutoZeroPeriodSeconds = 10;
constexpr double kACFrequencyHz = 50.0;
constexpr double kTotalVoltmeterBurdenResitanceOhms = 0.025666;

std::atomic<bool> g_quit_signal { false };


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


int main()
{
	//TODO uncomment ammeter
	::signal (SIGINT, catch_sigint);

	if (setpriority(PRIO_PROCESS, 0, -20) == -1)
		std::cout << "Could not set 'nice' to -20." << std::endl;

	QFile output_log ("log");
	output_log.open (QIODevice::Append);
	output_log.write ("#timestamp,#voltage,#voltmeter_temperature,#current,#ammeter_temperature,#power,"
					  "#energy,#voltage_corrected,#power_corrected,#energy_corrected,#voltage_corrected_filtered,"
					  "#current_filtered,#power_corrected_filtered,#energy_corrected_filtered\n");
	output_log.flush();

	SCPIDevice voltmeter ("voltmeter", QHostAddress (kVoltmeterIP), 5025, "log.v");
	SCPIDevice ammeter ("ammeter", QHostAddress (kAmmeterIP), 5025, "log.a");

	std::cout << "Configuring for test..." << std::endl;
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

	while (g_quit_signal.load() == false)
	{
		auto voltage = voltmeter.ask ("FETCH?").toDouble();
		auto current = ammeter.ask ("FETCH?").toDouble();

		++samples_number;

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

		// Initiate single measurement:
		voltmeter.send ("INITIATE");
		ammeter.send ("INITIATE");

		// Timestamp @ INITIATE command:
		initiate_timestamp = now();
		dt = initiate_timestamp - prev_initiate_timestamp;
		max_dt = std::max (dt, max_dt);

		// Calculations:
		double power = voltage * current;
		energy += power * dt;
		// Corrections:
		double voltage_error = current * kTotalVoltmeterBurdenResitanceOhms;
		double voltage_corrected = voltage - voltage_error;
		double power_corrected = voltage_corrected * current;
		energy_corrected += power_corrected * dt;
		// Filtering:
		double voltage_corrected_filtered = voltage_corrected_filter.process (voltage_corrected);
		double current_filtered = current_filter.process (current);
		double power_corrected_filtered = voltage_corrected_filtered * current_filtered;
		energy_corrected_filtered += power_corrected_filtered * dt;

		QString out;
		out += "\x1B[H\x1B[2J";
		out += QString ("now = %1 s   elapsed = %2 s   since last autozero = %3 s\n").arg (bold ("%-.3f", initiate_timestamp))
			.arg (bold ("%+6.1f", initiate_timestamp - start_timestamp)).arg (bold ("%+6.1f", initiate_timestamp - auto_zero_timestamp));
		out += QString (" dt = %1 s            max dt = %2 s         timing errors = %3\n")
			.arg (bold ("%+.3f", dt)).arg (bold ("%+.3f", max_dt)).arg (timing_errors_s);
		out += "\n";
		out += QString ("    PLC/sample                        = %1\n").arg (kNPLC);
		out += QString ("    Voltmeter-motherboard resistance  = %1 Ω\n").arg (kTotalVoltmeterBurdenResitanceOhms);
		out += "\n";
		out += QString ("    Voltmeter temperature             = %1°C\n").arg (green ("%.3f", voltmeter_temperature));
		out += QString ("    Ammeter temperature               = %1°C\n").arg (green ("%.3f", ammeter_temperature));
		out += QString ("    Samples                           = %1\n").arg (samples_number);
		out += "\n";
		out += QString ("    Raw measurements:\n");
		out += QString ("        U           = %1 V\n").arg (hs (voltage));
		out += QString ("        I           = %1 A\n").arg (important (hs (current)));
		out += QString ("        P           = %1 W\n").arg (hs (power));
		out += QString ("       ∫P dt        = %1 Ws = %2 Wh\n").arg (hs (energy)).arg (hs (energy / 3600.0));
		out += "\n";
		out += QString ("    Corrected measurements:\n");
		out += QString ("        U           = %1 V (error = %2 V)\n").arg (important (hs (voltage_corrected))).arg (hs (voltage_error));
		out += QString ("        P           = %1 W (error = %2 W)\n").arg (important (hs (power_corrected))).arg (hs (power - power_corrected));
		out += QString ("       ∫P dt        = %1 Ws = %2 Wh\n").arg (hs (energy_corrected)).arg (important (hs (energy_corrected / 3600.0)));
		out += "\n";
		out += QString ("    Filtered measurements (%1 taps, Hann):\n").arg (filter_taps);
		out += QString ("        U           = %1 V\n").arg (important (ls (voltage_corrected_filtered)));
		out += QString ("        I           = %1 A\n").arg (important (ls (current_filtered)));
		out += QString ("        P           = %1 W\n").arg (important (ls (power_corrected_filtered)));

		std::cout << out.toStdString() << std::flush;

		output_log.write (QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14\n")
						  .arg (initiate_timestamp, 0, 'f', 6)
						  .arg (voltage, 0, 'f', 9)
						  .arg (voltmeter_temperature, 0, 'f', 3)
						  .arg (current, 0, 'f', 9)
						  .arg (ammeter_temperature, 0, 'f', 3)
						  .arg (power, 0, 'f', 18)
						  .arg (energy, 0, 'f', 18)
						  .arg (voltage_corrected, 0, 'f', 18)
						  .arg (power_corrected, 0, 'f', 18)
						  .arg (energy_corrected, 0, 'f', 18)
						  .arg (voltage_corrected_filtered, 0, 'f', 9)
						  .arg (current_filtered, 0, 'f', 6)
						  .arg (power_corrected_filtered, 0, 'f', 18)
						  .arg (energy_corrected_filtered, 0, 'f', 18)
						  .toUtf8());

		prev_initiate_timestamp = initiate_timestamp;
	}

	std::cout << "\nQuitting.\n";

	voltmeter.send ("DISPLAY:TEXT:CLEAR");
	ammeter.send ("DISPLAY:TEXT:CLEAR");

	return EXIT_SUCCESS;
}

