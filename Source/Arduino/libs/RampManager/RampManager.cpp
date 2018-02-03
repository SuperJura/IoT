#include <Arduino.h>
#include <RampManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <Hash.h>

//Motor pins
#define motor_enable 4
#define motor_IN1 12
#define motor_IN2 14

//US pins
#define us_down 15
#define us_up 0
#define us_car_in 16
#define us_car_out 2

#define USE_SERIAL Serial

//RAMP VARIABLES
const int MOTOR_UNKNOWN = 0;
const int MOTOR_DOWN = 1;
const int MOTOR_MOVING_UP = 2;
const int MOTOR_UP = 3;
const int MOTOR_MOVING_DOWN = 4;

int motorState = MOTOR_UNKNOWN;
int long moveButtonTimestamp = -1;

const int CAR_MOVEMENT_NO = 0;
const int CAR_MOVEMENT_IN = 1;
const int CAR_MOVEMENT_OUT = 2;

int currentCarMovement = CAR_MOVEMENT_NO;
int sensorsCarPassed = 0;

const int NUM_OF_PARKINGS = 4;
int numOfCarsInParkings = 0;

int long changeTimestamp = -1;
int long rampUpTimestamp = -1;

//WEBSOCKET VARIABLES
ESP8266WiFiMulti WiFiMulti;

// WebSocketServer works on port 81
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer webServer(80);

RampManager::RampManager()
{

}

void RampManager::Setup()
{
	USE_SERIAL.begin(115200);
	setupRamp();
	setupWebSockets();
	setupWebServer();
}

void RampManager::Loop()
{
	rampLoop();
	webSocket.loop();
	webServer.handleClient();
}

void RampManager::setupRamp()
{
	USE_SERIAL.println("Starting ramp init");
	pinMode(motor_enable, OUTPUT);
	pinMode(motor_IN1, OUTPUT);
	pinMode(motor_IN2, OUTPUT);

	pinMode(us_down, INPUT);
	pinMode(us_up, INPUT);
	pinMode(us_car_in, INPUT);
	pinMode(us_car_out, INPUT);

	USE_SERIAL.println("Finished ramp init");
}

void RampManager::setupWebSockets()
{

	USE_SERIAL.println("Starting webSocket init");
	//Serial.setDebugOutput(true);
	USE_SERIAL.setDebugOutput(true);

	USE_SERIAL.println();
	USE_SERIAL.println();
	USE_SERIAL.println();

	for (uint8_t t = 4; t > 0; t--)
	{
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}

	//Ovdje upisati ime i pass wireless mreže
	//ISKON-HotSpot
	WiFiMulti.addAP("ISKON-HotSpot", "");

	while (WiFiMulti.run() != WL_CONNECTED) {
		delay(100);
	}

	//USE_SERIAL.println(WiFiMulti.localIP());

	webSocket.begin();
	webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length)
		{
			USE_SERIAL.printf("%u New package\n", type);
			switch (type) 
			{
				case WStype_DISCONNECTED:

					USE_SERIAL.printf("[%u] Disconnected!\n", num);
					break;
				case WStype_CONNECTED:
					{
						IPAddress ip = webSocket.remoteIP(num);
						USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

						// send message to client
						webSocket.sendTXT(num, "Connected");
					}
					break;
				case WStype_TEXT:

					USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
					String pl = (char*)payload;
					if (pl == "testApp:up")
					{
						startMotorButtonMove(true);
					}
					else if (pl == "testApp:down")
					{
						startMotorButtonMove(false);
					}
					else if (pl == "testApp:stop")
					{
						endMotorButtonMove();
					}
					else if (pl == "testApp:sensors")
					{
						String output = "";
						output = contatinatePinState(us_up, output);
						output = contatinatePinState(us_down, output);
						output = contatinatePinState(us_car_in, output);
						output = contatinatePinState(us_car_out, output);

						webSocket.sendTXT(num, output);
					}
					else if (pl == "RampApp:sensors")
					{
						String output = "";
						output = contatinatePinState(us_up, output);
						output = contatinatePinState(us_down, output);
						output = contatinatePinState(us_car_in, output);
						output = contatinatePinState(us_car_out, output);

						output += String(motorState);
						webSocket.sendTXT(num, output);
					}
					else if (pl == "RampApp:parkings")
					{
						String output = String(numOfCarsInParkings);
						webSocket.sendTXT(num, output);
					}
					break;
			}
		}
	);

	USE_SERIAL.println("Finished webSocket init");
}

void RampManager::setupWebServer()
{
	USE_SERIAL.println("Starting webServer init");
	SPIFFS.begin();
	webServer.on("/Test", []()
	{
		webServer.send(200, "text/plain", "Server is working");
	});
	webServer.on("/Home", []()
	{
		File homeFile = SPIFFS.open("/RampApp.html", "r");
		webServer.streamFile(homeFile, "text/html");
	});
	webServer.on("/", []()
	{
		File homeFile = SPIFFS.open("/RampApp.html", "r");
		webServer.streamFile(homeFile, "text/html");
	});
	webServer.on("/Debug", []()
	{
		File debugFile = SPIFFS.open("/TestApp.html", "r");
		webServer.streamFile(debugFile, "text/html");
	});
	webServer.begin();
	USE_SERIAL.println("Finished webServer init");
}

String RampManager::contatinatePinState(int pin, String baseString)
{
	if (digitalRead(pin) == HIGH)
	{
		baseString += "0";
	}
	else
	{
		baseString += "1";
	}

	return baseString;
}



