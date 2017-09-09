/* Defines */
#define MAP_DRIVE_INPUT(value) ((sbyte) (10.0 * pow(1.02, (float) abs(value))) * sgn(value))

/* Types */
typedef enum _tDriveStates
{
	driveIdle,
	driveManual
} tDriveStates;

/* Functions */
void setDrive(sbyte left, sbyte right);
void handleDrive();

/* State Machine */
MAKE_MACHINE(drive, tDriveStates, driveManual,
{
	case driveIdle: setDrive(0, 0); break;
})