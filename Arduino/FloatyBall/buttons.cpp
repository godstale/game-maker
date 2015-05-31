#include "buttons.h"

uint8_t readCapacitivePin(int pinToMeasure) {
	/*	readCapacitivePin
		Input: Arduino pin number
		Output: A number, from 0 to 17 expressing
		how much capacitance is on the pin
		When you touch the pin, or whatever you have
		attached to it, the number will get higher 
	*/

	// Variables used to translate from Arduino to AVR pin naming
	volatile uint8_t* port;
	volatile uint8_t* ddr;
	volatile uint8_t* pin;
	// Here we translate the input pin number from
	// Arduino pin number to the AVR PORT, PIN, DDR,
	// and which bit of those registers we care about.
	byte bitmask;
	port = portOutputRegister(digitalPinToPort(pinToMeasure));
	ddr = portModeRegister(digitalPinToPort(pinToMeasure));
	bitmask = digitalPinToBitMask(pinToMeasure);
	pin = portInputRegister(digitalPinToPort(pinToMeasure));
	// Discharge the pin first by setting it low and output
	*port &= ~(bitmask);
	*ddr  |= bitmask;
	delay(1);
	// Prevent the timer IRQ from disturbing our measurement
	noInterrupts();
	// Make the pin an input with the internal pull-up on
	*ddr &= ~(bitmask);
	*port |= bitmask;

	// Now see how long the pin to get pulled up. This manual unrolling of the loop
	// decreases the number of hardware cycles between each read of the pin,
	// thus increasing sensitivity.
	uint8_t cycles = 17;
	if (*pin & bitmask) { cycles =  0;}
	else if (*pin & bitmask) { cycles =  1;}
	else if (*pin & bitmask) { cycles =  2;}
	else if (*pin & bitmask) { cycles =  3;}
	else if (*pin & bitmask) { cycles =  4;}
	else if (*pin & bitmask) { cycles =  5;}
	else if (*pin & bitmask) { cycles =  6;}
	else if (*pin & bitmask) { cycles =  7;}
	else if (*pin & bitmask) { cycles =  8;}
	else if (*pin & bitmask) { cycles =  9;}
	else if (*pin & bitmask) { cycles = 10;}
	else if (*pin & bitmask) { cycles = 11;}
	else if (*pin & bitmask) { cycles = 12;}
	else if (*pin & bitmask) { cycles = 13;}
	else if (*pin & bitmask) { cycles = 14;}
	else if (*pin & bitmask) { cycles = 15;}
	else if (*pin & bitmask) { cycles = 16;}

	// End of timing-critical section
	interrupts();

	// Discharge the pin again by setting it low and output
	// It's important to leave the pins low if you want to 
	// be able to touch more than 1 sensor at a time - if
	// the sensor is left pulled high, when you touch
	// two sensors, your body will transfer the charge between
	// sensors.
	*port &= ~(bitmask);
	*ddr  |= bitmask;

	return cycles;
}
uint8_t readCapXtal(int pinToMeasure) {
	// Variables used to translate from Arduino to AVR pin naming
	// volatile uint8_t* port;
	// volatile uint8_t* ddr;
	// volatile uint8_t* pin;

	// Here we translate the input pin number from
	// Arduino pin number to the AVR PORT, PIN, DDR,
	// and which bit of those registers we care about.
	byte bitmask;
	if(pinToMeasure == 2){ bitmask = B10000000; }
	else { bitmask = B01000000; }
  
	// port = portOutputRegister(PORTB);
	// ddr = portModeRegister(DDRB);
	// bitmask = PINB;
	// pin = portInputRegister(PINB);
	// Discharge the pin first by setting it low and output
  
	// bitmask =  B10000000
	// ~(bitmask) = B01111111
  
	PORTB &= ~(bitmask);  // SET LOW
	DDRB  |= bitmask;  // OUTPUT
	delay(1);
	// Prevent the timer IRQ from disturbing our measurement
	noInterrupts();
	// Make the pin an input with the internal pull-up on
	DDRB &= ~(bitmask);  // INPUT
	PORTB |= bitmask;

	// Now see how long the pin to get pulled up. This manual unrolling of the loop
	// decreases the number of hardware cycles between each read of the pin,
	// thus increasing sensitivity.
	uint8_t cycles = 17;
		 if (PINB & bitmask) { cycles =  0;}
	else if (PINB & bitmask) { cycles =  1;}
	else if (PINB & bitmask) { cycles =  2;}
	else if (PINB & bitmask) { cycles =  3;}
	else if (PINB & bitmask) { cycles =  4;}
	else if (PINB & bitmask) { cycles =  5;}
	else if (PINB & bitmask) { cycles =  6;}
	else if (PINB & bitmask) { cycles =  7;}
	else if (PINB & bitmask) { cycles =  8;}
	else if (PINB & bitmask) { cycles =  9;}
	else if (PINB & bitmask) { cycles = 10;}
	else if (PINB & bitmask) { cycles = 11;}
	else if (PINB & bitmask) { cycles = 12;}
	else if (PINB & bitmask) { cycles = 13;}
	else if (PINB & bitmask) { cycles = 14;}
	else if (PINB & bitmask) { cycles = 15;}
	else if (PINB & bitmask) { cycles = 16;}

	// End of timing-critical section
	interrupts();

	// Discharge the pin again by setting it low and output
	// It's important to leave the pins low if you want to 
	// be able to touch more than 1 sensor at a time - if
	// the sensor is left pulled high, when you touch
	// two sensors, your body will transfer the charge between
	// sensors.
	PORTB &= ~(bitmask);
	DDRB  |= bitmask;

	return cycles;
}
