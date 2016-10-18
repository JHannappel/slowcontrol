#include <avr/io.h>
#include <util/delay.h>

int main(void) {
	ACSR = _BV(ACBG); // ACIE
	DDRB = 1;
	while (true) {
		if (bit_is_set(ACSR, ACO)) {
			PORTB |= 1;
		} else {
			PORTB &= ~1;
		}
	}
}
