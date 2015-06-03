/*
	Template3.ino
	Created by Young-Bae Suh, 2015-06-02.
	Find details at http://www.HardCopyWorld.com/gamemaker.html
*/

#define SSD1306_I2C

#include "bitmaps.h"
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "InputController.h"

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

// Display variables
int HEIGHT = 64;
int WIDTH = 128;
int gameFPS = 1000/6; 

// Time variables
unsigned long lTime;

// Game status
#define STATUS_MENU 0
#define STATUS_PLAYING 1
#define STATUS_PAUSED 2
#define STATUS_RESULT 3
#define STATUS_CREDIT 4

int gameState = STATUS_MENU;    // Pause or not
int gameScore = 0;
int gameHighScore = 0;

// Input
#define BUTTON_A 5
#define BUTTON_B 6
InputController inputController;
bool up, down, left, right, aBut, bBut;

// Menu
#define MENU_START 1
#define MENU_CREDIT 2
#define MENU_MIN 1
#define MENU_MAX 2
int prevMenu = MENU_MAX;
int currentMenu = 0;

PROGMEM const char* stringTable[] = {
  "Start game",
  "Credit"
};

//////////////////////////////////////////////////
// Game engine parameters
//////////////////////////////////////////////////

// Character parameters
#define CHAR_POS_X 20
#define CHAR_POS_Y 38

// Character status
#define CHAR_RUN 1
#define CHAR_JUMP 2
#define CHAR_FIRE 3
#define CHAR_DIE 4
int charStatus = CHAR_RUN;

// Run parameters
#define RUN_IMAGE_MAX 2
int charAniIndex = 1;
int charAniDir = 1;
boolean drawBg = true;

// Jump parameters
#define JUMP_MAX 20
#define JUMP_STEP 4
#define JUMP_IMAGE_INDEX 3
int charJumpIndex = 0;
int charJumpDir = JUMP_STEP;
int prevPosX = CHAR_POS_X;
int prevPosY = CHAR_POS_Y;

// Utilities
int getOffset(int s);
void initUserInput();
void stopUntilUserInput();
void setMenuMode();
void setGameMode();
void setResultMode();
void setCreditMode();
void checkInput();
void updateMove();
void checkCollision();
void draw();




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
	
	// display startup animation
	for(int i=-8; i<28; i=i+2) {
		display.clearDisplay();
		display.drawBitmap(46,i, arduino, 32,8,1);
		display.display();
	}
	delay(2000);
	
	// display main image
	display.clearDisplay();
	display.drawBitmap(0,0,nightrun,128,64,1);
	display.display();
	
	stopUntilUserInput();    // Wait until user touch the button

	setMenuMode();  // Menu mode
}


void loop() {
	uint8_t input = inputController.getInput();  // Get input status
	if (input & (1<<5)) left = true;
	if (input & (1<<4)) up = true;
	if (input & (1<<3)) right = true;
	if (input & (1<<2)) down = true;
	if (input & (1<<1)) aBut = true;	// a button
	if (input & (1<<0)) bBut = true;	// b button

	if (millis() > lTime + gameFPS) {
		lTime = millis();

		if (gameState == STATUS_MENU) {  // Main menu
			if(up) currentMenu--;
			else if(down) currentMenu++;
			if(currentMenu > MENU_MAX) currentMenu = MENU_MAX;
			if(currentMenu < MENU_MIN) currentMenu = MENU_MIN;
			
			// User selected one of menu
			if(aBut || bBut) {
  				if(currentMenu == MENU_CREDIT) {
					setCreditMode();
				}
				else if(currentMenu == MENU_START) {
					setGameMode();
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
		else if (gameState == STATUS_PAUSED) {  // If the game is paused
			stopUntilUserInput();    // Wait until user touch the button
		}
		else if (gameState == STATUS_PLAYING) { // If the game is playing
			// Run game engine
			checkInput();
			updateMove();
			checkCollision();
			
			// Draw game screen
			draw();
			
			// Exit condition
			if(bBut) {
				setResultMode();
			}
		}
		else if (gameState == STATUS_RESULT) {  // Draw a Game Over screen w/ score
			if (gameScore > gameHighScore) { gameHighScore = gameScore; }  // Update game score
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
		else if (gameState == STATUS_CREDIT) {  // Draw credit screen
			// Draw credit screen
			display.clearDisplay();
			display.drawBitmap(0,0,nightrun,128,64,1);

			display.setCursor(18,5);
			display.print(F("Presented by"));
			display.setCursor(18,20);
			display.print(F("HardCopyWorld.com"));

			display.display();  // Make sure final frame is drawn
			stopUntilUserInput();    // Wait until user touch the button
			setMenuMode();
		}
		
		// Initialize user input
		initUserInput();
	}
}

/////////////////////////////////////////////////////////////////
//  Game engine

void checkInput() {
	if(up || aBut) {
		if(charStatus == CHAR_RUN) {
			charStatus = CHAR_JUMP;
			charJumpIndex = 0;
			charJumpDir = JUMP_STEP;
			prevPosX = CHAR_POS_X; prevPosY = CHAR_POS_Y;
		} else if(charStatus == CHAR_JUMP) {
			// Do nothing
		}
	}
}

void updateMove() {
	if(charStatus == CHAR_JUMP) {
		charJumpIndex += charJumpDir;
		if(charJumpIndex >= JUMP_MAX) charJumpDir *= -1;
		// if jump ended
		if(charJumpIndex <= 0 && charJumpDir < 0) {
			charJumpDir = JUMP_STEP;
			charStatus = CHAR_RUN;
			display.fillRect(prevPosX, prevPosY, 24, 24, BLACK);	// delete previous character drawing
		}
	}
}

void checkCollision() {
	
}

void draw() {
	// draw background
	if(drawBg) {
		display.clearDisplay();
		display.drawLine(0, 63, 127, 63, WHITE);
		drawBg = false;
	}
	
	// draw char
	if(charStatus == CHAR_RUN) {
		charAniIndex += charAniDir;
		if(charAniIndex >= RUN_IMAGE_MAX || charAniIndex <= 0) charAniDir *= -1;
		display.fillRect(CHAR_POS_X, CHAR_POS_Y, 24, 24, BLACK);
		display.drawBitmap(CHAR_POS_X, CHAR_POS_Y, (const unsigned char*)pgm_read_word(&(char_anim[charAniIndex])), 24, 24, WHITE);
	} else if(charStatus == CHAR_JUMP) {
		display.fillRect(prevPosX, prevPosY, 24, 24, BLACK);
		prevPosY = CHAR_POS_Y-charJumpIndex;
		display.drawBitmap(prevPosX, prevPosY, (const unsigned char*)pgm_read_word(&(char_anim[JUMP_IMAGE_INDEX])), 24, 24, WHITE);
	}
	
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
			break;
		}
		delay(200);  // Slight delay the loop
	}
}

void setMenuMode() {
	gameState = STATUS_MENU;
	prevMenu = MENU_MAX;
	currentMenu = 0;
	delay(200);
}

void setGameMode() {
	gameState = STATUS_PLAYING;
	drawBg = true;
	charAniIndex = 0;
	charAniDir = 1;
	charStatus = CHAR_RUN;
	delay(300);
}

void setResultMode() {
	gameState = STATUS_RESULT;
	delay(200);
}

void setCreditMode() {
	gameState = STATUS_CREDIT;
	delay(200);
}
