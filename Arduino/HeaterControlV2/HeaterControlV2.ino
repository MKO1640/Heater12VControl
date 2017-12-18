//*********************
// * HW CONFIGURATION *
//*********************
//#define DEBUG	// output the results to Serial
#define OLED	// Use OLED display
//*********************


#include "temperature.h"
#include "encoder.h"
#include "adc.h"
//#include <AutoPID.h>
#include <PID_v1.h>
#include <EEPROM.h>
#include "eeprom.h"

#ifdef OLED
	#include <U8g2lib.h>
	#include "oled.h"
#endif



#define MOSFET_PIN 		A1
#define TEMPSENSOR_PIN 	A0
//#define KNOB_PIN 		A2	// Potentiometer for setting the desired temperature

#define encoderPinA   3	// Interrupt pin (coupled with capacitor to GND)
#define encoderPinB   4 // Interrupt pin (coupled with capacitor to GND)
#define BUTTON_PIN   12 // Pin for knob button



// RAMP can always be 3 deg per second



// ADC=1023 - temp sensor is not connected

// some macros: 
// mosfet (HIGH - OFF, LOW - ON)
#define H_ON 	digitalWrite(MOSFET_PIN,LOW)
#define H_OFF 	digitalWrite(MOSFET_PIN,HIGH)

uint8_t ControlType;		// 1 - Manual or 2 - Auto
uint8_t ProcessStage;		// 0 - just hold manual temperature; 1 - Ramp to Preheat; 2 - Preheat; 3 - Ramp to Reflow; 4 - Reflow; 5 - Cool down; 255 - finished

// PID global variables
int WindowSize = 500;
#define PID_VALUES_FACTOR 100.0	//PID values are stored in EEPROM in int format. So, scale them (div/mult) before use.
unsigned long soft_pwm_millis=0;
//bool soft_pwm_pin_state=LOW;

double currentTemp=0;
double setPoint=0;
double outputVal;

uint16_t pid_P, pid_I, pid_D; // PID values
uint16_t auto_preheatTemp, auto_preheatTime, auto_reflowTemp, auto_reflowTime;



//input/output variables passed by reference, so they are updated automatically
PID myPID(&currentTemp, &outputVal, &setPoint, 0, 0, 0, DIRECT); // PID values will be set later



bool stateHeater;
bool stateAdjustment=HIGH;	// Is temperature adjusted?
int last_presetTemp = 0;
unsigned long adjust_hyst_ms = 0;
unsigned long switch_mosfet_hyst_ms = 0;

#ifdef DEBUG
	unsigned long serial_mosfet_hyst_ms = 0;
#endif

void setup(){
	
	H_OFF;
	pinMode(MOSFET_PIN,OUTPUT);

	#ifdef DEBUG
		Serial.begin(57600);
	#endif
	
	#ifdef OLED
		u8g2.begin();
		u8g2.clearBuffer();
		u8g2.setFont(u8g2_font_inb16_mr);
		//u8g2_font_t0_22_tn - numeric font for current temperature
		//u8g2_font_profont12_tn - numeric font for preset temperature and timer
		
	#endif
	
	#ifdef DEBUG
		Serial.print("Temperature Sensor: ");
		Serial.println(analog2temp(collectADCraw(TEMPSENSOR_PIN)));
	#endif
	
		// read PID values from EEPROM
	restore_settingsEEPROM();
	myPID.SetTunings((float)pid_P / PID_VALUES_FACTOR,(float)pid_I / PID_VALUES_FACTOR,(float)pid_D / PID_VALUES_FACTOR);
	myPID.SetOutputLimits(0, WindowSize);	//set PID update interval to 4000ms

	initEncoder();
		
	// main screen (choose Manual.Auto)
	ControlType = constrain(EEPROM.read(EEPROM_CONTROLTYPE), 1, 2);
	drawMenu_AutoManual(ControlType);
	char encVal = 0;  // signed value - nothing is pressed
	while (rotaryEncRead() != 127) {
		encVal = rotaryEncRead();
		if(encVal!=127 && encVal!=0) {
			if(encVal>0){ControlType=1;}else{ControlType=2;}
			drawMenu_AutoManual(ControlType);
		}
	}
	EEPROM.update(EEPROM_CONTROLTYPE,ControlType);	// store selected COntrol Type for the next time
	

	if(ControlType==1){
		ProcessStage=0; // hold temperature
		//presetTemp = readEEPROMint(EEPROM_MANUAL_TEMP);
	}else{
		ProcessStage=1; // ramp to preheat temperature
		//presetTemp = readEEPROMint(EEPROM_AUTO_PREHEAT_TEMP);
	}
	
	myPID.SetMode(AUTOMATIC); // turn on PID
		
}

void loop() {
	
	if(ControlType==1){
		doManualReflow();
	}else{
		doAutoReflow();
	}

	currentTemp = analog2temp(collectADCraw(TEMPSENSOR_PIN));
	myPID.Compute();
	doSoftwarePWM((uint16_t)outputVal);	// make slow PWM to prevent unnecessary mosfet heating 
	
	#ifdef OLED
		// Display
		if (disp_hyst_ms+700 < millis()) {
			disp_hyst_ms=millis();
			screenRedraw();
		}	
	#endif
	
	// debug 
	#ifdef DEBUG
	if (serial_mosfet_hyst_ms+500 < millis()) {
		serial_mosfet_hyst_ms=millis();
		/*
		serial_mosfet_hyst_ms = millis();
		Serial.print("Set: ");
		Serial.print(presetTemp);
		Serial.print(", Actual: ");
		Serial.println(currentTemp);
		*/
	}	
	#endif

	if (has_encoderChange) {
		encoderVal+=rotaryEncRead();
		has_encoderChange=LOW;
		#ifdef DEBUG
			Serial.println(encoderVal);
		#endif
	}

}


// PWM output 
void doSoftwarePWM(uint16_t pwm_val){
	if (millis() - soft_pwm_millis > WindowSize)
	  { //time to shift the Relay Window
		soft_pwm_millis += WindowSize;
	  }
	if (pwm_val < millis() - soft_pwm_millis){H_ON;}else{H_OFF;}
		
}

void doManualReflow(){
}

void doAutoReflow(){
}
