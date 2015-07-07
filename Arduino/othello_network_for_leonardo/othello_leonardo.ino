/*
	Template5.ino
	Created by Young-Bae Suh, 2015-06-02.
	Find details at http://www.HardCopyWorld.com/gamemaker.html
*/
#define USE_SERIAL1
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
int gameFPS = 1000/4;
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
#define STATUS_PAUSE 4
#define STATUS_RESULT 5

int gameState = STATUS_MENU;    // Pause or not
boolean gameEnd = false;
boolean isWinner = false;
int gameScore = 0;
int gameHighScore = 0;

// Input
#define BUTTON_A 5
#define BUTTON_B 6
InputController inputController;
bool up, down, left, right, aBut, bBut;
#define INPUT_CHECK_INTERVAL 10
unsigned long inputCheckTime = 0;

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
boolean isHost = true;

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
int cursorX = 0;		// Cursor x position
int cursorY = 0;
int lastPosX = 0;		// Selected position
int lastPosY = 0;
int recvPosX = 0;	// Opponent's position
int recvPosY = 0;

#define BOARD_SIZE_X 8
#define BOARD_SIZE_Y 8
#define MY_STONE 1
#define OPNT_STONE 2
byte matrix[BOARD_SIZE_X][BOARD_SIZE_Y];
int myStoneCount = 0;
int opntStoneCount = 0;

// Utilities
int getOffset(int s);
void initUserInput();
void stopUntilUserInput();
void setMenuMode();
void setHostMode();
void setClientMode();
void startGame();
void setPauseMode();
void setPlayingMode();
void setResultMode();
void checkInput();
void sendMessage(int message_type);
boolean parseMessage();
void draw();
void initGameParameters();
void changeStone(boolean isMyStone);


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
//Serial.begin(9600);
	
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
	delay(1000);
	
	// initialize game engine
	initGameParameters();
	
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
	if(millis() > inputCheckTime + INPUT_CHECK_INTERVAL) {
		uint8_t input = inputController.getInput();
		if (input & (1<<5)) left = true;
		if (input & (1<<4)) up = true;
		if (input & (1<<3)) right = true;
		if (input & (1<<2)) down = true;
		if (input & (1<<1)) aBut = true;	// a button
		if (input & (1<<0)) bBut = true;	// b button	
		inputCheckTime = millis();
	}

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
			isHost = false;
			if(isMsgReceived) {
				display.clearDisplay();
				display.setCursor(10, 10);
				display.print(F("> Host activated game!!"));
				display.display();
				
				commIndex++;	// next communication index
				startGame();
			} else if(millis() > resendTime + SENDING_INTERVAL) {
				// Send connection start signal
				sendMessage(MSG_START_REQUEST);
				commIndex = 1;    // set communication index of response
			}
			isMsgReceived = false;
		}
		else if (gameState == STATUS_HOST) {  // Wait for start message from client
			isHost = true;
			if(isMsgReceived) {
				display.clearDisplay();
				display.setCursor(10, 10);
				display.print(F("> Received start message!!"));
				display.display();

				// Send noti to client
				sendMessage(MSG_START_RESPONSE);
				commIndex = 2;    // set communication index of response
				startGame();
			}
			isMsgReceived = false;
		}
		else if (gameState == STATUS_PAUSE) {  // Wait for a message from client
			// Run game engine
			if(isMsgReceived) {
				matrix[cursorX][cursorY] = OPNT_STONE;
				changeStone(false);
				setPlayingMode();
				isMsgReceived = false;
				commIndex++;	// next communication index
			}
			
			// Draw game screen
			draw();
			
			// Exit condition
			
			// Send message again
			if(millis() > resendTime + SENDING_INTERVAL) {
				sendMessage(MSG_SEND_AGAIN);
			}
		}
		else if (gameState == STATUS_PLAYING) { // If the game is playing
			// Check input
			checkInput();
			
			// Run game engine
			if(aBut || bBut) {
				if(matrix[cursorX][cursorY] != MY_STONE
					&& matrix[cursorX][cursorY] != OPNT_STONE) {
					matrix[cursorX][cursorY] = MY_STONE;
					changeStone(true);
					sendMessage(MSG_UPDATE);
					commIndex++;	// set next index which the response message must have
					setPauseMode();
				}
			}
			
			// Draw game screen
			draw();
			
			// Exit condition
			if(opntStoneCount < 1) { isWinner = true; gameEnd = true; }
			if(myStoneCount < 1) { isWinner = false; gameEnd = true; }
			if(opntStoneCount + myStoneCount >= STONE_WIDTH*STONE_HEIGHT) {
				if(opntStoneCount > myStoneCount) { isWinner = false; gameEnd = true; }
				else { isWinner = true; gameEnd = true; }
			}
			if(gameEnd) {
				setResultMode();
			}
			
			// Send message again
			if(millis() > resendTime + SENDING_INTERVAL) {
				sendMessage(MSG_SEND_AGAIN);
			}
		}
		else if (gameState == STATUS_RESULT) {  // Draw a Game Over screen w/ score
			gameScore += myStoneCount;
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
		
			display.setCursor(22,42);
			if(isWinner) display.print(F("You win!!"));
			else display.print(F("You lose!!"));

			display.display();

			// Send message again
			sendMessage(MSG_SEND_AGAIN);
			
			stopUntilUserInput();    // Wait until user touch the button
			gameState = STATUS_MENU;  // Then start the game paused
			gameScore = 0;  // Reset score to 0
			setMenuMode();
			
			// Send message again
			sendMessage(MSG_SEND_AGAIN);
		}
		
		// Initialize user input
		initUserInput();
	}
}

