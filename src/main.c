#pragma config(Sensor, in1,    autoPoti,       sensorPotentiometer)
#pragma config(Sensor, in2,    mobilePoti,     sensorPotentiometer)
#pragma config(Sensor, in3,    liftPoti,       sensorPotentiometer)
#pragma config(Sensor, in4,    armPoti,        sensorPotentiometer)
#pragma config(Sensor, in5,    clawPoti,       sensorPotentiometer)
#pragma config(Sensor, in6,    expander,       sensorAnalog)
#pragma config(Sensor, in7,    leftLine,       sensorLineFollower)
#pragma config(Sensor, in8,    rightLine,      sensorLineFollower)
#pragma config(Sensor, dgtl1,  armSonic,       sensorSONAR_mm)
#pragma config(Sensor, dgtl3,  limBottom,      sensorTouch)
#pragma config(Sensor, dgtl4,  limTop,         sensorTouch)
#pragma config(Sensor, dgtl5,  driveEncL,      sensorQuadEncoder)
#pragma config(Sensor, dgtl7,  driveEncR,      sensorQuadEncoder)
#pragma config(Sensor, dgtl9,  latEnc,         sensorQuadEncoder)
#pragma config(Sensor, dgtl11, frontSonic,     sensorSONAR_mm)
#pragma config(Motor,  port1,           mobile,        tmotorVex393HighSpeed_HBridge, openLoop)
#pragma config(Motor,  port2,           liftR,         tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port3,           driveL1,       tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port4,           driveL2,       tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port5,           claw,          tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port6,           arm,           tmotorVex393_MC29, openLoop)
#pragma config(Motor,  port7,           driveR1,       tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port8,           driveR2,       tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port9,           liftL,         tmotorVex393HighSpeed_MC29, openLoop, reversed)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

#include "Vex_Competition_Includes_Custom.c"

// Year-independent libraries

#include "task.h"
#include "async.h"
#include "motors.h"
#include "sensors.h"
#include "joysticks.h"
#include "cycle.h"
#include "utilities.h"
#include "pid.h"

#include "task.c"
#include "async.c"
#include "motors.c"
#include "sensors.c"
#include "joysticks.c"
#include "cycle.c"
#include "utilities.c"
#include "pid.c"

#include "controls.h"

unsigned long gOverAllTime = 0;
sCycleData gMainCycle;
int gNumCones = 0;

/* Drive */
void setDrive(word left, word right)
{
	gMotor[driveL1].power = gMotor[driveL2].power = left;
	gMotor[driveR1].power = gMotor[driveR2].power = right;
}

void handleDrive()
{
	//gJoy[JOY_TURN].deadzone = MAX(abs(gJoy[JOY_THROTTLE].cur) / 2, DZ_ARM);
	setDrive(gJoy[JOY_THROTTLE].cur + gJoy[JOY_TURN].cur, gJoy[JOY_THROTTLE].cur - gJoy[JOY_TURN].cur);
	if (RISING(JOY_TURN)) playSound(soundShortBlip);
}




/* Lift */
typedef enum _sLiftStates {
	liftManaged,
	liftIdle,
	//liftRaise,
	//liftLower,
	//liftRaiseBrk,
	//liftLowerBrk,
	liftManual,
	liftHold,
	liftAuto
} sLiftStates;

#define LIFT_UP_KP 2
#define LIFT_DOWN_KP 1.5

#define LIFT_BOTTOM 110
#define LIFT_TOP 4094

int gLiftTarget;
sLiftStates gLiftState = liftIdle;
unsigned long gLiftStart;

void setLift(word power,bool debug=false)
{
	if( debug )	writeDebugStreamLine("%06d Lift %4d", nPgmTime-gOverAllTime,power );
	gMotor[liftL].power = gMotor[liftR].power = power;
	//	Motor[liftL] = Motor[liftR] = power;
}

