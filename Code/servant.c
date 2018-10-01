/*
 * servant.c
 *
 * Created: 6/6/2018 3:07:30 AM
 *  Author: jkim393
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "usart.c" //usart for bluetooth
#include "io.c"    //lcd
//#include "timer.h" not necessary

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;

void TimerOn() {
	TCCR1B = 0x0B;
	OCR1A = 125;
	TIMSK1 = 0x02;
	TCNT1=0;
	_avr_timer_cntcurr = _avr_timer_M;
	SREG |= 0x80;
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
	/*ISR(TIMER1_COMPA_vect) {
		// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
		_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
		if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
			TimerISR(); // Call the ISR that the user uses
			_avr_timer_cntcurr = _avr_timer_M;
		}*/
}

ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

unsigned char reset = 0x00;
unsigned char upButton = 0x00;
unsigned char enterButton = 0x00;
unsigned char downButton = 0x00;
unsigned char flag = 0x00;
unsigned char temp = 0x00; // variable used to store received data
unsigned char stride = 0x00;
unsigned char strideBefore = 0x00;
unsigned long step = 0x00;  //65,535 maximum
unsigned long stepBefore = 0x00;
unsigned long distance = 0x00;
unsigned long distanceBefore = 0x00;

enum States {start, init, wait, upPress, inc, downPress, dec, update} state;
enum lcdStates {lcdStart, lcdWrite, lcdCount} lcdState;

void incStride(){
	if(stride < 95){
		stride += 5;
	}
	return;
}

void decStride(){
	if(stride >= 5){
		stride -= 5;
	}
	return;
}

void lcdtick(){
	switch(lcdState){ //transition
		case lcdStart:
			lcdState = lcdWrite;
			break;
		case lcdWrite:
			if(flag){
				LCD_DisplayString(1,"Steps:");
				LCD_DisplayString1Line(1, 2, "Dist(m):");
				lcdState = lcdCount;
				break;
			}
			else {
				lcdState = lcdWrite;
				break;
			}
		case lcdCount:
			if(flag){
				lcdState = lcdCount;
				break;
			}
			else {
				lcdState = lcdStart;
				break;
			}
		default:
			lcdState = lcdStart;
			break;
	}
	switch(lcdState){ //action
		case lcdStart:
			LCD_DisplayString(1,"Stride(cm):");
			break;
		case lcdWrite:
			if(stride != strideBefore){
				LCD_Cursor(12);
				LCD_WriteData((stride/10) + '0');
				LCD_Cursor(13);
				LCD_WriteData((stride%10)+ '0');
				strideBefore = stride;
			}
			break;
		case lcdCount:
			if(step != stepBefore){
				LCD_Cursor(7);
				LCD_WriteData((step/1000000) + '0');
				LCD_Cursor(8);
				LCD_WriteData(((step%1000000)/100000) + '0');
				LCD_Cursor(9);
				LCD_WriteData((((step%1000000)%100000)/10000) + '0');
				LCD_Cursor(10);
				LCD_WriteData(((((step%1000000)%100000)%10000)/1000) + '0');
				LCD_Cursor(11);
				LCD_WriteData((((((step%1000000)%100000)%10000)%1000)/100) + '0');
				LCD_Cursor(12);
				LCD_WriteData(((((((step%1000000)%100000)%10000)%1000)%100)/10) + '0');
				LCD_Cursor(13);
				LCD_WriteData(((((((step%1000000)%100000)%10000)%1000)%100)%10) + '0');
				stepBefore = step;
			}
			if(distance != distanceBefore){
				LCD_Cursor(25);
				LCD_WriteData((distance/1000000) + '0');
				LCD_Cursor(26);
				LCD_WriteData(((distance%1000000)/100000) + '0');
				LCD_Cursor(27);
				LCD_WriteData((((distance%1000000)%100000)/10000) + '0');
				LCD_Cursor(28);
				LCD_WriteData(((((distance%1000000)%100000)%10000)/1000) + '0');
				LCD_Cursor(29);
				LCD_WriteData((((((distance%1000000)%100000)%10000)%1000)/100) + '0');
				LCD_Cursor(30);
				LCD_WriteData(0x2E);
				LCD_Cursor(31);
				LCD_WriteData(((((((distance%1000000)%100000)%10000)%1000)%100)/10) + '0');
				LCD_Cursor(32);
				LCD_WriteData(((((((distance%1000000)%100000)%10000)%1000)%100)%10) + '0');
				distanceBefore = distance;
			}
			break;
	}
}

void buttontick(){
	switch(state){ //transition
		case start:
			state = init;
			break;
		case init:
			state = wait;
			break;
		case wait:
			if (enterButton){
				state = update;
				flag = 0x01;
				break;
			}
			else if (upButton && !downButton) {
				state = upPress;
				break;
			}
			else if (!upButton && downButton){
				state = downPress;
				break;
			}
			else {
				state = wait;
				break;
			}
		case upPress:
			if(!upButton){
				state = inc;
				break;
			}
			else {
				state = upPress;
				break;
			}
		case inc:
			state = wait;
			break;
		case downPress:
			if(!downButton){
				state = dec;
				break;
			}
			else {
				state = downPress;
				break;
			}
		case dec:
			state = wait;
			break;
		case update:
			if(!reset){
				state = update;
				break;
			}
			else {
				state = init;
				flag = 0x00;
				break;
			}
		default:
			state = start;
			break;
	}
	switch(state){ //action
		case start:
			break;
		case init:
			stride = 0;
			step = 0;
			distance = 0;
			break;
		case wait:
			break;
		case upPress:
			break;
		case inc:
			incStride();
			break;
		case downPress:
			break;
		case dec:
			decStride();
			break;
		case update:
			if ( USART_HasReceived(0) ) {
				 temp = USART_Receive(0);
				 step++;
				 distance += stride;
			} 
			else {
				temp = 0x00;
			}
			PORTB = temp;
			break;
	}
}

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xF0; PORTD = 0x0F;
	
	initUSART(0);
	LCD_init();
	TimerSet(100);
	TimerOn();
	LCD_DisplayString(1,"Stride(cm):");
	
	while(1)
	{
		reset = (~PINA & 0x01);
		upButton = (~PINA & 0x02);
		enterButton = (~PINA & 0x04);
		downButton = (~PINA & 0x08);
		
		buttontick();
		lcdtick();
		while (!TimerFlag) {}
		TimerFlag = 0;
	}
}