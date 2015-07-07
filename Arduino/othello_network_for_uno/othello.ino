/*
	Template5.ino
	Created by Young-Bae Suh, 2015-06-02.
	Find details at http://www.HardCopyWorld.com/gamemaker.html
*/

#define SSD1306_I2C

#include <SPI.h>
#include <Wire.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "bitmaps.h"
#include "InputController.h"

#ifndef USE_SERIAL1
  #include <SoftwareSerial.h>
  SoftwareSerial hcSerial(2, 3); //Connect HC-11's TX, RX
#else
  // In this case we use hardware serial, Serial1.
  #define hcSerial Serial1
#endif

#ifndef SSD1306_I2C
  #define CLK	13
  #define MOSI	11
  #define CS	6
  #define DC	4
  #define RST	12
  Adafruit_SSD1306 display(MOSI, CLK, DC, RST, CS);
#else
  #define OLED_RESET 8
  Adafruit_SSD1306 display(OLED_RESET);
#endif

// System
int eepromAddr = 0;

// Display variables
int HEIGHT = 64;
int WIDTH = 128;
int gameFPS = 1000/10;
boolean drawBg = false;

// Time variables
unsigned long lTime;
unsigned long resendTime = 0;
#define SENDING_INTERVAL 3000

// Game status
#define STATUS_MENU 0
#define STATUS_HOST 1
#define STATUS_CLIENT 2
#define STATUS_PLAYING 3
#define STATUS_COMM 4
#define STATUS_RESULT 5

int gameState = STATUS_MENU;    // Pause or not
boolean gameEnd = false;
int gameScore = 0;
int gameHighScore = 0;

// Input
#define BUTTON_A 5
#define BUTTON_B 6
InputController inputController;
bool up, down, left, right, aBut, bBut;

// Menu
#define MENU_HOST 1
#define MENU_CLIENT 2
#define MENU_MIN 1
#define MENU_MAX 2
int prevMenu = MENU_MAX;
int currentMenu = 0;

PROGMEM const char* stringTable[] = {
  "Start as host",
  "Start as client"
};

// Communication
#define MSG_START_REQUEST 0
#define MSG_START_RESPONSE 1
#define MSG_UPDATE 5
#define MSG_SEND_AGAIN 10
int commIndex = 0;

#define START_BYTE 0xFF
#define END_BYTE 0xFE

#define BUFFER_SIZE 8
static byte Buffer[BUFFER_SIZE] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};


//////////////////////////////////////////////////
// Game engine parameters
//////////////////////////////////////////////////

// Image parameters
#define STONE_WIDTH 8
#define STONE_HEIGHT 8
#define STONE_START_X 64
#define STONE_START_Y 0

// Game logic
int myPosX = 0;
int myPosY = 0;
int recvPosX = 0;
int recvPosY = 0;


// Utilities
int getOffset(int s);
void initUserInput();
void stopUntilUserInput();
void setMenuMode();
void setHostMode();
void setClientMode();
void setGameMode();
void setResultMode();
void checkInput();
void sendMessage(int message_type);
boolean parseMessage();
void draw();
unsigned long getRandTime();



void setup() {
	// Initialize display
#ifndef SSD1306_I2C
	SPI.begin();
	display.begin(SSD1306_SWITCHCAPVCC);
#else
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
#endif
	display.setTextSize(1);
	display.setTextColor(WHITE);
	//digitalWrite( 17, LOW);    // what is this??

	// set button pins as INPUT_PULLUP
	pinMode(BUTTON_A,INPUT_PULLUP);
	pinMode(BUTTON_B,INPUT_PULLUP);
	
	// prepare serial communication
	hcSerial.begin(9600);
	
	// Read high score from eeprom
	while (!eeprom_is_ready()); // Wait for EEPROM to be ready
	cli();
	gameHighScore = eeprom_read_word((uint16_t*)eepromAddr);
	sei();
	if(gameHighScore < 0 || gameHighScore > 65500) gameHighScore = 0;
	
	// display startup animation
	for(int i=-8; i<28; i=i+2) {
		display.clearDisplay();
		display.drawBitmap(46,i, arduino, 32,8,1);
		display.display();
	}
	delay(2000);
	
	// display main image
	display.clearDisplay();
	display.drawBitmap(0,0,othello,128,64,1);
	display.display();
	
	stopUntilUserInput();    // Wait until user touch the button

	setMenuMode();  // Menu mode
}

