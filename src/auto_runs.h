/* Enumerations */
typedef enum _tAlliance
{
	allianceBlue,
	allianceRed
} tAlliance;

/* Functions */
void selectAuto();
void runAuto();
void autoSkills(int segment = -1);
void autoBlock();
void auto20Left();
void auto20Right();
void autoStationaryLeftBlock();
void autoStationaryRightBlock();
void autoStationaryLeft5();
void autoStationaryRight5();
void autoStationaryLeft2();
void autoStationaryRight2();

void autoTest();

/* Variables */
int gCurAuto = 0;
tAlliance gAlliance = allianceBlue;