void handleLift()
{

	if (RISING(JOY_LIFT))
	{
		gLiftState = liftManual;
	}
	if (FALLING(JOY_LIFT))
	{
		gLiftState = liftHold;
	}

	if( gLiftState == liftManaged ) return;

	switch (gLiftState)
	{

	case liftManual:
		{
			word value = gJoy[JOY_LIFT].cur * 2 - 128 * sgn(gJoy[JOY_LIFT].cur);
			if (gSensor[limBottom].value && value < -10) value = -10;
			if (gSensor[limTop].value && value > 10) value = 10;
			setLift(value);
			break;
		}
	case liftHold:
		{
			if (gSensor[liftPoti] < 10)
				setLift(-10);
			else
				setLift(12);
			break;
		}
	case liftIdle:
		{
			setLift(0);
			break;
		}
	}
}


/* Arm */
typedef enum _sArmStates {
	armManaged,
	armIdle,
	armManual,
	armPlainPID,
	armRaise,
	armLower,
	armHold,
	armHorizontal
} sArmStates;

#define ARM_UP_KP 0.25
#define ARM_DOWN_KP 0.25
#define ARM_POSITIONS (ARR_LEN(gArmPositions) - 1)

#define ARM_TOP 2060
#define ARM_BOTTOM 115

short gArmPositions[] = { 130, 750, 2050 };
word gArmHoldPower[] = { -12, 0, 10 };
short gArmPosition = 2;
short gArmTarget;
sArmStates gArmState = armIdle;
unsigned long gArmStart;
sPID gArmPID;
sPID* gArmPIDInUse = &gArmPID;

void setArm(word power, bool debug = false)
{
	if( debug ) writeDebugStreamLine("%06d Arm  %4d", nPgmTime-gOverAllTime, power);
	gMotor[arm].power = power;
	//	motor[arm]=power;
}

void handleArm()
{
	if (RISING(JOY_ARM))
	{
		gArmState = armManual;
	}
	if (FALLING(JOY_ARM) && gArmState == armManual)
	{
		if (gSensor[armPoti].value <= ARM_BOTTOM) gArmPosition = 0;
		else if (gSensor[armPoti].value >= ARM_TOP) gArmPosition = 2;
		else gArmPosition = 1;
		gArmState = armHold;
	}
	if (RISING(BTN_ARM_DOWN))
	{
		gArmState = armLower;

		gArmPosition = 0;

		gArmTarget = gArmPositions[gArmPosition];
		gArmStart = nPgmTime;
	}

	if( gArmState==armManaged ) return;

	switch (gArmState)
	{
	case armManual:
		{
			word value = gJoy[JOY_ARM].cur * 2 - 128 * sgn(gJoy[JOY_ARM].cur);
			if (gSensor[armPoti].value >= ARM_TOP && value > 10) value = 10;
			if (gSensor[armPoti].value <= ARM_BOTTOM && value < -10) value = -10;
			setArm(value);
			break;
		}
	case armPlainPID:
		{
			int value = gSensor[armPoti].value;
			if (gArmTarget >= ARM_TOP && value >= ARM_TOP)
			{
				writeDebugStreamLine("Arm raise: %d", nPgmTime - gArmStart);
				gArmState = armHold;
			}
			else if (gArmTarget <= ARM_BOTTOM && value <= ARM_BOTTOM)
			{
				writeDebugStreamLine("Arm lower: %d", nPgmTime - gArmStart);
				gArmState = armHold;
			}
			else
			{
				pidCalculate(gArmPID, (float)gArmTarget, (float)value);
				setArm((word)gArmPID.output);
			}
			break;
		}
	case armRaise:
		{
			if (gSensor[armPoti].value >= gArmTarget)
			{
				writeDebugStreamLine("Arm raise: %d", nPgmTime - gArmStart);
				gArmState = armHold;
			}
			else setArm(127);
			break;
		}
	case armLower:
		{
			if (gSensor[armPoti].value <= gArmTarget)
			{
				writeDebugStreamLine("Arm lower: %d", nPgmTime - gArmStart);
				gArmState = armHold;
			}
			else setArm(-127);
			break;
		}
	case armHold:
		{
			if( gArmPosition == 1 )
			{
				if( gSensor[armPoti].value < 1700 )
					setArm(15);
				else
					setArm(9);
			}
			else
				setArm(gArmHoldPower[gArmPosition]);


			//setArm(gArmPosition == 1 ? gSensor[armPoti].value < 1700 ? 15 : 9 : gArmHoldPower[gArmPosition]);
			break;
		}
	case armHorizontal:
		{
			int value = gSensor[armPoti].value;
			if (abs(gArmTarget - value) < 400)
			{
				velocityCheck(armPoti);
				if (gSensor[armPoti].velGood)
				{
					int power = 10 - gSensor[armPoti].velocity / 2;
					setArm(LIM_TO_VAL(power, 15));
				}
			}
			else if (value > gArmTarget) setArm(-127);
			else setArm(127);
			break;
		}
	case armIdle:
		{
			setArm(0);
			break;
		}
	}
}