boolean isMsgReceived = false;
void loop() {
	// Get input status
	uint8_t input = inputController.getInput();
	if (input & (1<<5)) left = true;
	if (input & (1<<4)) up = true;
	if (input & (1<<3)) right = true;
	if (input & (1<<2)) down = true;
	if (input & (1<<1)) aBut = true;	// a button
	if (input & (1<<0)) bBut = true;	// b button

	// Read message from HC-11
	if (hcSerial.available() > 0)  {
		byte in_byte = (byte)(hcSerial.read());
		
		// Add received byte to buffer
		for(int i=0; i<BUFFER_SIZE-1; i++) {
			Buffer[i] = Buffer[i+1];
		}
		Buffer[BUFFER_SIZE-1] = in_byte;
		
		isMsgReceived = parseMessage();
	}
	
	if (millis() > lTime + gameFPS) {
		lTime = millis();

		if (gameState == STATUS_MENU) {  // Main menu
			if(up) currentMenu--;
			else if(down) currentMenu++;
			if(currentMenu > MENU_MAX) currentMenu = MENU_MAX;
			if(currentMenu < MENU_MIN) currentMenu = MENU_MIN;
			
			// User selected one of menu
			if(aBut || bBut) {
  				if(currentMenu == MENU_HOST) {
					setHostMode();
				}
				else if(currentMenu == MENU_CLIENT) {
					setClientMode();
				}
			} 
			// Menu navigation
			else {
				if(prevMenu != currentMenu) {
					prevMenu = currentMenu;		// Remember current menu index
					
					// Draw menu
					display.clearDisplay();
					for(int i=0; i<MENU_MAX; i++) {
						display.setCursor(24, 10+i*12);
						display.print((const char*)pgm_read_word(&(stringTable[i])));
					}
					display.setCursor(10, 10 + (currentMenu - 1)*12);
					display.print("> ");
					display.display();
				}
			}
		}
		else if (gameState == STATUS_CLIENT) {	// Wait for host response
			if(isMsgReceived) {
				display.clearDisplay();
				display.setCursor(10, 10);
				display.print(F("> Host activated game!!"));
				display.display();
				
				setGameMode();
			} else if(millis() > resendTime + SENDING_INTERVAL) {
				// Send connection start signal
				sendMessage(MSG_START_REQUEST);
				commIndex = 1;    // set communication index of response
			}
			isMsgReceived = false;
		}
		else if (gameState == STATUS_HOST) {  // Wait for start message from client
			if(isMsgReceived) {
				display.clearDisplay();
				display.setCursor(10, 10);
				display.print(F("> Received start message!!"));
				display.display();
				
				// Send noti to client
				sendMessage(MSG_START_RESPONSE);
				commIndex++;    // set communication index of response
				setGameMode();
			}
			isMsgReceived = false;
		}
		else if (gameState == STATUS_PLAYING) { // If the game is playing
			// Run game engine
			checkInput();
			
			// Draw game screen
			draw();
			
			// Exit condition
			
			// Send message again
			if(millis() > resendTime + SENDING_INTERVAL) {
				// Send connection start signal
				sendMessage(MSG_SEND_AGAIN);
				commIndex = 1;    // set communication index of response
			}
		}
		else if (gameState == STATUS_RESULT) {  // Draw a Game Over screen w/ score
			gameScore += 5;
			if (gameScore > gameHighScore) { 
				gameHighScore = gameScore; 
				cli();
				eeprom_write_word((uint16_t*)eepromAddr, gameHighScore);
				sei();
			}  // Update game score
			display.display();  // Make sure final frame is drawn

			// delay(100); // Pause for the sound
			
			// Draw game over screen
			display.drawRect(16,8,96,48, WHITE);  // Box border
			display.fillRect(17,9,94,46, BLACK);  // Black out the inside
			display.drawBitmap(30,12,gameover,72,14,1);
			
			display.setCursor(56 - getOffset(gameScore),30);
			display.print(gameScore);
			display.setCursor(69,30);
			display.print(F("Score"));
		
			display.setCursor(56 - getOffset(gameHighScore),42);
			display.print(gameHighScore);
			display.setCursor(69,42);
			display.print(F("High"));

			display.display();

			stopUntilUserInput();    // Wait until user touch the button
			gameState = STATUS_MENU;  // Then start the game paused
			gameScore = 0;  // Reset score to 0
			setMenuMode();
		}
		
		// Initialize user input
		initUserInput();
	}
}

