/*
 * DriveTrain.cpp
 *
 *  Created on: Nov 10, 2018
 *      Author: super-tails
 */

#include "subsystems/DriveTrain.h"
#include "Robot.h"
#include <iostream>
#include <algorithm>
#include <SmartDashboard/SmartDashboard.h>

bool DriveTrain::leftSidePhase() const {
	return frc::SmartDashboard::GetBoolean("Left Sensor Phase", false);
}

void DriveTrain::setLeftSidePhase(bool phase) {
	frc::SmartDashboard::SetPersistent("Left Sensor Phase");
	frc::SmartDashboard::PutBoolean("Left Sensor Phase", phase);

	m_frontLeft.SetSensorPhase(phase);
}

bool DriveTrain::rightSidePhase() const {
	return frc::SmartDashboard::GetBoolean("Right Sensor Phase", false);
}

void DriveTrain::setRightSidePhase(bool phase) {
	frc::SmartDashboard::SetPersistent("Right Sensor Phase");
	frc::SmartDashboard::PutBoolean("Right Sensor Phase", phase);

	m_frontRight.SetSensorPhase(phase);
}

std::pair< int, int > DriveTrain::getEncoderPositions() {
	return { m_frontLeft.GetSelectedSensorPosition(), m_frontRight.GetSelectedSensorPosition() };
}

DriveTrain::DriveTrain(int frontLeft, int midLeft, int backLeft, int frontRight, int midRight, int backRight) :
	m_frontLeft(frontLeft),
	m_midLeft(midLeft),
	m_backLeft(backLeft),
	m_frontRight(frontRight),
	m_midRight(midRight),
	m_backRight(backRight),
	m_arm(7),
	m_shiftFast(0),
	m_shiftSlow(1),
	Subsystem("DriveTrain")
{
	if (!frc::SmartDashboard::SetDefaultBoolean("Left Sensor Phase", false)) {
		std::cout << "Setting default left side phase\n";
		setLeftSidePhase(false);
	}
	else {
		setLeftSidePhase(leftSidePhase());
	}
	if (!frc::SmartDashboard::SetDefaultBoolean("Right Sensor Phase", false)) {
		std::cout << "Setting default right side phase\n";
		setRightSidePhase(false);
	}
	else {
		setRightSidePhase(rightSidePhase());
		std::cout << "Setting default right side phase\n";
		setRightSidePhase(false);
	}

	m_midLeft.Follow(m_frontLeft);
	m_backLeft.Follow(m_frontLeft);

	m_midRight.Follow(m_frontRight);
	m_backRight.Follow(m_frontRight);

	m_frontLeft.SetInverted(true);
	m_midLeft.SetInverted(true);
	m_backLeft.SetInverted(true);

	// Configure both front left and front right talons identically
	std::array< WPI_TalonSRX*, 2 > talons = { &m_frontLeft, &m_frontRight };
	for (int i = 0; i < 2; ++i) {
		talons[i]->ConfigNominalOutputForward(0, 10);
		talons[i]->ConfigNominalOutputReverse(0, 10);
		talons[i]->ConfigPeakOutputForward(1, 10);
		talons[i]->ConfigPeakOutputReverse(-1, 10);

		talons[i]->ConfigSelectedFeedbackSensor(FeedbackDevice::QuadEncoder, 0, 10);
		talons[i]->ConfigClosedloopRamp(0.0, 10);
	}

	m_frontLeft.Config_kF(0, 0.0779131, 10);
	m_frontLeft.Config_kP(0, 0.07, 10);
	m_frontLeft.Config_kI(0, 0.0, 10);
	m_frontLeft.Config_kD(0, 0, 10);

	m_frontRight.Config_kF(0, 0.0905309, 10);
	m_frontRight.Config_kP(0, 0.09, 10);
	m_frontRight.Config_kI(0, 0 , 10);
	m_frontRight.Config_kD(0, 0, 10);


	m_frontLeft.Config_kF(1, 0.0300882, 10);
	m_frontLeft.Config_kP(1, 0.03, 10);
	m_frontLeft.Config_kI(1, 0.0, 10);
	m_frontLeft.Config_kD(1, 0, 10);

	m_frontRight.Config_kF(1, 0.0343288, 10);
	m_frontRight.Config_kP(1, 0.03, 10);
	m_frontRight.Config_kI(1, 0.0, 10);
	m_frontRight.Config_kD(1, 0, 10);

	m_frontLeft.SetSensorPhase(false);
	m_frontRight.SetSensorPhase(false);

	frc::Scheduler::GetInstance()->RegisterSubsystem(this);
}

