#pragma config(Sensor, in1,    autoPoti,       sensorPotentiometer)
#pragma config(Sensor, in2,    mobilePoti,     sensorPotentiometer)
#pragma config(Sensor, in3,    liftPoti,       sensorPotentiometer)
#pragma config(Sensor, in4,    armPoti,        sensorPotentiometer)
#pragma config(Sensor, in5,    clawPoti,       sensorPotentiometer)
#pragma config(Sensor, in6,    expander,       sensorAnalog)
#pragma config(Sensor, dgtl1,  limBottom,      sensorTouch)
#pragma config(Sensor, dgtl2,  limTop,         sensorTouch)
#pragma config(Sensor, dgtl3,  driveEncL,      sensorQuadEncoder)
#pragma config(Sensor, dgtl5,  driveEncR,      sensorQuadEncoder)
#pragma config(Sensor, dgtl7,  latEnc,         sensorQuadEncoder)
#pragma config(Sensor, dgtl9,  armSonic,       sensorSONAR_mm)
#pragma config(Motor,  port1,           arm,           tmotorVex393_HBridge, openLoop, reversed)
#pragma config(Motor,  port2,           liftL1,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port3,           driveL1,       tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port4,           driveL2,       tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port5,           liftL2,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port6,           mobile,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port7,           driveR1,       tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port8,           driveR2,       tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port9,           liftR,         tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port10,          claw,          tmotorVex269_HBridge, openLoop)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

#include "Vex_Competition_Includes_Custom.c"

// Year-independent libraries

#include "motors.h"
#include "sensors.h"
#include "joysticks.h"
#include "cycle.h"
#include "utilities.h"

#include "motors.c"
#include "sensors.c"
#include "joysticks.c"
#include "cycle.c"
#include "utilities.c"

sCycleData gMainCycle;

//#define MOBILE_LIFT_SAFETY

/* Drive */
#define TCHN Ch4
#define PCHN Ch3
#define LCHN Ch2
#define ACHN Ch1

#define TDZ 10
#define PDZ 10
#define LDZ 25
#define ADZ 25

void setDrive(word left, word right)
{
	gMotor[driveL1].power = gMotor[driveL2].power = left;
	gMotor[driveR1].power = gMotor[driveR2].power = right;
}

void handleDrive()
{
	setDrive(gJoy[PCHN].cur + gJoy[TCHN].cur, gJoy[PCHN].cur - gJoy[TCHN].cur);
}

void stack();
task stackAsync();

/* Lift */
typedef enum _sLiftStates {
	liftIdle,
	liftRaise,
	liftLower,
	liftRaiseBrk,
	liftLowerBrk,
	liftHold,
	liftAuto
} sLiftStates;

#define LIFT_UP_KP 2
#define LIFT_DOWN_KP 1.5

#define LIFT_BOTTOM 110
#define LIFT_TOP 4094

short gLiftTarget;
sLiftStates gLiftState = liftIdle;
unsigned long liftStart;

void setLift(word power)
{
	gMotor[liftL1].power = gMotor[liftL2].power = gMotor[liftR].power = power;
}

void handleLift()
{
	if (RISING(Btn5U))
	{
		stopTask(stackAsync);
		gLiftTarget = LIFT_TOP;
		gLiftState = liftRaise;
		liftStart = nPgmTime;
	}
	else if (FALLING(Btn5U))
	{
		stopTask(stackAsync);
		gLiftTarget = gSensor[liftPoti].value;
	}
	if (RISING(Btn5D))
	{
		stopTask(stackAsync);
		gLiftTarget = LIFT_BOTTOM;
		gLiftState = liftLower;
		liftStart = nPgmTime;
	}
	if (RISING(Btn8L))
	{
		startTask(stackAsync);
	}

	switch (gLiftState)
	{
		case liftRaise:
		{
			short error = gLiftTarget - gSensor[liftPoti].value;
			if (error <= 0 || gSensor[limTop].value)
			{
				velocityClear(liftPoti);
				setLift(-10);
				gLiftState = liftRaiseBrk;
			}
			else
			{
				float output = error * LIFT_UP_KP;
				if (output < 60) output = 60;
				setLift((word)output);
			}
			break;
		}
		case liftRaiseBrk:
		{
			velocityCheck(liftPoti);
			if (gSensor[liftPoti].velGood && gSensor[liftPoti].velocity <= 0)
			{
				gLiftState = liftHold;
				writeDebugStreamLine("Lift up %d", nPgmTime - liftStart);
			}
			break;
		}
		case liftLower:
		{
			short error = gLiftTarget - gSensor[liftPoti].value;
			if (error >= 0 || gSensor[limBottom].value)
			{
				velocityClear(liftPoti);
				setLift(10);
				gLiftState = liftLowerBrk;
			}
			else
			{
				float output = error * LIFT_DOWN_KP;
				if (output > -60) output = -60;
				setLift((word)output);
			}
			break;
		}
		case liftLowerBrk:
		{
			velocityCheck(liftPoti);
			if (gSensor[liftPoti].velGood && gSensor[liftPoti].velocity >= 0)
			{
				gLiftState = liftHold;
				writeDebugStreamLine("Lift down %d", nPgmTime - liftStart);
			}
			break;
		}
		case liftHold:
		{
			if (gSensor[limBottom].value)
				setLift(-10);
			else
				setLift(12);
			break;
		}
	}
}


