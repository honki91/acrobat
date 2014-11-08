//
//  main.c
//
//  Created by Max Wasserman on 11/1/14.
//

#include <avr/io.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "m_general.h"
#include "m_usb.h"
#include "m_imu.h"

#define SYSTEM_CLOCK 16*pow(10,6)
#define POLLING_FREQ 100

#define ACC_SCALE 1
#define GYRO_SCALE 0

#define AX 0
#define AY 1
#define AZ 2
#define GX 3
#define GY 4
#define GZ 5

#define ALPHA 0.98
#define OMEGA_X_OFFSET 3
#define RAD_TO_DEG 57.29578

void init();
void init_timer1();
void convertGyroToDps_print();
void send_float(char *label, float value);
void motor_init();
void init_timer0();

int imu_buf[9];
float G_GAIN[4] = {131, 65.5, 32.8, 16.4}; //[LSB/dps]
float A_GAIN[4] = {16384, 8192, 4096, 2048}; //[LSB/g]
float DT = 1.0/POLLING_FREQ;

float omega_x = 0;
float acc_angle_x = 0;
float gyro_angle_x = 0;
float cf_angle_x = 0;

int main()
{
  init();
  m_bus_init();
  m_imu_init(ACC_SCALE, GYRO_SCALE);
  init_timer1();
  /* motor_init(); */
  /* init_timer0(); */

  //  motors on
  // OCR0A = 200;

  while(1) { }
}

void init()
{
  //set system clock to 16MHz
  m_clockdivide(0);

  // enable global interrupts
  sei();

  //Disable JTAG
  m_disableJTAG();

  // initalize usb communiations
  m_usb_init();
}

// this timer will pull values from the imu when it overflows
void init_timer1()
{
  float timer_freq;

  // set prescaler to /8
  clear(TCCR1B,CS12);
  set(TCCR1B,CS11);
  clear(TCCR1B,CS10);

  // timer freq = 16MHz /8 = 2MHz
  timer_freq = SYSTEM_CLOCK/8;

  // set timer mode: (mode 4) UP to OCR1A
  clear(TCCR1B,WGM13);
  set(TCCR1B,WGM12);
  clear(TCCR1A,WGM11);
  clear(TCCR1A, WGM10);

  // set OCR1A so that interrupt would occur at POLLING_FREQ.
  OCR1A =  timer_freq/POLLING_FREQ;

  // enable global interrupts
  sei();

  // enable interrput when timer is OCR1A
  set(TIMSK1,OCIE1A);
}

// convert gyro values to degrees per second
void convertGyroToDps_print()
{
  acc_angle_x = atan2(imu_buf[AX], imu_buf[AZ]) * RAD_TO_DEG;

  // Gives same result as above for small angles.
  // theta_x = a_x/g
  // acc_angle_x = imu_buf[AX]/A_GAIN[ACC_SCALE] * RAD_TO_DEG);

  omega_x = imu_buf[GX]/G_GAIN[GYRO_SCALE] + OMEGA_X_OFFSET;

  // This value simply drifts away from zero.
  // gyro_angle_x = gyro_angle_x + omega_x*DT;

  cf_angle_x = ALPHA * (cf_angle_x + omega_x*DT) + (1-ALPHA)*acc_angle_x;

  send_float("acc_angle", acc_angle_x);
  send_float("omega", omega_x);
  send_float("cf_angle", cf_angle_x);
  usb_tx_string("-----------------------\n");
}

// pull data from imu to buf
ISR(TIMER1_COMPA_vect)
{
  m_imu_raw(imu_buf);

  convertGyroToDps_print();
  m_red(TOGGLE);
}

char buf[100];
void send_float(char *label, float value) {
  sprintf(buf, "%s: %.3f\n", label, value);
  int i;
  for (i=0; i< strlen(buf); i++) {
    m_usb_tx_char(*(buf+i));
  }
}

//// CODE BELOW NOT YET USED ////

void motor_init()
{  // set pin B7 as output  - note this problem is symettric. thus will only use one pin for both motors
  set(DDRB,7);   ///ENABLE PINS

  //MOTOR DIRECTION
  set(DDRB,3);

  // intially low
  clear(PORTB,3);

  // set speed of motors with pwm - adjusts time motor is in with enable to b7

  // slow down pwm: stay in hundreds Hz (~300)
}

void init_timer0()
{  // set speed by adjusting OCR0A and OCR0B:  Vout = (OCR0n)/(255)*Vin

  // BE SURE D0 IS NOT MESSED UP BC OF THIS TIMER

  // set timer mode:UP to 0xFF, PWM mode
  clear(TCCR0B,WGM02);
  set(TCCR0A,WGM01);
  set(TCCR0A,WGM00);

  //  B7: OCR0A OUTPUT COMPARE: clear at OCR0A, set at 0xFF: Duty cycle - OCR0A/255
  set(TCCR0A, COM0A1);
  clear(TCCR0A, COM0A0);

  // enable global interrupts
  sei();

  //initially duty cycle is 0 - motor is off
  OCR0A = 0;

  // set prescaler to /8 = 2 Mhz
  clear(TCCR0B,CS02);
  set(TCCR0B,CS01);
  clear(TCCR0B,CS00);
}

