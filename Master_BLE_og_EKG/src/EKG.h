#include <Arduino.h>



#define FILTER_LENGTH 4
#define Array_Length 512

#define INCLUDE_vTaskDelayUntil 1         // er ikke sikker på om bruges

//Buffer til IIR filter samt B og A koefficienter
double x[FILTER_LENGTH + 1] = {0, 0, 0, 0, 0};
double y[FILTER_LENGTH + 1] = {0, 0, 0, 0, 0};
double b[FILTER_LENGTH + 1] = {0.3503,  0,   -0.7007,  0,    0.3503};
double a[FILTER_LENGTH + 1] = {1.0000, -1.4953, 0.4566, -0.1171, 0.1802};

//Buffer til normalisering af data
float y_norm[Array_Length];

//variabler til findepeaks
const float beatThreshold = 0.52;    // threshold til detektion af peaks (må maks være 1)
const int16_t beatTimeout = 25;     // 25 fordi ved en puls på 240 vil der være et RR-interval på på 0,25sek, og der er 100 samples pr. sekund

//Variabler til læs af sreiel procEKGTaskHandler
//HardwareSerial DataSerial(1);
  //    uint8_t s1RXpin = 17;       // tror der kan slettes 
    //  uint8_t s1TXpin = 16;       // tror der kan slettes

     // int16_t serialValue = 0;    //tror der kan slettes

// Task handler til sample EKG data task
TaskHandle_t EKGTaskHandler;
// Task handler til process activity data task
TaskHandle_t procEKGTaskHandler;

float floor_and_convert(float value) {       //skal mulighvis ikke bruges, når vi bruger float
  if (value > 0){ // positiv 
    return (float)(value + 0.5);
  }
  else { // negativ
    return (float)(value - 0.5);
  }
}

void Printarray(float Var_array2[Array_Length],float Array_Len) {    // skal mulighvis ikke bruges, ligner noget debug
  for (uint16_t i = 0; i < Array_Len; i++){
    Serial.println(Var_array2[i]);
  }
}

// void readTwoBytes(int16_t *val) {       //Læser indput data
// 	if (DataSerial.available() > 1) {
// 		byte lowbyte = DataSerial.read();
// 		*val = DataSerial.read();
// 		*val = ((*val)<<8) + lowbyte;
// 	}
// }


float iir_filter(int16_t value){
	x[0] =  (double) (value);           // Read received sample and perform typecast
	y[0] = b[0] * x[0];                 // Run IIR filter for first element
	for (int i = 1; i <= FILTER_LENGTH; i++){ // Run IIR filter for all other elements
		y[0] += b[i] * x[i] - a[i] * y[i];
	}
	for (int i = FILTER_LENGTH - 1; i >= 0; i--){ // Roll x and y arrays in order to hold old sample inputs and outputs
		x[i + 1] = x[i];
		y[i + 1] = y[i];
	}
	//return floor_and_convert(y[0]);     // fix rounding issues;
	return y[0];
}

void copyArrayData(float *fromArray, float *toArray, int16_t Array_Len){   // kopirer data til et andet array så Sampling og databehandling kan foregå seperat
  for (uint16_t i = 0; i < Array_Len+1; i++){
    *(toArray + i) = *(fromArray + i);
  }
}

float findMax(float *var_y, uint16_t arrayLength) {
	float maxvaerdi = 0;
	for(uint16_t i=0 ;i < arrayLength; i++){
		if (*(var_y+i) > maxvaerdi) {
			maxvaerdi = *(var_y+i);
		}
	}
	return maxvaerdi;
}

void Normalize(float *Norm_Array, float *Filt_Array, float maxV){ //Variablet Array
	Serial.print("Normalize(), Normalisere data med maks værdi: ");
	Serial.println(maxV, 5);
	if (maxV <= 0) {
		maxV = 1;
	}
	for (uint16_t i = 0; i < Array_Length; i++){
		*(Norm_Array + i) = (*(Filt_Array + i)) / maxV;
	}
}

uint8_t findPeaks(float *Var_array, uint16_t *fb, uint16_t *lb){ //Var_Array = Variablet Array
	int16_t lastBeat = 0 - beatTimeout;
	uint8_t beats = 0;
	for (uint16_t i = 1; i < Array_Length-1; i++){   //i 2 for i-2 kan lade sig gøre
		if ((i - lastBeat) > beatTimeout){    
			if  ((*(Var_array+i) > beatThreshold)){
				if ((*(Var_array+i)) >= (*(Var_array+i - 1)) and (*(Var_array+i)) >= (*(Var_array+i + 1))) {
					lastBeat = i;
					//Serial.println("findPeaks(), fandt peak!");
					//peaks(i-1) = y_norm(i-1);
					beats = beats + 1;
					if (beats == 1){
						*fb = i;
					}
				}
			}
		*lb = lastBeat;
		if (lastBeat > Array_Length) {
			*lb = 0;
		}
		}
	}
	Serial.print("findPeaks(), Beats fundet: ");
	Serial.print(beats);
	Serial.print(" , første beat index: ");
	Serial.print(*fb);
	Serial.print(" , sidste beat index: ");
	Serial.println(*lb);
    return beats;
}

double regnBPM (uint8_t Var_beats, uint16_t Var_finalbeat, uint16_t Var_firstbeat) { //NOTE: Alle variabler konverteret fra int16_T til float
	double BPM = 0;
	Serial.print("regnBPM(), Beats: ");
	Serial.print(Var_beats);
	Serial.print(" , første beat index: ");
	Serial.print(Var_firstbeat);
	Serial.print(" , sidste beat index: ");
	Serial.println(Var_finalbeat);	
	if (Var_finalbeat != 0 and Var_firstbeat != 0 and Var_finalbeat != Var_firstbeat) {
		BPM = (((double)Var_beats - (double)1) * (double)6000) / ((double)Var_finalbeat - (double)Var_firstbeat);      // (6000 pga 60 sek og 100hz )
	}
	Serial.print("regnBPM(), BPM is: ");
	Serial.println(BPM);
	return BPM;
}