/* Arm */
typedef enum _sArmStates {
	armIdle,
	armRaise,
	armLower,
	armRaiseBrk,
	armLowerBrk,
	armHold,
	armAuto
} sArmStates;

#define ARM_UP_KP 0.15
#define ARM_DOWN_KP 0.08
#define ARM_POSITIONS (ARR_LEN(gArmPositions) - 1)

short gArmPositions[] = { 950, 1650, 2400 };
short gArmPosition = 0;
short gArmTarget;
sArmStates gArmState = armIdle;

void setArm(word power)
{
	if (power != gMotor[arm].power) writeDebugStreamLine("%d %d", gMainCycle.count, power);
	gMotor[arm].power = power;
}

void handleArm()
{
	if (RISING(Btn6U))
	{
		stopTask(stackAsync);
		if (gArmPosition < ARM_POSITIONS)
		{
			gArmTarget = gArmPositions[++gArmPosition];
			gArmState = armRaise;
		}
	}
	if (RISING(Btn6D))
	{
		stopTask(stackAsync);
		if (gArmPosition > 0)
		{
			gArmTarget = gArmPositions[--gArmPosition];
			gArmState = armLower;
		}
	}

	switch (gArmState)
	{
		case armRaise:
		{
			short error = gArmTarget - gSensor[armPoti].value;
			if (error <= 100)
			{
				velocityClear(armPoti);
				setArm(-8);
				gArmState = armRaiseBrk;
			}
			else
			{
				float output = error * ARM_UP_KP;
				if (output < 50) output = 50;
				setArm((word)output);
			}
			break;
		}
		case armRaiseBrk:
		{
			velocityCheck(armPoti);
			if (gSensor[armPoti].velGood && gSensor[armPoti].velocity <= 0) gArmState = armHold;
			break;
		}
		case armLower:
		{
			short error = gArmTarget - gSensor[armPoti].value;
			if (error >= -100)
			{
				velocityClear(armPoti);
				setArm(15);
				gArmState = armLowerBrk;
			}
			else
			{
				float output = error * ARM_DOWN_KP;
				if (output > -30) output = -30;
				setArm((word)output);
			}
			break;
		}
		case armLowerBrk:
		{
			velocityCheck(armPoti);
			if (gSensor[armPoti].velGood && gSensor[armPoti].velocity >= 0) gArmState = armHold;
			break;
		}
		case armHold:
		{
			if (gArmPosition == ARM_POSITIONS)
				setArm(10);
			else
				setArm(12);
			break;
		}
	}
}


/* Claw */

void setClaw(word power)
{
	gMotor[claw].power = power;
}

void handleClaw()
{
	if (RISING(Btn7U)) setClaw(127);
	if (RISING(Btn7D)) setClaw(-127);
	if (FALLING(Btn7U) || FALLING(Btn7D)) setClaw(0);
}


/* Mobile Goal */

typedef enum _sMobileStates {
	mobileIdle,
	mobileRaise,
	mobileLower,
	mobileLowerSlow,
	mobileLowerDecel,
	mobileHold
} sMobileStates;

#define MOBILE_TOP 2950
#define MOBILE_BOTTOM 600

#define MOBILE_UP_POWER 127
#define MOBILE_DOWN_POWER -127
#define MOBILE_UP_HOLD_POWER 10
#define MOBILE_DOWN_HOLD_POWER -10
#define MOBILE_DOWN_DECEL_POWER 4

short gMobileTarget;
sMobileStates gMobileState = mobileIdle;

void setMobile(word power)
{
	gMotor[mobile].power = power;
}