/* Claw */

typedef enum _sClawStates {
	clawManaged,
	clawIdle,
	clawOpening,
	clawClosing,
	clawOpened,
	clawClosed
} sClawStates;

#define CLAW_CLOSE_POWER 127
#define CLAW_OPEN_POWER -127
#define CLAW_CLOSE_HOLD_POWER 15
#define CLAW_OPEN_HOLD_POWER -12

#define CLAW_OPEN 1630
#define CLAW_CLOSE 610

#define CLAW_TIMEOUT 1000

sClawStates gClawState = clawIdle;
unsigned long gClawStart;

void setClaw(word power, bool debug=false )
{
	if( debug )	writeDebugStreamLine("%06d Claw %4d", nPgmTime-gOverAllTime,power );
	gMotor[claw].power = power;
	//	motor[claw]= power;
}

void handleClaw()
{
	if (RISING(BTN_CLAW_CLOSE))
	{
		gClawState = clawClosing;
		setClaw(CLAW_CLOSE_POWER);
		gClawStart = nPgmTime;
	}

	if (RISING(BTN_CLAW_OPEN))
	{
		gClawState = clawOpening;
		setClaw(CLAW_OPEN_POWER);
		gClawStart = nPgmTime;
	}

	if( gClawState==clawManaged ) return;

	switch (gClawState)
	{
	case clawIdle:
		{
			setClaw(0);
			break;
		}
	case clawOpening:
		{
			if (gSensor[clawPoti].value >= CLAW_OPEN && nPgmTime - gClawStart < CLAW_TIMEOUT) gClawState = clawOpened;
			break;
		}
	case clawClosing:
		{
			if (gSensor[clawPoti].value <= CLAW_CLOSE && nPgmTime - gClawStart < CLAW_TIMEOUT) gClawState = clawClosed;
			break;
		}
	case clawOpened:
		{
			setClaw(CLAW_OPEN_HOLD_POWER);
			break;
		}
	case clawClosed:
		{
			setClaw(CLAW_CLOSE_HOLD_POWER);
			break;
		}
	}
}


/* Mobile Goal */

typedef enum _sMobileStates {
	mobileManaged,
	mobileIdle,
	mobileRaise,
	mobileLower,
	mobileHold,
	mobileUpToMiddle,
	mobileDownToMiddle
} sMobileStates;

#define MOBILE_TOP 2950
#define MOBILE_BOTTOM 600
#define MOBILE_MIDDLE_UP 1200
#define MOBILE_MIDDLE_DOWN 2200

#define MOBILE_UP_POWER 127
#define MOBILE_DOWN_POWER -127
#define MOBILE_UP_HOLD_POWER 10
#define MOBILE_DOWN_HOLD_POWER -10

short gMobileTarget;
word gMobileHoldPower;
sMobileStates gMobileState = mobileIdle;
sMobileStates gMobileNextState;
unsigned long gMobileStart;

