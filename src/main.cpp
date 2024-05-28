#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AHTX0.h>
#include "ThingSpeak.h"
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_Fingerprint.h>

// MQTT Broker
const char *mqtt_broker = "51.20.105.27"; //
const char *topic = "mqtt/esp32";
const char *subtopic1 = "mqtt/receive";
const char *subtopic2 = "mqtt/lcd";
const char *mqtt_username = "userttpu";
const char *mqtt_password = "studentpass";
const int mqtt_port = 1884;

WiFiClient espClient;
PubSubClient client(espClient);

// thingspeak
WiFiClient client1;

unsigned long myChannelNumber = 2;
const char *myWriteAPIKey = "9X3LFQDD0RDACX82";

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

// AHT10
Adafruit_AHTX0 aht;

// rfid
#define SS_PIN 5
#define RST_PIN 4

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[4];
String card = "";

//fingerprint scaner
#define mySerial Serial2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
uint8_t id;
#define ENROL_MODE 0 // 0 or 1

//===========PINS==================
#define relayPIN 25
#define buttonPin 35
#define pirMotion 32
#define bOut 12
#define rOut 14

//======variables==========
String buffer = "";
bool doorFlag = 0;
unsigned long ttime;
bool lcdFlag = 0;
unsigned long ttimeLcd;
unsigned long lastTime = 110000;
unsigned long ttimeButton;
bool button;
bool buttonFlag = 0;



uint8_t readnumber(void)
{
  uint8_t num = 0;

  while (num == 0)
  {
    while (!Serial.available())
      ;
    num = Serial.parseInt();
  }
  return num;
}

uint8_t getFingerprintEnroll()
{

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Prints matched!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    Serial.println("Fingerprints did not match");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Stored!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not store in that location");
    return p;
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez()
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)
    return -1;

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  if (finger.fingerID != 0)
  {
    doorFlag = 1;
    digitalWrite(relayPIN, 1);
    ttime = millis();
  }
  return finger.fingerID;
}

void enrolFP()
{
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
  id = readnumber();
  if (id == 0)
  { // ID #0 not allowed, try again!
    return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);

  while (!getFingerprintEnroll())
    ;
}

void FPinit()
{
  while (!Serial)
    ; // For Yun/Leo/Micro/Zero/...
  delay(100);
  Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1);
    }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);
}

void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    card += String(buffer[i], HEX);
  }
  Serial.println(card);
  if (card.equals("e0c486e"))
  {
    doorFlag = 1;
    ttime = millis();
    digitalWrite(relayPIN, 1);
  }
}

void rfid_read()
{
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (rfid.PICC_IsNewCardPresent())
  {

    // Verify if the NUID has been readed
    if (rfid.PICC_ReadCardSerial())
    {

      Serial.print(F("PICC type: "));
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.println(rfid.PICC_GetTypeName(piccType));

      // Check is the PICC of Classic MIFARE type
      if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
          piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
          piccType != MFRC522::PICC_TYPE_MIFARE_4K)
      {
        Serial.println(F("Your tag is not of type MIFARE Classic."));
        return;
      }

      // if (rfid.uid.uidByte[0] != nuidPICC[0] ||
      //   rfid.uid.uidByte[1] != nuidPICC[1] ||
      //   rfid.uid.uidByte[2] != nuidPICC[2] ||
      //   rfid.uid.uidByte[3] != nuidPICC[3] )

      Serial.println(F("card has been detected."));

      // Store NUID into nuidPICC array
      for (byte i = 0; i < 4; i++)
      {
        nuidPICC[i] = rfid.uid.uidByte[i];
      }

      Serial.println(F("The NUID tag is:"));
      Serial.print(F("In dec: "));
      printDec(rfid.uid.uidByte, rfid.uid.size);
      Serial.println();
      card = "";

      // Halt PICC
      rfid.PICC_HaltA();

      // Stop encryption on PCD
      rfid.PCD_StopCrypto1();
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  buffer = "";
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    buffer += (char)payload[i];
  }

  Serial.println();
  Serial.println("-----------------------");
  StaticJsonDocument<200> doc;
  if (strcmp(topic, subtopic1) == 0)
  {

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, buffer);

    // Test if parsing succeeds
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Extract values
    String door = doc["door"];

    // Print values
    Serial.println(door);

    if (door.equals("open"))
    {
      doorFlag = 1;
      digitalWrite(relayPIN, 1);
      ttime = millis();
    }
  }
  else if (strcmp(topic, subtopic2) == 0)
  {

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, buffer);

    // Test if parsing succeeds
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Extract values
    const char *msg = doc["msg"];

    // Print values
    Serial.println(msg);
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print(String(msg));
    ttimeLcd = millis();
    lcdFlag = 1;
  }
}

