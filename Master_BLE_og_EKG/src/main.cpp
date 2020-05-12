#include <Arduino.h>
#include "BLE.h"
#include "EKG.h"
#include "SD_ESP32.h"

// BLE variabler og funktioner

// Funktionsdefinationer

//Task der sampler og flitrerer EKG
void sampleEKGDataTask(void *pvParamaters);
//Task der normaliserer og finder peaks
void processEKGDataTask(void *pvParameters);

void startSampleTask() {
  	vTaskResume(EKGTaskHandler);
}

void stopSampleTask() {
  	vTaskSuspend(EKGTaskHandler);
}

//Buffere som filtreret data lægges i
float EKGbuffer[Array_Length];  //Buffer der fyldes til 499 med filtreret data og overføres til filtData
float filtData[Array_Length];   //Buffer der benyttes til Normalisering og FindPeaks

#define adPin 32

uint16_t samples[10];

// EKG variabeler og funktioner

unsigned long startTime = 0;
unsigned long endTime = 0;
uint32_t excerciseTime = 0;

float VO2pulse = 0;
float VO2rest = 0;

float HRmax;     // Udregn HRmax
float HRreserve; // Udregn HRreserve

float VO2max;
float VO2reserve;

float THR;
float EVO2;
float EE;
float totalEE;

// funktion som sørger for at konvertere heltal til en string af længden characters.
String dataToCharacters(int32_t data, uint8_t characters)
{                              //input data kan bestå af HR, EE, antal skridt, og characters beskriver antallet af karaktere i output string.
  String dataS = String(data); // Ex: 12 skridt -> "12"
  for (uint8_t i = dataS.length(); i < characters; i++)
  {                      // Ex: længde 2, ønskede karaktere 5, mangler 3 nuller
    dataS = "0" + dataS; // Tilfør nul foran de andre karaktere
  }
  return dataS; // Ex: "00012"
}

