/****************************************************************************
 *
 *   Copyright (c) 2013-2016 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/*
 * @file FixedwingLandDetector.cpp
 *
 * @author Johan Jansen <jnsn.johan@gmail.com>
 * @author Lorenz Meier <lorenz@px4.io>
 * @author Julian Oes <julian@oes.ch>
 */

#include <px4_config.h>
#include <px4_defines.h>
#include <cmath>
#include <drivers/drv_hrt.h>

#include "FixedwingLandDetector.h"


namespace land_detector
{

FixedwingLandDetector::FixedwingLandDetector()
{
	_paramHandle.maxVelocity = param_find("LNDFW_VEL_XY_MAX");
	_paramHandle.maxClimbRate = param_find("LNDFW_VEL_Z_MAX");
	_paramHandle.maxAirSpeed = param_find("LNDFW_AIRSPD_MAX");
	_paramHandle.maxIntVelocity = param_find("LNDFW_VELI_MAX");
}

void FixedwingLandDetector::_initialize_topics()
{
	_armingSub = orb_subscribe(ORB_ID(actuator_armed));
	_airspeedSub = orb_subscribe(ORB_ID(airspeed));
	_local_pos_sub = orb_subscribe(ORB_ID(vehicle_local_position));
	_sensor_combined_sub = orb_subscribe(ORB_ID(sensor_combined));
}

void FixedwingLandDetector::_update_topics()
{
	_orb_update(ORB_ID(actuator_armed), _armingSub, &_arming);
	_orb_update(ORB_ID(airspeed), _airspeedSub, &_airspeed);
	_orb_update(ORB_ID(sensor_combined), _sensor_combined_sub, &_sensors);
	_orb_update(ORB_ID(vehicle_local_position), _local_pos_sub, &_local_pos);
}

void FixedwingLandDetector::_update_params()
{
	param_get(_paramHandle.maxVelocity, &_params.maxVelocity);
	param_get(_paramHandle.maxClimbRate, &_params.maxClimbRate);
	param_get(_paramHandle.maxAirSpeed, &_params.maxAirSpeed);
	param_get(_paramHandle.maxIntVelocity, &_params.maxIntVelocity);
}

float FixedwingLandDetector::_get_max_altitude()
{
	// TODO
	// This means no altitude limit as the limit
	// is always 100000 meters
	return 100000;
}

bool FixedwingLandDetector::_get_freefall_state()
{
	// TODO
	return false;
}
bool FixedwingLandDetector::_get_ground_contact_state()
{
	// TODO
	return false;
}

bool FixedwingLandDetector::_get_landed_state()
{
	// only trigger flight conditions if we are armed
	if (!_arming.armed) {
		return true;
	}

	bool landDetected = false;

	if (hrt_elapsed_time(&_local_pos.timestamp) < 500 * 1000) {

		// horizontal velocity
		float val = 0.97f * _velocity_xy_filtered + 0.03f * sqrtf(_local_pos.vx * _local_pos.vx + _local_pos.vy *
				_local_pos.vy);

		if (PX4_ISFINITE(val)) {
			_velocity_xy_filtered = val;
		}

		// vertical velocity
		val = 0.99f * _velocity_z_filtered + 0.01f * fabsf(_local_pos.vz);

		if (PX4_ISFINITE(val)) {
			_velocity_z_filtered = val;
		}

		_airspeed_filtered = 0.95f * _airspeed_filtered + 0.05f * _airspeed.true_airspeed_m_s;

		// a leaking lowpass prevents biases from building up, but
		// gives a mostly correct response for short impulses
		const float acc_hor = sqrtf(_sensors.accelerometer_m_s2[0] * _sensors.accelerometer_m_s2[0] +
					    _sensors.accelerometer_m_s2[1] * _sensors.accelerometer_m_s2[1]);
		_accel_horz_lp = _accel_horz_lp * 0.8f + acc_hor * 0.18f;

		// crude land detector for fixedwing
		landDetected = _velocity_xy_filtered < _params.maxVelocity
			       && _velocity_z_filtered < _params.maxClimbRate
			       && _airspeed_filtered < _params.maxAirSpeed
			       && _accel_horz_lp < _params.maxIntVelocity;

	} else {
		// Control state topic has timed out and we need to assume we're landed.
		landDetected = true;
	}

	return landDetected;
}

} // namespace land_detector