void openDoor()
{
  if (doorFlag and millis() - ttime >= 5000)
  {
    doorFlag = 0;
    digitalWrite(relayPIN, 0);
  }
}
void thing_speak()
{
  if ((millis() - lastTime) > 120000)
  {

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data
    Serial.print("Temperature: ");
    Serial.print(temp.temperature);
    Serial.println(" degrees C");
    Serial.print("Humidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println("% rH");

    // uncomment if you want to get temperature in Fahrenheit
    /*temperatureF = 1.8 * bme.readTemperature() + 32;
    Serial.print("Temperature (ºC): ");
    Serial.println(temperatureF);*/

    // set the fields with the values
    ThingSpeak.setField(1, temp.temperature);
    // ThingSpeak.setField(1, temperatureF);
    ThingSpeak.setField(2, humidity.relative_humidity);

    // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
    // pieces of information in a channel.  Here, we write to field 1.
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if (x == 200)
    {
      Serial.println("Channel update successful.");
    }
    else
    {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    lastTime = millis();
  }
}

void setup() //================================== SETUP ============================
{
  Serial.begin(115200);
  delay(10);

  ThingSpeak.begin(client1); // Initialize ThingSpeak

  // initialize the lcd
  lcd.init();
  lcd.backlight();

  // initialize wifi manager
  WiFiManager wifiManager;

  if (!wifiManager.autoConnect("esp32noCam", "esp32nocam"))
  {
    Serial.println("failed to connect and hit timeout");
    // reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected())
  {
    String client_id = "esp32-client";
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Public EMQX MQTT broker connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  // Publish and subscribe
  client.subscribe(subtopic1);
  client.subscribe(subtopic2);

  // initialize aht10
  if (!aht.begin())
  {
    Serial.println("Could not find AHT? Check wiring");
    while (1)
      delay(10);
  }
  Serial.println("AHT10 or AHT20 found");

  // initialize rfid
  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  Serial.println(F("This code scan the MIFARE Classsic NUID."));

  // fingerprint scaner
  finger.getTemplateCount();

  if (finger.templateCount == 0)
  {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else
  {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }

  pinMode(relayPIN, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(pirMotion, INPUT);
  pinMode(bOut, OUTPUT);
}

void loop()
{
  // put your main code here, to run repeatedly:
  button = !digitalRead(buttonPin);
  if(button and !buttonFlag){
    buttonFlag = 1;
    ttimeButton = millis();
    digitalWrite(bOut, 1);
    Serial.println("button is pressed");
  }
  if(!button and buttonFlag and millis() - ttimeButton >= 2000){
    digitalWrite(bOut, 0);
    buttonFlag = 0;
  }

  if (ENROL_MODE)
  {
    enrolFP();
  }
  else
  {
    getFingerprintIDez();
  }
  rfid_read();

  client.loop();
  openDoor();
  if (millis() - ttimeLcd >= 10000 & lcdFlag == 1)
  {
    lcdFlag = 0;
    lcd.clear();
  }
}