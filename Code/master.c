/*
 * master.c
 *
 * Created: 6/6/2018 2:48:56 AM
 *  Author: jkim393
 */ 


#define F_CPU 8000000UL		/* Define CPU clock Frequency 8MHz */
#include <avr/io.h>		/* Include AVR std. library file */
#include <avr/interrupt.h>
#include <util/delay.h>		/* Include delay header file */
#include <inttypes.h>		/* Include integer type header file */
#include <stdlib.h>		/* Include standard library file */
#include <stdio.h>		/* Include standard I/O library file */
#include "MPU6050_res_define.h"	/* Include MPU6050 register define file */
#include "I2C_Master_H_file.h"	/* Include I2C Master header file */
#include "I2C_Master_C_file.c"
#include "usart.c"              /*Usart for bluetooth*/

float threshold = 0.05;
float Acc_x,Acc_y,Acc_z;
unsigned char i = 0x00;
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

//Gyro_Init(), MPU_Start_Loc(), Read() functions taken from electronicwings.com
void Gyro_Init()		/* Gyro initialization function */
{
	_delay_ms(150);		/* Power up time >100ms */
	I2C_Start_Wait(0xD0);	/* Start with device write address */
	I2C_Write(SMPLRT_DIV);	/* Write to sample rate register */
	I2C_Write(0x07);	/* 1KHz sample rate */
	I2C_Stop();

	I2C_Start_Wait(0xD0);
	I2C_Write(PWR_MGMT_1);	/* Write to power management register */
	I2C_Write(0x01);	/* X axis gyroscope reference frequency */
	I2C_Stop();

	I2C_Start_Wait(0xD0);
	I2C_Write(CONFIG);	/* Write to Configuration register */
	I2C_Write(0x00);	/* Fs = 8KHz */
	I2C_Stop();

	I2C_Start_Wait(0xD0);
	I2C_Write(GYRO_CONFIG);	/* Write to Gyro configuration register */
	I2C_Write(0x18);	/* Full scale range +/- 2000 degree/C */
	I2C_Stop();

	I2C_Start_Wait(0xD0);
	I2C_Write(INT_ENABLE);	/* Write to interrupt enable register */
	I2C_Write(0x01);
	I2C_Stop();
}

void MPU_Start_Loc()
{
	I2C_Start_Wait(0xD0);	/* I2C start with device write address */
	I2C_Write(ACCEL_XOUT_H);/* Write start location address from where to read */
	I2C_Repeated_Start(0xD1);/* I2C start with device read address */
}

void Read()
{
	MPU_Start_Loc();	/* Read Gyro values */
	Acc_x = (((int)I2C_Read_Ack()<<8) | (int)I2C_Read_Ack());
	Acc_y = (((int)I2C_Read_Ack()<<8) | (int)I2C_Read_Ack());
	Acc_z = (((int)I2C_Read_Ack()<<8) | (int)I2C_Read_Ack());
	I2C_Stop();
}

enum States {start, read, pass} state;
float Xa,Y,Za;

void tick(){
	switch(state){//transition
		case start:
			state = read;
			break;
			
		case read:
			if(Y > threshold /*|| Xa > threshold || Za > threshold*/){
				PORTB = 0x01;
				if ( USART_IsSendReady(0) ) {
					USART_Send(0xFF, 1);
				}
				if ( USART_HasTransmitted(0) ) {
					USART_Flush(0);
				}
				state = pass;
				break;
			}
			else {
				PORTB = 0x00;
				state = read;
				break;
			}
			
		case pass:
			if(Y > threshold /*|| Xa > threshold || Za > threshold*/){
				state = pass;
				break;
			}
			else {
				state = read;
				break;	
			}
		
		default:
			state = start;
			break;
	}
	switch(state){//action
		case start:
			break;
		case read:
			PORTB = 0x00;
			break;
		case pass:
			PORTB = 0x01;
			break;
	}
}

int main()
{
	/*float Xa,Ya,Za;*/
	I2C_Init();		/* Initialize I2C   */
	Gyro_Init();	/* Initialize Gyro  */
	initUSART(0);	/* Initialize USART */
	TimerSet(100);
	TimerOn();
	while(1)
	{
		Read();
		//Xa = Acc_x/16384.0;
		Y = Acc_y/16384.0;
		//Za = Acc_z/16384.0;
		tick();
// 		Read();
// 		Xa = Acc_x/16384.0;
// 		//Ya = Acc_y/16384.0;
// 		//Za = Acc_z/16384.0;
// 
// 		if (Xa > 0.3 /*|| Xa < -0.3*/){
// 			PORTB = 0x01;
// 			if ( USART_IsSendReady(0) ) {
// 				USART_Send(0xFF, 1);
// 			}
// 			if ( USART_HasTransmitted(0) ) {
// 				USART_Flush(0);
// 			}
// 		}
// 		else {
// 			PORTB = 0x00;
// 		}

		while (!TimerFlag) {}
		TimerFlag = 0;
	}
}