/////////////////////////////////////////////////////////////////
//  Game engine

void initGameParameters() {
	int stoneColor, opntColor;
	if(isHost) { stoneColor = MY_STONE; opntColor = OPNT_STONE; }
	else { stoneColor = OPNT_STONE; opntColor = MY_STONE; }
	
	// initialize game board with initial 4 stone at center
	for(int i=0; i<BOARD_SIZE_X; i++) {
		for(int j=0; j<BOARD_SIZE_Y; j++) {
			if(i==(BOARD_SIZE_X/2 - 1) && j==(BOARD_SIZE_Y/2 - 1)) {
				matrix[i][j] = stoneColor;
			}
			else if(i==(BOARD_SIZE_X/2) && j==(BOARD_SIZE_Y/2 - 1)) {
				matrix[i][j] = opntColor;
			}
			else if(i==(BOARD_SIZE_X/2 - 1) && j==(BOARD_SIZE_Y/2)) {
				matrix[i][j] = opntColor;
			}
			else if(i==(BOARD_SIZE_X/2) && j==(BOARD_SIZE_Y/2)) {
				matrix[i][j] = stoneColor;
			}
			else {
				matrix[i][j] = 0;
			}
		}
	}
	cursorX = 0;
	cursorY = 0;
	myStoneCount = 0;
	opntStoneCount = 0;
}

void checkInput() {
	if(up) cursorY--;
	else if(down) cursorY++;
	if(left) cursorX--;
	else if(right) cursorX++;
	
	if(cursorX < 0) cursorX = 0;
	else if(cursorX >= BOARD_SIZE_X) cursorX = BOARD_SIZE_X - 1;
	if(cursorY < 0) cursorY = 0;
	else if(cursorY >= BOARD_SIZE_Y) cursorY = BOARD_SIZE_Y - 1;
}