void setMobile(word power)
{
	gMotor[mobile].power = power;
}

void handleMobile()
{
	if (RISING(BTN_MOBILE_TOGGLE))
	{
		if (gMobileState == mobileLower || gMobileState == mobileHold && gMobileTarget == MOBILE_BOTTOM)
		{
			gMobileTarget = MOBILE_TOP;
			gMobileState = mobileRaise;
			gMobileHoldPower = MOBILE_UP_HOLD_POWER;
			setMobile(MOBILE_UP_POWER);
		}
		else
		{
			gMobileTarget = MOBILE_BOTTOM;
			gMobileState = mobileLower;
			gMobileHoldPower = MOBILE_DOWN_HOLD_POWER;
			setMobile(MOBILE_DOWN_POWER);
		}
		gMobileNextState = mobileHold;
	}
	if (RISING(BTN_MOBILE_MIDDLE))
	{
		if (gSensor[mobilePoti].value > MOBILE_MIDDLE_DOWN)
		{
			gMobileTarget = MOBILE_MIDDLE_DOWN;
			gMobileState = mobileLower;
			gMobileNextState = mobileDownToMiddle;
			setMobile(MOBILE_DOWN_POWER);
		}
		else
		{
			gMobileTarget = MOBILE_MIDDLE_UP;
			gMobileState = mobileRaise;
			gMobileNextState = mobileUpToMiddle;
			setMobile(MOBILE_UP_POWER);
		}
	}

	if( gMobileState==mobileManaged ) return;

	switch (gMobileState)
	{
	case mobileRaise:
		{
			if (gSensor[mobilePoti].value >= gMobileTarget)
			{
				gMobileState = gMobileNextState;
				gMobileStart = nPgmTime;
			}
			break;
		}
	case mobileLower:
		{
			if (gSensor[mobilePoti].value <= gMobileTarget)
			{
				gMobileState = gMobileNextState;
				gMobileStart = nPgmTime;
				gNumCones = 0;
			}
			break;
		}
	case mobileHold:
		{
			setMobile(gMobileHoldPower);
			break;
		}
	case mobileDownToMiddle:
		{
			if( nPgmTime - gMobileStart > 300 )
				setMobile( 10 );
			else
				setMobile( -5 );

			break;
		}
	case mobileUpToMiddle:
		{
			setMobile(15);
			break;
		}
	case mobileIdle:
		{
			setMobile(0);
			break;
		}
	}
}


/* Macros */

bool gLiftAsyncDone;
bool gContinueLoader = false;
bool gMacros[20] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
bool gLiftTargetReached;



task stackAsync();
task LiftTaskUp();
task dropArm();

bool gKillDriveOnTimeout = false;

bool TimedOut( unsigned long timeOut )
{
	if( nPgmTime > timeOut )
	{
		hogCPU();
		gMotor[arm].power =	gMotor[claw].power = gMotor[liftL].power =  gMotor[liftR].power = 0;
		if (gKillDriveOnTimeout) setDrive(0, 0);
		updateMotors();
		writeDebugStreamLine ("%06d EXCEEDED TIME %d", nPgmTime-gOverAllTime,timeOut-gOverAllTime);
		gArmState= armHold;
		gLiftState = liftHold;
		gClawState = clawIdle;
		gMacros[stackAsync] = false;
		tStopRoot();
		return true;
	}
	else
		return false;
}

