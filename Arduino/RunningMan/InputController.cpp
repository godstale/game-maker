#include "InputController.h"

InputController::InputController() {
}

uint8_t InputController::getInput() {
	uint8_t value = B00000000;
//	int x_move = analogRead(JOYSTICK_X_PIN);
//	int y_move = analogRead(JOYSTICK_Y_PIN);
//
//	if (x_move < LEFT_THRESHOLD) { value = value | B00100000; }	// left
//	if (y_move < UP_THRESHOLD) { value = value | B00010000; }	// up
//	if (x_move > RIGHT_THRESHOLD) { value = value | B00001000; }	// right
//	if (y_move > DOWN_THRESHOLD) { value = value | B00000100; }	// down
	if (digitalRead(BUTTON_A_PIN) == LOW) { value = value | B00000010; }	// a?
	if (digitalRead(BUTTON_B_PIN) == LOW) { value = value | B00000001; }	// b?
	return value;
}