void handleMobile()
{
#ifdef MOBILE_LIFT_SAFETY
	if (gSensor[liftPoti].value > 2200)
	{
#endif
		if (RISING(Btn8U))
		{
			gMobileTarget = MOBILE_TOP;
			gMobileState = mobileRaise;
			setMobile(MOBILE_UP_POWER);
		}
		if (RISING(Btn8D))
		{
			gMobileTarget = MOBILE_BOTTOM;
			gMobileState = mobileLower;
			setMobile(MOBILE_DOWN_POWER);
		}
		if (RISING(Btn8R))
		{
			gMobileTarget = MOBILE_BOTTOM;
			gMobileState = mobileLowerSlow;
			setMobile(MOBILE_DOWN_POWER);
		}
#ifdef MOBILE_LIFT_SAFETY
	}
#endif

	switch (gMobileState)
	{
		case mobileRaise:
		{
			if (gSensor[mobilePoti].value >= gMobileTarget) gMobileState = mobileHold;
			break;
		}
		case mobileLower:
		{
			if (gSensor[mobilePoti].value <= gMobileTarget) gMobileState = mobileHold;
			break;
		}
		case mobileLowerSlow:
		{
			if (gSensor[mobilePoti].value <= gMobileTarget + 1400)
			{
				gMobileState = mobileLowerDecel;
				setMobile(MOBILE_DOWN_DECEL_POWER);
				velocityClear(mobilePoti);
			}
			break;
		}
		case mobileLowerDecel:
		{
			velocityCheck(mobilePoti);
			if ((gSensor[mobilePoti].velGood && gSensor[mobilePoti].velocity >= 0) || gSensor[mobilePoti].value <= gMobileTarget) gMobileState = mobileHold;
			break;
		}
		case mobileHold:
		{
			if (gMobileTarget == MOBILE_TOP)
				setMobile(MOBILE_UP_HOLD_POWER);
			else if (gMobileTarget == MOBILE_BOTTOM)
				setMobile(MOBILE_DOWN_HOLD_POWER);
			else
				setMobile(0);
			break;
		}
	}
}

int gNumCones = 0;
const int gStackPos[11] = { 0, 0, 0, 150, 320, 510, 720, 950, 1200, 1470, 1760 };

void stack()
{
	gLiftTarget = LIFT_BOTTOM + gStackPos[gNumCones];
	gLiftState = liftRaise;
	while (gLiftTarget - gSensor[liftPoti].value > 100) sleep(10);
	gArmTarget = gArmPositions[ARM_POSITIONS];
	gArmState = armRaise;
	while (gArmTarget - gSensor[armPoti].value > 100) sleep(10);
	sleep(200);
	setClaw(-127);
	sleep(500);
	if (gNumCones < 11) ++gNumCones;
	setClaw(0);
	gArmTarget = gArmPositions[1];
	gArmState = armLower;
	while (gArmTarget - gSensor[armPoti].value < -400) sleep(10);
	gLiftTarget = LIFT_BOTTOM;
	gLiftState = liftLower;
	while (gLiftTarget - gSensor[liftPoti].value < -100) sleep(10);
}

task stackAsync()
{
	stack();
}

/* LCD */

void handleLcd()
{
	string line;
	sprintf(line, "%4d %4d", gSensor[driveEncL].value, gSensor[driveEncR].value);

	clearLCDLine(0);
	displayLCDString(0, 0, line);
}

// This function gets called 2 seconds after power on of the cortex and is the first bit of code that is run
void startup()
{
	// Setup and initilize the necessary libraries
	setupMotors();
	setupSensors();
	setupJoysticks();

	gJoy[TCHN].deadzone = TDZ;
	gJoy[PCHN].deadzone = PDZ;
	gJoy[LCHN].deadzone = LDZ;
	gJoy[ACHN].deadzone = ADZ;
}

// This function gets called every 25ms during disabled (DO NOT PUT BLOCKING CODE IN HERE)
void disabled()
{

}

// This task gets started at the begining of the autonomous period
task autonomous()
{
	startSensors(); // Initilize the sensors
}

// This task gets started at the beginning of the usercontrol period
task usercontrol()
{
	startSensors(); // Initilize the sensors
	initCycle(gMainCycle, 10);

	while (true)
	{
		updateSensorInputs();
		updateJoysticks();

		handleDrive();
		handleLift();
		handleArm();
		handleClaw();
		handleMobile();

		handleLcd();

		updateSensorOutputs();
		updateMotors();
		endCycle(gMainCycle);
	}
}
