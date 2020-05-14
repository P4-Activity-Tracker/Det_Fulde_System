#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Start/Stop indikator
void ( *startSampleFuncPointer) ();
bool doStopSampling = false;

bool isSamplingRunning = false;

uint32_t lastActivity = 0;
#define timeoutThreshold 20000

// UUID'er til BLE
#define APP_SERVICE_UUID "b6785631-99ba-4128-8871-72651d3f1228"
#define APP_RX_CHARACTERISTIC_UUID "4f1cdbda-590e-47c9-b193-1a2e694029cd"
#define APP_TX_CHARACTERISTIC_UUID "337d98b2-e1f6-43b7-b9e9-451f241f3965"
#define IMU_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define IMU_RX_CHARACTERISTIC_UUID "12ee6f51-021d-438f-8094-bf5c5b36eab9"
#define IMU_TX_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// læs/recive (RX) og send/transmit (TX)--> RX og TX UUID er omvendt på BLE-client (slave-enhed)
BLECharacteristic *pCharacteristic_TX_APP; 
BLECharacteristic *pCharacteristic_RX_APP;
BLECharacteristic *pCharacteristic_TX_IMU; 
BLECharacteristic *pCharacteristic_RX_IMU;

// Variabler som modtages fra APP                                 skal måske op i toppen 
int activity = 0;
int32_t peakCount;
int32_t peakCountTotal = 0;

// MyCallbacks er en form for tilbagekald, der tilknyttes BLE-egenskaber for at informere om begivenheder.
class IMU_MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) { // Callback funktion som gør at der kan læses en værdi som sendes af BLE-client.
    	//std::string value = pCharacteristic->getValue();
    	String value = pCharacteristic_RX_IMU->getValue().c_str(); // modtag data fra IMU
    	activity = value.substring(0,1).toInt();  
		peakCount = value.substring(1,3).toInt();
		peakCountTotal = peakCountTotal + peakCount;
		Serial.print("Fik aktivitet: ");
		Serial.println(activity);

		lastActivity = millis();
		// pCharacteristic_TX_APP->setValue(value.c_str());   // send det modtaget data fra IMU til APP'en


    	//   if (value.length() > 0) {
    	//     Serial.println("*********");
    	//     Serial.print("New value: ");
    	//     for (int i = 0; i < value.length(); i++)
    	//       Serial.print(value[i]);

    	//     Serial.println();
    	//     Serial.println("*********");
    	//   }
    }
};

class myServerCallback: public BLEServerCallbacks{
	void onConnect(BLEServer* pServer){
		BLEDevice::startAdvertising();
		Serial.println("forbundet til klient");
		if (isSamplingRunning) {
			pCharacteristic_TX_IMU->setValue("start"); // send beskeden videre til IMU
		} else {
			pCharacteristic_TX_IMU->setValue("stop"); 
		}

	};
};

// Variabler som modtages fra APP                                 skal måske op i toppen 
int8_t weight;
int8_t age;
int8_t height;
int8_t gender;
int8_t HRhvile;
bool brugerdata;  // bruges til at opdatere variabler i EE-kode-delen 
bool nystart;



class APP_MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) { // Callback funktion som gør at der kan læses en værdi som sendes af BLE-client.
      //std::string value = pCharacteristic->getValue();
      String value = pCharacteristic_RX_APP->getValue().c_str();

		if (value.indexOf ("start") >= 0) { // Finder index til "start" i besked, -1 hvis ikke fundet
			pCharacteristic_TX_IMU->setValue("start"); // send beskeden videre til IMU
			pCharacteristic_TX_IMU->notify();
			startSampleFuncPointer(); // start samplingsfunktionen
			brugerdata = true;   // Gør at vi opdatere variabler i EE-kode-delen
			nystart = true; // gør at der nulstilles i samplingsarray'et og starttid
			Serial.println("start det skidt");
		}
		else if (value.indexOf ("stop") >= 0){
			pCharacteristic_TX_IMU->setValue("stop"); // send beskeden videre til IMU
			pCharacteristic_TX_IMU->notify();
			doStopSampling = true; // stopper sampling
			Serial.println("stop det skidt");
		}
		else if (value.length() == 11){		// gem data i de forskellige variabler som integers
		Serial.println("modtog brugerdata");
			weight = value.substring(0,3).toInt();  
			age = value.substring(3,5).toInt();
			height = value.substring(5,8).toInt();
			gender = value.substring(8,9).toInt();
			HRhvile = value.substring(9,11).toInt();
			brugerdata = true;   // Gør at vi opdatere variabler i EE-kode-delen
		}
	
		//Serial.println(value);

	
		//   if (value.indexOf ("start") >= 0) {
    	//   // start eller stop sampling
    	//   Serial.print("Lortet starter")
    	//   }
    	//  if (value.indexOf ("stop") >= 0) {
    	//   // start eller stop sampling
    	//   Serial.print("Lortet Stopper")
    	//   }

    	//   if (value.length() == 11) {
    	//     Serial.println("*********");
    	//     Serial.print("New value: ");
    	//     for (int i = 0; i < value.length(); i++)
    	//       Serial.print(value[i]);

    	//     Serial.println();
    	//     Serial.println("*********");
    	//      }
    
		// Serial.print("vægt:");       // Dette er for for at sikre at vi modtager de forventede værdier
		// Serial.println(weight);		// dette skal ikke med i den endelige kode, men bruges til tests
		// Serial.print("alder:");
		// Serial.println(age);
		// Serial.print("højde:");
		// Serial.println(height);
		// Serial.print("Køn:");
		// Serial.println(gender);
		// Serial.print("HRhvile:");
		// Serial.println(HRhvile);

    }
};


void writeToAPP (String message){
	//std::string value = message.c_str();
	pCharacteristic_TX_APP->setValue(message.c_str());    // skal der sendes i c_string? (jacob spørger)
	pCharacteristic_TX_APP->notify();
}