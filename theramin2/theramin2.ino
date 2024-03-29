// Arduino theremin
// Test code for Sharp IR distance sensor
// Copyright 2009 mechomaniac.com
 
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
 
// Map all the input and output pins
#define AnalogInIR 0
#define DigitalOutSignal 11
 
#define INTERRUPT_PERIOD 512
#define FINT (F_CPU / INTERRUPT_PERIOD) // 16kHz?
#define FS (FINT)
 
// sine lookup table pre-calculated
prog_uchar PROGMEM sinetable[256] = {
  128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
  176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
  218,220,222,224,226,228,230,232,234,236,237,239,240,242,243,245,
  246,247,248,249,250,251,252,252,253,254,254,255,255,255,255,255,
  255,255,255,255,255,255,254,254,253,252,252,251,250,249,248,247,
  246,245,243,242,240,239,237,236,234,232,230,228,226,224,222,220,
  218,216,213,211,209,206,204,201,199,196,193,191,188,185,182,179,
  176,174,171,168,165,162,159,156,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,99, 96, 93, 90, 87, 84, 81, 
  79, 76, 73, 70, 67, 64, 62, 59, 56, 54, 51, 49, 46, 44, 42, 39, 
  37, 35, 33, 31, 29, 27, 25, 23, 21, 19, 18, 16, 15, 13, 12, 10, 
  9,  8,  7,  6,  5,  4,  3,  3,  2,  1,  1,  0,  0,  0,  0,  0,  
  0,  0,  0,  0,  0,  0,  1,  1,  2,  3,  3,  4,  5,  6,  7,  8,  
  9,  10, 12, 13, 15, 16, 18, 19, 21, 23, 25, 27, 29, 31, 33, 35, 
  37, 39, 42, 44, 46, 49, 51, 54, 56, 59, 62, 64, 67, 70, 73, 76, 
  79, 81, 84, 87, 90, 93, 96, 99, 103,106,109,112,115,118,121,124
};
 
// lookup table for output waveform
unsigned char wavetable[256];
 
unsigned int frequencyCoef = 100;
bool soundEnabled = true;
bool soundPWM = true; //start with square wave
bool SoundOn = false;
 
// This is called at sampling freq to output 8-bit samples to PWM
ISR(TIMER1_COMPA_vect)
{
  static unsigned int phase0;
  static unsigned int sig0;
  static unsigned char flag = 0;
  static unsigned int tempphase;
 
  if (soundPWM)
  {
    tempphase = phase0 + frequencyCoef;
    sig0 = wavetable[phase0>>8];
    phase0 = tempphase;
    OCR2A = sig0; // output the sample
  } 
  else { //square wave 
    flag ^= 1;
    digitalWrite(DigitalOutSignal, flag);
  }
}      
 
void SetupSquareSound()
{
  soundPWM = false;
}
 
void SetupPWMSound()
{
  // Set up Timer 2 to do pulse width modulation on the speaker pin.
  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));
  // Set fast PWM mode  (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);
  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  // Set initial pulse width to the first sample.
  OCR2A = 0;
  // Set up Timer 1 to send a sample every interrupt.
  cli();
  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));
  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = INTERRUPT_PERIOD;
  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);
  sei();
  soundPWM = true;
}
 
void StartSound()
{
  // Enable interrupt when TCNT1 == OCR1A (p.136)  
  cli();
  TIMSK1 |= _BV(OCIE1A);
  sei();
  SoundOn = true;
} 
 
void StopSound()
{
  cli();  
  // Disable playback per-sample interrupt.
  TIMSK1 &= ~_BV(OCIE1A);
  sei();
  SoundOn = false;
}
 
void SetFrequency(unsigned int freq)
{
  if (soundPWM) {
    unsigned long templong = freq;
    frequencyCoef = templong * 65536 / FS;
  } 
  else {
    unsigned long periode = F_CPU/(2*freq); //multiply by 2, because its only toggled once per cycle
    cli();
    OCR1A = periode;
  }
}
 
void SineWave()
{
  for (int i = 0; i < 256; ++i) {
    wavetable[i] = pgm_read_byte_near(sinetable + i);
  }
}
 
void SawtoothWave()
{
  for (int i = 0; i < 256; ++i) {
    wavetable[i] = i; // sawtooth
  }
}
 
void TriangleWave()
{
  for (int i = 0; i < 128; ++i) {
    wavetable[i] = i * 2;
  }
  int value = 255;
  for (int i = 128; i < 256; ++i) {
    wavetable[i] = value;
    value -= 2;
  }
}
 
void setup()
{ 
  pinMode (DigitalOutSignal, OUTPUT);  
  SineWave();
  //SawtoothWave();
  SetupPWMSound();
  StartSound();
} 
 
void loop() 
{
  int dist = analogRead(AnalogInIR); //read from the Sharp IR sensor
  int freq = map(dist, 0, 600, 1760, 440); 
  SetFrequency(freq);
  delay(50);
} 