void changeStone(boolean isMyStone) {
	int stoneColor, opntColor;
	if(isMyStone) { stoneColor = MY_STONE; opntColor = OPNT_STONE; }
	else { stoneColor = OPNT_STONE; opntColor = MY_STONE; }
	
	// change horizonal stones
	for(int i=cursorX-1; i>=0; i--) {
		if(matrix[i][cursorY] == stoneColor) {
			for(int j=i; j<cursorX; j++) {
				if(matrix[j][cursorY] == opntColor)
					matrix[j][cursorY] = stoneColor;
			}
			break;
		}
		else if(matrix[i][cursorY] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	for(int i=cursorX+1; i<BOARD_SIZE_X; i++) {
		if(matrix[i][cursorY] == stoneColor) {
			for(int j=i; j>cursorX; j--) {
				if(matrix[j][cursorY] == opntColor)
					matrix[j][cursorY] = stoneColor;
			}
			break;
		}
		else if(matrix[i][cursorY] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	// change vertical stones
	for(int i=cursorY-1; i>=0; i--) {
		if(matrix[cursorX][i] == stoneColor) {
			for(int j=i; j<cursorY; j++) {
				if(matrix[cursorX][j] == opntColor)
					matrix[cursorX][j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX][i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	for(int i=cursorY+1; i<BOARD_SIZE_Y; i++) {
		if(matrix[cursorX][i] == stoneColor) {
			for(int j=i; j>cursorY; j--) {
				if(matrix[cursorX][j] == opntColor)
					matrix[cursorX][j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX][i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	// change diagonal stones (from left-top)
	int temp_start = 0;
	int temp_end = 0;
	if(cursorX <= cursorY) {
		temp_start = cursorX;
		temp_end = BOARD_SIZE_Y - 1 - cursorY;
	} else {
		temp_start = cursorY;
		temp_end = BOARD_SIZE_X - 1 - cursorX;
	}
	for(int i=1; i<=temp_start; i++) {
		if(matrix[cursorX-i][cursorY-i] == stoneColor) {
			for(int j=i; j>0; j--) {
				if(matrix[cursorX-j][cursorY-j] == opntColor)
					matrix[cursorX-j][cursorY-j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX-i][cursorY-i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	for(int i=1; i<=temp_end; i++) {
		if(matrix[cursorX+i][cursorY+i] == stoneColor) {
			for(int j=i; j>0; j--) {
				if(matrix[cursorX+j][cursorY+j] == opntColor)
					matrix[cursorX+j][cursorY+j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX+i][cursorY+i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	// change diagonal stones (from right-top)
	if(BOARD_SIZE_X - 1 - cursorX <= cursorY) {
		temp_start = BOARD_SIZE_X - 1 - cursorX;
		temp_end = BOARD_SIZE_Y - 1 - cursorY;
	} else {
		temp_start = cursorY;
		temp_end = cursorX;
	}
	for(int i=1; i<=temp_start; i++) {
		if(matrix[cursorX+i][cursorY-i] == stoneColor) {
			for(int j=i; j>0; j--) {
				if(matrix[cursorX+j][cursorY-j] == opntColor)
					matrix[cursorX+j][cursorY-j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX+i][cursorY-i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
	for(int i=1; i<=temp_end; i++) {
		if(matrix[cursorX-i][cursorY+i] == stoneColor) {
			for(int j=i; j>0; j--) {
				if(matrix[cursorX-j][cursorY+j] == opntColor)
					matrix[cursorX-j][cursorY+j] = stoneColor;
			}
			break;
		}
		else if(matrix[cursorX-i][cursorY+i] != opntColor) {
			// it's blank, stop loop.
			break;
		}
	}
}

void draw() {
	// draw background
	if(true/*drawBg*/) {	// always draw all
		display.clearDisplay();
		display.drawRect(61, 0, 2, 64, WHITE);
		//drawBg = false;
	}
	
	// init params
	myStoneCount = 0;
	opntStoneCount = 0;
	
	// draw stones
	display.fillRect(64, 1, 63, 64, BLACK);
	for(int i=0; i<BOARD_SIZE_X; i++) {
		for(int j=0; j<BOARD_SIZE_Y; j++) {
			if(matrix[i][j] == MY_STONE) {
				myStoneCount++;
				display.drawBitmap(STONE_START_X+i*STONE_WIDTH,STONE_START_Y+j*STONE_HEIGHT,(const unsigned char*)pgm_read_word(&(stone_image[0])),STONE_WIDTH,STONE_HEIGHT,1);
			} else if(matrix[i][j] == OPNT_STONE) {
				opntStoneCount++;
				display.drawBitmap(STONE_START_X+i*STONE_WIDTH,STONE_START_Y+j*STONE_HEIGHT,(const unsigned char*)pgm_read_word(&(stone_image[1])),STONE_WIDTH,STONE_HEIGHT,1);
			}
		}
	}
	// draw cursor
	display.drawRect(STONE_START_X+cursorX*STONE_WIDTH, 
					STONE_START_Y+cursorY*STONE_HEIGHT, 
					STONE_WIDTH, 
					STONE_HEIGHT, 
					WHITE);
	
	// Draw score
	display.setCursor(2, 2);
	display.print(F("My: "));
	display.print(myStoneCount);
	display.setCursor(2, 14);
	display.print(F("Opp: "));
	display.print(opntStoneCount);
	display.setCursor(2, 30);
	if(gameState == STATUS_PLAYING)
		display.print(F("Your turn"));
	else if(gameState == STATUS_PAUSE)
		display.print(F("Wait!!"));
	
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
	delay(400);
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

void startGame() {
	drawBg = true;
	gameScore = 0;
	gameEnd = false;
	isWinner = false;
	initGameParameters();
	if(isHost)
		gameState = STATUS_PAUSE;
	else
		gameState = STATUS_PLAYING;
}

void setPlayingMode() {
	gameState = STATUS_PLAYING;
}

void setPauseMode() {
	gameState = STATUS_PAUSE;
}

void setResultMode() {
	gameState = STATUS_RESULT;
	delay(2000);
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
		lastPosX = 0;
		lastPosY = 0;
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
		lastPosX = 0;
		lastPosY = 0;
	}
	else if(message_type == MSG_UPDATE) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)(commIndex >> 8));		// communication index
		hcSerial.write((byte)commIndex);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)cursorX);		// position x
		hcSerial.write((byte)cursorY);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
		lastSentMsg = message_type;
		lastCommIndex = commIndex;
		lastPosX = cursorX;
		lastPosY = cursorY;
	}
	else if(message_type == MSG_SEND_AGAIN) {
		hcSerial.write(START_BYTE);		// start byte
		hcSerial.write((byte)(lastCommIndex >> 8));		// communication index
		hcSerial.write((byte)lastCommIndex);
		hcSerial.write((byte)0x00);		// temp
		hcSerial.write((byte)0x00);
		hcSerial.write((byte)lastPosX);		// position x
		hcSerial.write((byte)lastPosY);		// position y
		hcSerial.write(END_BYTE);	// end byte
		resendTime = millis();
	}
}

boolean parseMessage() {
//Serial.println("Message received!!");
	boolean isUpdated = false;
	if(Buffer[0] == START_BYTE && Buffer[BUFFER_SIZE - 1] == END_BYTE) {
		int recv_index = 0;
		recv_index = (Buffer[1] << 8) | Buffer[2];
//Serial.print("expected index = ");
//Serial.print(commIndex);
//Serial.print(", received index = ");
//Serial.println(recv_index);
		if(commIndex == recv_index) {
			recvPosX = (int)(Buffer[5]);
			cursorX = recvPosX;		// To change stone, reset cursor position.
			recvPosY = (int)(Buffer[6]);
			cursorY = recvPosY;
			isUpdated = true;
		}
	}
	return isUpdated;
}