task LiftTaskUp()
{
	int p;
	writeDebugStreamLine("%d LiftTask started %d", nPgmTime-gOverAllTime, gLiftTarget);
	gLiftTargetReached = false;
	unsigned long gLiftTopTime = 0;
	int minPower =35;
	if( gLiftTarget == 4095) minPower = 60;
	if( gLiftTarget <200 ) 	minPower = 35;
	writeDebugStreamLine("target=%d current=%d",gLiftTarget,gSensor[liftPoti].value);
	while (!gLiftTargetReached )
	{
		float err = gLiftTarget - gSensor[liftPoti].value;
		bool limTopVal = (bool)gSensor[limTop].value;
		if ( (err < 20 && gLiftTarget != 4095) || limTopVal )
		{
			writeDebugStreamLine("Limit: %d", limTopVal);
			gLiftTargetReached = true;
			if( limTopVal ) setLift(25,true); else setLift(15,true);
			break;
		}
		else
		{
			p = err*0.44;
		}

		if (p < minPower) p = minPower;
		setLift(p);
		sleep(10);
	}
	writeDebugStreamLine("%06d LiftTask ended", nPgmTime-gOverAllTime);
	writeDebugStreamLine("target=%d reached=%d",gLiftTarget,gSensor[liftPoti].value);

	return_t;
}

bool gArmDown;
task dropArm()
{
	unsigned long armTimeOut;
	gArmDown = false;
	setArm(-127,true);
	armTimeOut = nPgmTime + 500;
	while( gSensor[armPoti].value > ARM_BOTTOM  + 700 && !TimedOut(armTimeOut) ) sleep(10);
	setArm(30,true);
	sleep(60);
	setArm(-90,true);
	sleep(60);
	setArm(-15,true);
	gArmDown = true;

	return_t;
}

int gliftTargetA[11] = { 0, 0, 70, 190, 450, 975, 1500, 1900, 2600, 3250, 4095-LIFT_BOTTOM };