void setup() {
  Serial.begin(115200);
  // Funktioner pointers
  startSampleFuncPointer = startSampleTask;
  stopSampleFuncPointer = stopSampleTask;

  /**************************** SD-Kort setup *******************************/
  isCardMounted();

  doesFileExist();

  /*************************** BLE SETUP *****************************/

  Serial.println("Starter BLE Server");

  BLEDevice::init("ESP32_Master");                                     // opret en enhed og giver enheden navnet ESP32_Master
  BLEServer *pServer = BLEDevice::createServer();                      // sætter enheden til at være en server
  pServer->setCallbacks(new myServerCallback());
  BLEService *pService_APP = pServer->createService(APP_SERVICE_UUID); // opret en tjeneste til APP-enhed med egenskaberne
  pCharacteristic_TX_APP = pService_APP->createCharacteristic(
      APP_TX_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_RX_APP = pService_APP->createCharacteristic(
      APP_RX_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);

  BLEService *pService_IMU = pServer->createService(IMU_SERVICE_UUID); // opret en tjeneste til IMU-enhed med egenskaberne
  pCharacteristic_TX_IMU = pService_IMU->createCharacteristic(
      IMU_TX_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_RX_IMU = pService_IMU->createCharacteristic(
      IMU_RX_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);

  // kalder funktionen der kan læse data der sendes fra client-enhed
  pCharacteristic_RX_APP->setCallbacks(new APP_MyCallbacks());
  pCharacteristic_RX_IMU->setCallbacks(new IMU_MyCallbacks());

  // Opretter en a BLE Descriptor til begge Charateristics
  pCharacteristic_RX_APP->addDescriptor(new BLE2902()); // til APP
  pCharacteristic_TX_APP->addDescriptor(new BLE2902()); // til APP
  pCharacteristic_RX_IMU->addDescriptor(new BLE2902()); // til IMU
  pCharacteristic_TX_IMU->addDescriptor(new BLE2902()); // til IMU
  pService_APP->start();                                // starter tjenesten til APP'en
  pService_IMU->start();                                // starter tjenesten til IMU'en
  BLEAdvertising *pAdvertising_APP = BLEDevice::getAdvertising();
  pAdvertising_APP->addServiceUUID(APP_SERVICE_UUID); // anoncer til APP'en
  pAdvertising_APP->setScanResponse(true);
  pAdvertising_APP->setMinPreferred(0x06); // funktion som hjælper med iPhone connection problemer
  pAdvertising_APP->setMinPreferred(0x12); // funktion som hjælper med iPhone connection problemer

  BLEAdvertising *pAdvertising_IMU = BLEDevice::getAdvertising();
  pAdvertising_IMU->addServiceUUID(IMU_SERVICE_UUID); // anoncer til IMU'en
  pAdvertising_IMU->setScanResponse(true);

  BLEDevice::startAdvertising(); // start annoncering, så andre BLE-enheder kan scanne og finde denne BLE-enhed
  Serial.println("Characteristic er defineret! Nu kan det ses på  smartphone!");

  /****************************** EKG SETUP *******************************/
	analogSetPinAttenuation(adPin, ADC_11db);
	adcAttachPin(adPin);
  // Lav EKG sampler task
  xTaskCreate(
      sampleEKGDataTask,
      "Sample EKG data",
      2024,
      NULL,
      2,
      &EKGTaskHandler);

  // Lav process EKG data task
  xTaskCreate(
      processEKGDataTask,
      "process EKG data",
      16384,
      NULL,
      1,
      &procEKGTaskHandler);

  delay(100); //kan nok godt slettes

  //vTaskResume(EKGTaskHandler);
}

void loop()
{
  // skriv kode til loop her:
}

void sampleEKGDataTask(void *pvParapvParamaters) {
	Serial.print("sampling start nr.1");
  	// Task setup
  	const TickType_t frequency = 10; // Tick frekvens af task
  	//variabler til opsamling og filtrering af data
  	int16_t y_org;
  	//tæller til nuværende index af x buffer og y buffer
  	uint16_t dataIndex = 0;
  	vTaskSuspend(NULL); 
	TickType_t lastWakeTime = xTaskGetTickCount(); // Gem nuværende tid
  	// Task loop
  	for (;;) {
		if (nystart) {
			dataIndex = 0;
			startTime = millis(); // Gemmer start tiden
			nystart = false;
			Serial.print("Sampling start nr.2");
		} 
		// Læs indput
		//readTwoBytes(&y_org);
		y_org = analogRead(adPin);
		if (dataIndex < 10) {
			samples[dataIndex] = y_org;
		}
		//Serial.println("Ny Værdi");
		//Serial.println(y_org);
		EKGbuffer[dataIndex] = iir_filter(y_org);

		if (dataIndex >= Array_Length - 1) {
			// for (uint8_t i = 0; i < 10; i++) {
			// 	Serial.print("y_org: ");
			// 	Serial.println(samples[i]);
			// }

			dataIndex = 0;                                    //sæt data Index tilbage til 0
			copyArrayData(EKGbuffer, filtData, Array_Length); //Kopier data fra rolling buffer til static buffer
			vTaskResume(procEKGTaskHandler);
		} else {
			dataIndex++; //Optæl dataindex hvis bufferen ikke er fyldt endnu
		}
    	vTaskDelayUntil(&lastWakeTime, frequency); // Vent indtil der skal samples igen
	}
}

// Task til HR, EE, og sending af data
void processEKGDataTask(void *pvParameters) {
  	// Task setup
  	brugerdata = true;
  	vTaskSuspend(NULL); // Sæt task på pause indtil den skal bruges
  	// Task loop
  	for (;;) {
		Serial.print("procces start");
		if (brugerdata) {            // I tilfælde af der er kommet nye bruger data opdateres dette
			HRmax = 208 - (age * 0.7);   // Udregn HRmax
			HRreserve = HRmax - HRhvile; // Udregn HRreserve
			// hvad sender vi som mand og hvad er kvinde (0,1,2?) hvilket nummer tildeles kvinde??
			if (gender == 2) { // Udregner VO2rest for kvinde eller mand  // Kvinde
				VO2rest = (((655.1 + (9.56 * weight) + (1.85 * height) - (4.68 * age)) / 24) / 60) / 5;
			}
			else { // Mand
				VO2rest = (((66.5 + (13.75 * weight) + (5.03 * height) - (6.75 * age)) / 24) / 60) / 5;
			}
			VO2max = 3.542 + (-0.014 * age) + (0.015 * weight) + (-0.011 * HRhvile); // Udregner VO2max
			VO2reserve = VO2max - VO2rest;                                           // Udregner VO2reserve
			VO2pulse = VO2reserve / HRreserve;                                       // Udregner VO2puls
			brugerdata = false; //gør vi ikke kører gennem dette før næste gang brugerdata ændres
			Serial.print("brugerdata installeret");
		}
		// Klargør task til næste data processering
		uint16_t firstBeat = 0; 
		uint16_t finalBeat = 0; 
		float maxV = 0;
		uint8_t beats = 0;
		//Variabler til antal hjerteslag
		float BPM = 0;
		// Normaliser EKG
		maxV = findMax(filtData, Array_Length);
		Normalize(y_norm, filtData, maxV);
		// for (uint8_t i = 0; i < 10; i++) {
		// 	Serial.print("EKGbuffer: ");
		// 	Serial.print(EKGbuffer[i]);
		// 	Serial.print(" , filtData: ");
		// 	Serial.print(filtData[i]);
		// 	Serial.print(" normalized: ");
		// 	Serial.println(y_norm[i]);
		// }
		// Find  Peaks
		beats = findPeaks(y_norm, &firstBeat, &finalBeat);
		// Beregn Puls
		BPM = regnBPM(beats, finalBeat, firstBeat);
		// Beregn EE
		endTime = millis();                            // Gemmer slut tiden
		excerciseTime = (endTime - startTime) / 1000; // Udregner samlet motionstid i sek
		THR = BPM - HRhvile;             // Udregner THR
		if (BPM < HRhvile) {
			THR = 0;
		}
		EVO2 = THR * VO2pulse + VO2rest; // Udregner EVO2
										// EE = EVO2 * 5 * excerciseTime;   // Udregner EE
		EE = EVO2 * 5 * Array_Length / 6000;          // 5/60 da det er de sidste 5 sek vi regner ee ud fra
		totalEE = totalEE + EE; // skal det være en pointer?

		// Modtaget data fra IMU??
		//sker i BLE.h

		// Send data til APP
		// Akt, BPM, EE, Skridt, Tid(s)

		String dataOut = dataToCharacters(activity, 1) + dataToCharacters(BPM, 3) + dataToCharacters(totalEE, 4) + dataToCharacters(peakCountTotal, 5) + dataToCharacters(excerciseTime, 5);

		writeToAPP(dataOut);

		// Gem på SD-kort

		String dataOut2 = dataToCharacters(activity, 1) + "," + dataToCharacters(BPM, 3) + "," + dataToCharacters(totalEE, 4) + "," + dataToCharacters(peakCountTotal, 5) + "," + dataToCharacters(excerciseTime, 5) + "\r\n";

		appendFile(SD, fileName.c_str(), dataOut2.c_str());
			Serial.print("Procces gjort");
		//Suspend task indtil der er ny
		vTaskSuspend(NULL); // data klar
	}
}