void DriveTrain::InitDefaultCommand() {
	SetDefaultCommand(Robot::m_manualControl.get());
}

void DriveTrain::resetSensors() {
	m_frontLeft.SetSelectedSensorPosition(0, 0, 10);
	m_frontRight.SetSelectedSensorPosition(0, 0, 10);
}

bool fast = false;

void DriveTrain::drive(InputState state) {
	double lSpeed = -state.y + state.r;
	double rSpeed = -state.y - state.r;
	lSpeed = std::max(std::min(lSpeed, 1.0), -1.0);
	rSpeed = std::max(std::min(rSpeed, 1.0), -1.0);

	m_shiftFast.Set(buttonValue(state, "SHIFT_FAST"));
	m_shiftSlow.Set(buttonValue(state, "SHIFT_SLOW"));

	fast |= buttonValue(state, "SHIFT_FAST");
	fast &= !buttonValue(state, "SHIFT_SLOW");

	drive(lSpeed, rSpeed, buttonValue(state, "TRIGGER"));
}
/*void DriveTrain::driveElevator() {
	drive(0.1,0.1, false);
}*/

template < typename T >
int signum(const T& value) {
	return (value > 0) - (value < 0);
}

void DriveTrain::calibratePhase(double leftSpeed, double rightSpeed) {
	// Allow time for measured velocity to change after inverting a phase
	static int debounce = 0;

	if (debounce > 0) {
		--debounce;
		return;
	}

	double leftVelocity = m_frontLeft.GetSelectedSensorVelocity();
	if (std::abs(leftSpeed) > 0.5 && std::abs(leftVelocity) > 10 && signum(leftVelocity) != signum(leftSpeed)) {
		bool currentPhase = leftSidePhase();
		std::cout << "Left side sensor phase '" << std::boolalpha << currentPhase << "' incorrect, inverting...\n";
		setLeftSidePhase(!currentPhase);
		debounce = 1000;
	}

	double rightVelocity = m_frontRight.GetSelectedSensorVelocity();
	if (std::abs(rightSpeed) > 0.5 && std::abs(rightVelocity) > 10 && signum(rightVelocity) != signum(rightSpeed)) {
		bool currentPhase = rightSidePhase();
		std::cout << "Right side sensor phase '" << std::boolalpha << currentPhase << "' incorrect, inverting...\n";
		setRightSidePhase(!currentPhase);
		debounce = 1000;
	}
}

void DriveTrain::drive(double leftSpeed, double rightSpeed, bool percent) {
	//calibratePhase(leftSpeed, rightSpeed);

	if (percent) {
		m_frontLeft.Set(ControlMode::PercentOutput, leftSpeed);
		m_frontRight.Set(ControlMode::PercentOutput, rightSpeed);
	}
	else {
		if (fast) {
			m_frontLeft.SelectProfileSlot(1, 0);
			m_frontRight.SelectProfileSlot(1, 0);

			m_frontLeft.Set(ControlMode::Velocity, leftSpeed * 28000.0);
			m_frontRight.Set(ControlMode::Velocity, rightSpeed * 28000.0);
		}
		else {
			m_frontLeft.SelectProfileSlot(0, 0);
			m_frontRight.SelectProfileSlot(0, 0);

			m_frontLeft.Set(ControlMode::Velocity, leftSpeed * 10000.0);
			m_frontRight.Set(ControlMode::Velocity, rightSpeed * 10000.0);
		}
	}

	static MovingAverage leftVelocityAverage(30);
	static MovingAverage rightVelocityAverage(30);

	const double leftVelocity = m_frontLeft.GetSelectedSensorVelocity(0);
	const double rightVelocity = m_frontRight.GetSelectedSensorVelocity(0);

	frc::SmartDashboard::PutNumber("Left Side Speed", leftSpeed);
	frc::SmartDashboard::PutNumber("Right Side Speed", rightSpeed);

	frc::SmartDashboard::PutNumber("Left Side Velocity", leftVelocityAverage.Process(leftVelocity));
	frc::SmartDashboard::PutNumber("Right Side Velocity", rightVelocityAverage.Process(rightVelocity));


	frc::SmartDashboard::PutNumber("Left Side Error", m_frontLeft.GetClosedLoopError(fast));
	frc::SmartDashboard::PutNumber("Right Side Error", m_frontRight.GetClosedLoopError(fast));
}