void stack()
{
	gArmState = armManaged;
	gClawState = clawManaged;
	gLiftState = liftManaged;
	unsigned long armTimeOut;
	unsigned long clawTimeOut;
	unsigned long liftTimeOut;

	if( gNumCones< 2 )
	{
		gOverAllTime = nPgmTime;
		writeDebugStreamLine(" STACKING on %d", gNumCones );

		clawTimeOut = nPgmTime + 300;
		setClaw(128, true);
		while( SensorValue[clawPoti] > CLAW_CLOSE+200 && !TimedOut(clawTimeOut) )  sleep(10);
		setArm(128, true);
		clawTimeOut = nPgmTime + 300;
		while( SensorValue[clawPoti] > CLAW_CLOSE && !TimedOut(clawTimeOut) )  sleep(10);

		//claw holding power while lifting
		setClaw (20, true) ;
		clawTimeOut = nPgmTime +600;
		while( SensorValue[armPoti] < ARM_TOP - 600 && !TimedOut(clawTimeOut)) sleep(10);
		//squeeze claw as it approaches the top of the arm swing
		setClaw ( 40, true );
		armTimeOut = nPgmTime +350;
		while( SensorValue[armPoti] < ARM_TOP && !TimedOut(armTimeOut) )  sleep(10);
		setArm(15, true);
		setClaw(25, true);
		sleep(450);
		setClaw(-128, true);
		while( SensorValue[clawPoti] < CLAW_OPEN-500 )  sleep(10);
		gArmDown= false;
		writeDebugStreamLine("%06d starting Task dropArm", nPgmTime-gOverAllTime);
		tStart( dropArm );

		while( SensorValue[clawPoti] < CLAW_OPEN )  sleep(10);
		setClaw(-10);
		gNumCones++;
		while( !gArmDown) sleep(10);
		setArm(-15,true);
		writeDebugStreamLine("stack time %dms",nPgmTime- gOverAllTime );

	}
	else if (gNumCones == 2)
	{

		gOverAllTime = nPgmTime;
		writeDebugStreamLine(" STACKING on %d", gNumCones );
		clawTimeOut = nPgmTime + 300;
		setClaw(128,true);
		while( SensorValue[clawPoti] > CLAW_CLOSE+200 && !TimedOut(clawTimeOut) )  sleep(10);

		setArm(128,true);
		armTimeOut = nPgmTime + 600;
		while( SensorValue[clawPoti] > CLAW_CLOSE && !TimedOut(clawTimeOut) )  sleep(10);

		setClaw (20,true) ;
		gLiftTarget = LIFT_BOTTOM +  gliftTargetA[gNumCones];
		tStart( LiftTaskUp );

		while( SensorValue[armPoti] < ARM_TOP - 600 && !TimedOut(armTimeOut)) sleep(10);

		setClaw (40);
		while( SensorValue[armPoti] < ARM_TOP && !TimedOut(armTimeOut))  sleep(10);

		setArm(20);
		setClaw(15);
		sleep(125);
		setArm(15);

		setClaw(-128);
		while( SensorValue[clawPoti] < CLAW_OPEN)  sleep(10);
		gNumCones++;
		setLift(-30);
		sleep(50);
		gArmDown= false;
		tStart( dropArm );
		setClaw(-10);
		setLift(-12);
		while( !gArmDown) sleep(10);
		writeDebugStreamLine( " STACKTIME = %d ms", nPgmTime- gOverAllTime);
	}
	else if (gNumCones == 3)
	{
		gOverAllTime = nPgmTime;
		writeDebugStreamLine(" -------------STACKING on %d", gNumCones );
		clawTimeOut = nPgmTime + 300;
		setClaw(128,true);
		while( SensorValue[clawPoti] > CLAW_CLOSE+200 && !TimedOut(clawTimeOut) )  sleep(10);

		setArm(128,true);
		armTimeOut = nPgmTime + 600;
		while( SensorValue[clawPoti] > CLAW_CLOSE && !TimedOut(clawTimeOut) )  sleep(10);

		setClaw (20,true) ;
		gLiftTarget = LIFT_BOTTOM +  gliftTargetA[gNumCones];
		tStart( LiftTaskUp );
		while( SensorValue[armPoti] < ARM_TOP - 600 && !TimedOut(armTimeOut)) sleep(10);

		setClaw (40,true);
		while( SensorValue[armPoti] < ARM_TOP && !TimedOut(armTimeOut))  sleep(10);

		setArm (15,true);
		setClaw (15,true);
		setLift(-30,true);
		sleep(150);
		setClaw(-128,true);

		while ( SensorValue[liftPoti] > gLiftTarget-200  && !SensorValue[limBottom] ) sleep(10);
		setLift(15,true);

		writeDebugStreamLine("%d sleep(100)", nPgmTime-gOverAllTime);
		sleep(100);

		gLiftTarget = LIFT_BOTTOM + gliftTargetA[3] ;

		tStart( LiftTaskUp );
		while( SensorValue[clawPoti] < CLAW_OPEN )  sleep(10);
		gNumCones++;
		setClaw(-10,true);
		while( !gLiftTargetReached) sleep(10);

		armTimeOut = nPgmTime + 600;
		gArmDown= false;
		tStart( dropArm );
		while( SensorValue[armPoti] > ARM_TOP-600  && !TimedOut(armTimeOut))  sleep(10);
		setLift(-30,true);
		while ( !SensorValue[limBottom] ) sleep(10);
		setLift(-10,true);
		while( !gArmDown ) sleep(10);

		writeDebugStreamLine( " STACKTIME = %d ms", nPgmTime- gOverAllTime);

	}
	else
	{


		writeDebugStreamLine(" -------------STACKING on %d", gNumCones );

		// if the claw is open then make sure the arm is down close the claw
		if( gSensor[clawPoti].value > CLAW_CLOSE+100 )
		{
			setArm(-50,true);
			armTimeOut = nPgmTime + 500;
			while( gSensor[armPoti].value > ARM_BOTTOM && !TimedOut(armTimeOut) ) sleep(10);
		}

		gOverAllTime = nPgmTime;
		setArm(-15,true);
		setClaw(CLAW_CLOSE_POWER,true);
		clawTimeOut = nPgmTime + 300;
		while( gSensor[clawPoti].value > CLAW_CLOSE+300 && !TimedOut(clawTimeOut) ) sleep(10);

		// raise the arm while closing the claw along the way.
		setArm(127,true);
		armTimeOut = nPgmTime + 500;
		clawTimeOut = nPgmTime + 300;
		while( gSensor[clawPoti].value > CLAW_CLOSE && !TimedOut(clawTimeOut) ) sleep(10);
		setClaw(CLAW_CLOSE_HOLD_POWER,true);

		//wait for the arm to pull the cone away from the base before raising the lift
		while( gSensor[armPoti].value < ARM_BOTTOM+100 && !TimedOut(armTimeOut)) sleep(10);//200

		setArm(25,true);
		tStart( LiftTaskUp );

		//slow the arm down otherwise it will hit the stack on the way up
		while ( gSensor[liftPoti].value < gLiftTarget - 600 ) sleep(10);
		armTimeOut = nPgmTime + 650;
		setArm(127,true);

		// tighten the claw just before hitting the top
		while( gSensor[armPoti].value < ARM_TOP-600 && !TimedOut(armTimeOut) ) sleep(10);
		setClaw(40,true);

		//arm reached the top
		while( gSensor[armPoti].value < ARM_TOP && !TimedOut(armTimeOut) ) sleep(10);
		setArm(25,true);
		setClaw(CLAW_CLOSE_HOLD_POWER,true);


		if( gNumCones < 10 )
		{
			setLift(-80,true);
			liftTimeOut = nPgmTime+400;
			while( gSensor[liftPoti].value > gLiftTarget-60 && !gSensor[limBottom].value && !TimedOut(liftTimeOut) ) sleep(10);
			setLift(15,true);

			setClaw(CLAW_OPEN_POWER,true);
			sleep(150);
			// dont lift as high as the original stack target
			gLiftTarget = gLiftTarget-60;
			tStart(LiftTaskUp);
			while( gSensor[clawPoti].value< CLAW_OPEN ) sleep(10);
			setClaw(CLAW_OPEN_HOLD_POWER,true);
			gNumCones++;
			while( !gLiftTargetReached ) sleep(10);

			gArmDown = false;
			tStart( dropArm );
			while( gSensor[armPoti].value > ARM_TOP-400 )  sleep(10);
			setLift(-127);
			while ( !gSensor[limBottom].value ) sleep(10);
			setLift(-10);
			while( !gArmDown) sleep(10);
		}
		else
		{
			setArm(15,true);
			setLift(15,true);
			sleep(200);
			setLift(-50,true);
			liftTimeOut = nPgmTime + 700;
			gLiftTarget = 3100;
			while( gSensor[liftPoti].value > gLiftTarget && !gSensor[limBottom].value && !TimedOut(liftTimeOut) ) sleep(10);
			setLift(15, true);
			setClaw(10,true);
		}

		writeDebugStreamLine( " STACKTIME = %d ms", nPgmTime- gOverAllTime);
	}
}



