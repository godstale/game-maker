/*
	Template.ino
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
int gameFPS = 1000/10; 

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
InputController inputController;
bool up, down, left, right, aBut, bBut;


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
	pinMode(6,INPUT_PULLUP);
	pinMode(5,INPUT_PULLUP);
	
	// display startup animation
	for(int i=-8; i<28; i=i+2) {
		display.clearDisplay();
		display.drawBitmap(46,i, arduino, 32,8,1);
		display.display();
	}
	delay(2000);
	
	// display logo
	display.clearDisplay();
	display.drawBitmap(0,0,nightrun,128,64,1);
	display.display();
	
	// Waiting for user input	
	bool waitingForInput = true;
	while (waitingForInput) {
		if (digitalRead(6)==0) {
			waitingForInput = false;
		}
		if (digitalRead(5)==0) {
			waitingForInput = false;
		}
	}

	delay(500);
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
			// TODO: 
		}
		else if (gameState == STATUS_PAUSED) {  // If the game is paused
			// TODO: 
		}
		else if (gameState == STATUS_PLAYING) { // If the game is playing
			// Calc game turn
			// Draw game screen
			
		}
		else if (gameState == STATUS_RESULT) {  // Draw a Game Over screen w/ score
			if (gameScore > gameHighScore) { gameHighScore = gameScore; }  // Update game score
			display.display();  // Make sure final frame is drawn
		}
		else if (gameState == STATUS_CREDIT) {  // Draw a Game Over screen w/ score
			display.display();  // Make sure final frame is drawn
		}
	}
}

