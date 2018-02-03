#ifndef RampManager_h
#define RampManager_h

#include "Arduino.h"
#include <WebSocketsServer.h>
class RampManager
{
public:
	RampManager();
	void Setup();
	void Loop();
private:
	void setupRamp();
	void setupWebSockets();
	void setupWebServer();
	String contatinatePinState(int pin, String baseString);
	void rampLoop();
	void carPassedTrough();
	void resetRampWhenUp();
	void moveMotor(bool up);
	void stopMotor();
	void startMotorButtonMove(bool up);
	void endMotorButtonMove();
};

#endif