task stackAsync()
{
	gMacros[stackAsync] = true;
	gLiftTarget =LIFT_BOTTOM + gliftTargetA[gNumCones];
	stack();
	gMacros[stackAsync] = false;
	return_t;
}


task stackFromLoaderAsync()
{


	return_t;
}

void handleMacros()
{

	if (RISING(BTN_MACRO_STACK) && gNumCones < 11 )
	{
		writeDebugStreamLine("Stacking");
		tStart(stackAsync, true);
		playSound(soundUpwardTones);
	}

	if (RISING(BTN_MACRO_STACK_CANCEL) && gMacros[stackAsync])
	{
		tStopAll(stackAsync);
		gMacros[stackAsync] = false;
		gLiftState = liftIdle;
		gArmState = armIdle;
		gClawState = clawIdle;
		writeDebugStreamLine("Stack cancelled");
	}
	if (RISING(BTN_MACRO_LOADER))
	{
		gContinueLoader = false;
		if (gMacros[stackFromLoaderAsync])
		{
			tStopAll(stackFromLoaderAsync);
			gMacros[stackFromLoaderAsync] = false;
			gLiftState = liftIdle;
			gArmState = armIdle;
			gClawState = clawIdle;
			writeDebugStreamLine("Stack from loader cancelled");
		}
		else
		{
			tStart(stackFromLoaderAsync);
			writeDebugStreamLine("Stacking from loader");
		}
	}
	if (FALLING(BTN_MACRO_LOADER) && gMacros[stackFromLoaderAsync]) gContinueLoader = true;
	//if (RISING(JOY_ADJUST))
	//{
	//	if (gJoy[JOY_ADJUST].cur > 0 && gNumCones < 11) ++gNumCones;
	//	else if (gJoy[JOY_ADJUST].cur < 0 && gNumCones > 0) --gNumCones;
	//}
	if (RISING(BTN_MACRO_INC) && gNumCones < 11) ++gNumCones;
	if (RISING(BTN_MACRO_DEC) && gNumCones > 0) --gNumCones;
	if (RISING(BTN_MACRO_SCAN)) gNumCones = 0;
}