void RampManager::rampLoop() {
	//ako je rampa negdje, a neznamo gdje
	if (digitalRead(us_up) == HIGH &&
		digitalRead(us_down) == HIGH &&
		motorState == MOTOR_UNKNOWN &&
		(moveButtonTimestamp == -1 || moveButtonTimestamp + 3000 < millis()))
	{
		motorState = MOTOR_MOVING_DOWN;
		changeTimestamp = millis() - 500;
		moveButtonTimestamp = -1;
	}

	//uvijek gledaj dali je dolje, ako je, nemoj spustati vise
	if (digitalRead(us_down) == LOW)
	{
		if (changeTimestamp != -1 && changeTimestamp + 500 < millis())
		{
			motorState = MOTOR_DOWN;
			changeTimestamp = -1;
		}
	}

	//uvijek gledaj dali je gore, ako je, nemoj dizati vise
	if (digitalRead(us_up) == LOW)
	{
		if (changeTimestamp != -1 && changeTimestamp + 500 < millis())
		{
			motorState = MOTOR_UP;
			changeTimestamp = -1;
		}
	}

	//logika za pomicanje motora
	switch (motorState)
	{
	case MOTOR_MOVING_UP:
		moveMotor(true);
		break;
	case MOTOR_MOVING_DOWN:
		moveMotor(false);
		break;
	case MOTOR_UNKNOWN:
	case MOTOR_DOWN:
		stopMotor();
		break;
	case MOTOR_UP:
		stopMotor();
		if (rampUpTimestamp == -1 && changeTimestamp + 500 < millis())
		{
			rampUpTimestamp = millis();
		}
		break;
	}


	//logika za ulaz i izlaz motora
	if (moveButtonTimestamp == -1)
	{
		//auto JE kod senzora za ULAZ
		if (digitalRead(us_car_in) == LOW)
		{
			if (motorState != MOTOR_UP)
			{
				if (numOfCarsInParkings < NUM_OF_PARKINGS)
				{
					motorState = MOTOR_MOVING_UP;
					if (changeTimestamp == -1)
					{
						changeTimestamp = millis();
					}
					currentCarMovement = CAR_MOVEMENT_IN;
					sensorsCarPassed = 1;
				}
			}
			else
			{
				if (currentCarMovement == CAR_MOVEMENT_OUT && sensorsCarPassed == 1)
				{
					sensorsCarPassed = 2;
				}
			}
		}
		//auto NIJE kod senzora za ULAZ
		else
		{
			if (currentCarMovement == CAR_MOVEMENT_OUT && sensorsCarPassed == 2)
			{
				carPassedTrough();
				numOfCarsInParkings -= 1;
			}
		}

		//auto JE kod senzora za IZLAZ
		if (digitalRead(us_car_out) == LOW)
		{
			if (motorState != MOTOR_UP)
			{
				motorState = MOTOR_MOVING_UP;
				if (changeTimestamp == -1)
				{
					changeTimestamp = millis();
				}
				currentCarMovement = CAR_MOVEMENT_OUT;
				sensorsCarPassed = 1;
			}
			else
			{
				if (currentCarMovement == CAR_MOVEMENT_IN && sensorsCarPassed == 1)
				{
					sensorsCarPassed = 2;
				}
			}
		}

		//auto NIJE kod senzora za IZLAZ
		else
		{
			if (currentCarMovement == CAR_MOVEMENT_IN && sensorsCarPassed == 2)
			{
				carPassedTrough();
				numOfCarsInParkings += 1;
			}
		}
	}

	//ako je rampa gore 3 sekunde i nema nikoga, resetiraj ju
	if (digitalRead(us_car_out) == HIGH &&
		digitalRead(us_car_in) == HIGH &&
		digitalRead(us_up) == LOW)
	{
		if (rampUpTimestamp != -1 && rampUpTimestamp + 3000 < millis())
		{
			resetRampWhenUp();
		}
	}
}

void RampManager::carPassedTrough()
{
	currentCarMovement = CAR_MOVEMENT_NO;
	sensorsCarPassed = 0;
	motorState = MOTOR_MOVING_DOWN;
	if (changeTimestamp == -1)
	{
		changeTimestamp = millis();
	}
}

void RampManager::resetRampWhenUp()
{
	rampUpTimestamp = -1;
	motorState = MOTOR_MOVING_DOWN;
	changeTimestamp = millis();
	sensorsCarPassed = 0;
	currentCarMovement = CAR_MOVEMENT_NO;
}

void RampManager::moveMotor(bool up)
{
	if (up)
	{
		digitalWrite(motor_enable, HIGH);
		digitalWrite(motor_IN1, LOW);
		digitalWrite(motor_IN2, HIGH);
	}
	else
	{
		digitalWrite(motor_enable, HIGH);
		digitalWrite(motor_IN1, HIGH);
		digitalWrite(motor_IN2, LOW);
	}
}

void RampManager::stopMotor()
{
	digitalWrite(motor_enable, HIGH);
	digitalWrite(motor_IN1, LOW);
	digitalWrite(motor_IN2, LOW);
}


//API
void RampManager::startMotorButtonMove(bool up)
{
	if (up)
	{
		if (digitalRead(us_up) == HIGH)
		{
			motorState = MOTOR_MOVING_UP;
			if (changeTimestamp == -1)
			{
				changeTimestamp = millis();
			}
		}
	}
	else
	{
		if (digitalRead(us_down) == HIGH)
		{
			motorState = MOTOR_MOVING_DOWN;
			if (changeTimestamp == -1)
			{
				changeTimestamp = millis();
			}
		}
	}
}

void RampManager::endMotorButtonMove()
{
	if (digitalRead(us_down) == LOW)
	{
		motorState = MOTOR_DOWN;
		moveButtonTimestamp = -1;
	}
	else
	{
		motorState = MOTOR_UNKNOWN;
		moveButtonTimestamp = millis();
	}
}