/////////////////////////////////////////////////////////////////
//  Game engine

void checkInput() {
	if(up || down || left || right) {
		
	}
	else if(bBut) {
		
	}
}

void draw() {
	// draw background
	if(drawBg) {
		display.clearDisplay();
		display.drawLine(0, 63, 127, 63, WHITE);
		drawBg = false;
	}
	
	// draw stones
	//display.drawRect(16,8,96,48, WHITE);
	//display.fillRect(CHAR_POS_X, prevPosY, CHAR_WIDTH, CHAR_HEIGHT, BLACK);
	//display.drawLine(prevBulletPosX[i], BULLET_POS_Y, bulletX[i], BULLET_POS_Y, BLACK);
	//display.drawBitmap(CHAR_POS_X, prevPosY, (const unsigned char*)pgm_read_word(&(char_anim[charAniIndex])), CHAR_WIDTH, CHAR_HEIGHT, WHITE);
	
	// Draw score

	
	// Show on screen
	display.display();
}


/////////////////////////////////////////////////////////////////
//  Utilities

int getOffset(int s) {
	if (s > 9999) { return 20; }
	if (s > 999) { return 15; }
	if (s > 99) { return 10; }
	if (s > 9) { return 5; }
	return 0;
}

void initUserInput() {
	left = false;
	up = false;
	right = false;
	down = false;
	aBut = false;	// a button
	bBut = false;	// b button
}

void stopUntilUserInput() {
	while (true) {  // While we wait for input
		if (digitalRead(BUTTON_A) == LOW || digitalRead(BUTTON_B) == LOW) {  // Wait for a button press
			delay(200);
			break;
		}
		delay(200);  // Slight delay the loop
	}
}

void setMenuMode() {
	gameState = STATUS_MENU;
	prevMenu = MENU_MAX;
	currentMenu = 0;
	initUserInput();
	delay(200);
}

void setHostMode() {
	// Show waiting message
	display.setCursor(10, 46);
	display.print(F(" Connecting..."));
	display.display();
	// Wait for start message
	gameState = STATUS_HOST;
	commIndex = 0;    // set communication index of response
	delay(200);
}

void setClientMode() {
	// Show waiting message
	display.setCursor(10, 46);
	display.print(F(" Connecting..."));
	display.display();
	
	// Send connection start signal
	sendMessage(MSG_START_REQUEST);
	commIndex = 1;    // set communication index of response
	// Wait for response
	gameState = STATUS_CLIENT;
	delay(200);
}

void setGameMode() {
	gameState = STATUS_PLAYING;
	drawBg = true;
	gameScore = 0;
	gameEnd = false;
}

void setResultMode() {
	gameState = STATUS_RESULT;
	delay(200);
}

unsigned long getRandTime() {
	return millis() + 100 * random(12, 30);
}

int lastSentMsg = 0;
int lastCommIndex = 0;
void sendMessage(int message_type) {
	if(message_type == MSG_START_REQUEST) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)0x00);		// communication index
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)0x00);		// position x
		hcSerial.write((byte)0x00);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
		lastSentMsg = message_type;
		lastCommIndex = commIndex;
	}
	else if(message_type == MSG_START_RESPONSE) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)0x00);		// communication index
		hcSerial.write((byte)0x01);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)0x00);		// position x
		hcSerial.write((byte)0x00);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
		lastSentMsg = message_type;
		lastCommIndex = commIndex;
	}
	else if(message_type == MSG_UPDATE) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)(commIndex >> 8));		// communication index
		hcSerial.write((byte)commIndex);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)myPosX);		// position x
		hcSerial.write((byte)myPosY);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
		lastSentMsg = message_type;
		lastCommIndex = commIndex;
	}
	else if(message_type == MSG_SEND_AGAIN) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)(lastCommIndex >> 8));		// communication index
		hcSerial.write((byte)lastCommIndex);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)myPosX);		// position x
		hcSerial.write((byte)myPosY);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
	}
}

boolean parseMessage() {
	boolean isUpdated = false;
	if(Buffer[0] == START_BYTE && Buffer[BUFFER_SIZE - 1] == END_BYTE) {
		int recv_index = 0;
		recv_index = (Buffer[1] << 8) | Buffer[2];
		if(commIndex == recv_index) {
			recvPosX = (int)(Buffer[5]);
			recvPosY = (int)(Buffer[6]);
			isUpdated = true;
		}
	}
	return isUpdated;
}