/* Autonomous */

#include "auto.h"
#include "auto_runs.h"

#include "auto.c"
#include "auto_runs.c"


/* LCD */

void handleLcd()
{
	string line;

	sprintf(line, "%4d %4d %2d", gSensor[driveEncL].value, gSensor[driveEncR].value, gNumCones);
	clearLCDLine(0);
	displayLCDString(0, 0, line);

	velocityCheck(driveEncL);
	velocityCheck(driveEncR);

	sprintf(line, "%2.1f %2.1f", gSensor[driveEncL].velocity, gSensor[driveEncR].velocity);
	clearLCDLine(1);
	displayLCDString(1, 0, line);
}

// This function gets called 2 seconds after power on of the cortex and is the first bit of code that is run
void startup()
{
	clearDebugStream();
	writeDebugStreamLine("Code start");

	// Setup and initilize the necessary libraries
	setupMotors();
	setupSensors();
	setupJoysticks();
	tInit();

	setupDgtIn(leftLine, 0, 150);
	setupDgtIn(rightLine, 0, 150);
	setupDgtIn(armSonic, 20, 200);

	velocityClear(driveEncL);
	velocityClear(driveEncR);

	gJoy[JOY_TURN].deadzone = DZ_TURN;
	gJoy[JOY_THROTTLE].deadzone = DZ_THROTTLE;
	gJoy[JOY_LIFT].deadzone = DZ_LIFT;
	gJoy[JOY_ARM].deadzone = DZ_ARM;

	pidInit(gArmPID, 0.2, 0.001, 0.0, 50, 150, 5, 127);
}

// This function gets called every 25ms during disabled (DO NOT PUT BLOCKING CODE IN HERE)
void disabled()
{
	handleLcd();
}

// This task gets started at the begining of the autonomous period
task autonomous()
{
	gAutoTime = nPgmTime;
	writeDebugStreamLine("Auto start");

	startSensors(); // Initilize the sensors

	gKillDriveOnTimeout = true;

	resetPosition(gPosition);
	resetQuadratureEncoder(driveEncL);
	resetQuadratureEncoder(driveEncR);
	resetQuadratureEncoder(latEnc);

	tStart(autoMotorSensorUpdateTask);
	tStart(trackPositionTask);

	runAuto();

	writeDebugStreamLine("Auto: %d ms", nPgmTime - gAutoTime);

	return_t;
}

// This task gets started at the beginning of the usercontrol period
task usercontrol()
{
	startSensors(); // Initilize the sensors
	initCycle(gMainCycle, 10);

	gKillDriveOnTimeout = false;

	while (true)
	{
		updateSensorInputs();
		updateJoysticks();

		handleDrive();
		handleLift();
		handleArm();
		handleClaw();
		handleMobile();
		handleMacros();

		handleLcd();

		updateSensorOutputs();
		updateMotors();
		endCycle(gMainCycle);
	}

	return_t;
}
