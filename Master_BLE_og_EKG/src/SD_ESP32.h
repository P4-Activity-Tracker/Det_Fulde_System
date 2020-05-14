#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

/*
 * Connect the SD card to the following pins:
 *
 * SD-kort adapter | Pin setup på ESP32
 *        3.3V               5V       
 *        GND                GND
 *        MOSI               23
 *        MISO               19 
 *        SCK                18
 *        SS                 5
 */

String dataMessage;
String activityType;
String stepData;
String timeData;
String eeData;
String hrData;
int dataNumber = 0;
String fileName = "/data" + String(dataNumber) + ".txt"; // global variable

// Tilføj data til en eksisterende datafil ("database") på SD-kort
void appendFile(fs::FS &fs, const char *path, const char *message)
{
	Serial.printf("Appending to file: %s\n", path);

	File file = fs.open(path, FILE_APPEND);
	if (!file)
	{
		Serial.println("Failed to open file for appending");
		return;
	}
	if (file.print(message))
	{
		Serial.println("Message appended");
	}
	else
	{
		Serial.println("Append failed");
	}
	file.close();
}

// Skriv til SD-kortet
void writeFile(fs::FS &fs, const char *path, const char *message)
{
	Serial.printf("Writing file: %s\n", path);

	File file = fs.open(path, FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return;
	}
	if (file.print(message))
	{
		Serial.println("File written");
	}
	else
	{
		Serial.println("Write failed");
	}
	file.close();
}

// Gem data på SD-kort
void saveDataToSDcard()
{
	dataMessage = String(activityType) + "," + String(timeData) + "," + String(hrData) + "," + String(eeData) + "," + String(stepData) + "\n";
	Serial.print("Save Data: ");
	Serial.println(dataMessage);
	appendFile(SD, fileName.c_str(), dataMessage.c_str());
}

uint8_t getSessionNumber()
{
	uint8_t i = 0;
	while (SD.exists("/data" + String(i) + ".txt"))
	{
		i++;
	}
	return i;
}

void isCardMounted()
{
	if (!SD.begin()isCardMounted)
	{
		Serial.println("Card Mount Failed");
		return;
	}
	uint8_t cardType = SD.cardType();

	if (cardType == CARD_NONE)
	{
		Serial.println("No SD card attached");
		return;
	}
}

void doesFileExist()
{
	dataNumber = getSessionNumber(); // sørger for, at dataNumber tælles op i takt med der orettes en ny session

	fileName = "/data" + String(dataNumber) + ".txt"; // opdater filnavnet så, der fås: /data + næste nummer +.txt (næste nummer = session nummer)

	File file = SD.open(fileName.c_str()); // Åben filen data(nummer). txt på SD-kortet

	// Hvis /data(nummer).txt (fileName) filen ikke eksisterer
	// Opret en fil på SD-kortet og skriv forskellige labels
	if (!file)
	{
		Serial.println("File doens't exist");
		Serial.println("Creating file...");
		writeFile(SD, fileName.c_str(), "EE,HR,Skridt,Aktivitet,Tid\r\n"); //  Vi kan altid ændre rækkefølgen på disse labels (EE,HR osv..)
		saveDataToSDcard();												   // Gem data på SD-kortet
	}

	else
	{

		Serial.println("File already exists");
		saveDataToSDcard(); // Gem data på SD-kortet
	}

	file.close();
}
// skal laves en funktion som sørger for, at ved stop af session stopper med at gemme og lukker filen.
// og sørger for, at dataNumber skifter, så der skrives (oprettes) til en ny datafil ved næste træningssession.
/*
 void stopDataSession(){
 if (BLE.stop())
 { gem og luk filen kode .:.:.}
 dataNumber++;
 }
*/
