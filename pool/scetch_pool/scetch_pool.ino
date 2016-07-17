#include <EtherCard.h>
#include <IPAddress.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Timer.h>

/*
   ******************PINBELEGUNG********************

  Ethernet Shield

  PIN  12      MISO Ethernet
  PIN  11      MOSI Ethernet
  PIN  13      SCK Ethernet
  PIN  10      Hardware SPI Ethernet (outputmode)
  3v3          VCC
  GND          GND

  1-Wire
  PIN  9      LCK Latch Innensensor       (use pullup 4,5K) Gelbes Kabel
  GND         GND   weißes Kabel
  VCC         5V    orangenes Kabel

  Relais

  PIN 1       Port 1  //Unbelegt wegen Serial Monitor
  PIN 2       Port 2
  PIN 3       Port 3
  PIN 4       Port 4
  PIN 5       Port 5
  PIN 6       Port 6
  PIN 7       Port 7
  PIN 8       Port 8

  PH-Sensor
  GND        GND
  VCC        5V
  Data       Pin A0

*/

/*
   *************************DECLARATIONS****************
*/

/*    PH-Sensor */
//#define PHSensorPin 0



/*
      1-Wire
*/
OneWire  oneWireTemp(9);  //Port des oneWire
DallasTemperature sensorTemp(&oneWireTemp);
DeviceAddress poolProbe = {0x28, 0xFF, 0x35, 0x3B, 0x93, 0x15, 0x03, 0xFF}; //Adresse des Pool Sensors
DeviceAddress plattenProbe = {0x28, 0xFF, 0x46, 0xFC, 0x4C, 0x04, 0x00, 0xEF}; //Adresse des Platten Sensosrs

/*
    Ethernet Shield
*/

////////////////////

static byte myip[] = { 192, 168, 178, 100 }; //Adresse des Arduino
// gateway ip address
static byte gwip[] = { 192, 168, 178, 1 }; //Gateway und DNS
static byte mymac[] = { 0x90, 0xFF, 0xFF, 0x00, 0xA1, 0xAE };
byte Ethernet::buffer[700]; //Ethernet Buffer
static byte loxone[] = { 192, 168, 178, 20 }; //Anpassen auf Loxone
const int localPort = 8888;      // local port to listen on
const int loxonePort = 7000; //Port der Loxone

///////////////////////


Timer time;

//******************************Constants**************************

//*****************************Globale Variables***************************
byte relaisStatus = B00000000;  //Erststatus der Relais
int poolTemp, poolTempBuffer, plattenTemp, plattenTempBuffer = 0;  //Pool Temperatur als Int

char onewire_pool_char[4];  //Pool Temperatur Char
char onewire_platten_char[4]; // Platten Temperatur char
bool tempChange = false; //Zeigt an wenn Temperatur sich geändert hat
bool relaisChange = false; //Zeigt an ob ein Relais geschaltet wurde

/******************** SETUP *******************/
void setup() {
  //Start Serial-Monitor
  Serial.begin(9600);

  // start the Ethernet and UDP:
  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0)
  {
    Serial.println(F("Failed to access Ethernet controller"));
    }
  else
  {
    Serial.println(("Ethernet initiated"));
  }
  
  ether.staticSetup(myip, gwip, gwip);
  ether.udpServerListenOnPort(&readUDP, localPort);

  // Setzen der Digitalen Ausgänge
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  //pinMode(1, OUTPUT);     //Wird für RS232 verwendet Monitor PC
  //digitalWrite(1,HIGH);

  // 1-Wire starten
  sensorTemp.begin();

  //Timer
  time.every(60000, timerEvent);
}

//*****************************Helpers*********************


void timerEvent()
{
  //Serial.println(("Timer"));
  sendPoolTemp();
  sendPlattenTemp();
  sendRelais();
}

//Schalten der Relais anhan des Byte-Wertes (Byte-Stelle 0= arduino Port 1 Byte-Stelle 7 = Arduino Port 8)
void relaisSwitch()
{
  for (int i = 0; i < 8; i++)
  {
    bool theBit = bitRead(relaisStatus, i);
    //Serial.print(theBit);
    int relaisAusgang = i + 1; //Anpassung an Arduino Port
    if (theBit == true && i != 0) //Damit Port 2 nicht verwendet wird
    {
      digitalWrite(relaisAusgang, LOW);
    } else {
      digitalWrite(relaisAusgang, HIGH);
    }
  }
}

//Schreiben des Bytes der Relais
//Relais sind als Byte hinterlegt (8Bit)
void relaisByteWriter(int relais) //Werte von 0-7 entsprechen 1-8 der Relais
{
  //Serial.println(relais);
  bool bitChanged = false;
  //Ändert den Status des Relais (An = 1 oder Aus = 0)
  if (bitRead(relaisStatus, relais) == LOW && relais != 0 && relais <= 8)
  {
    //Serial.println("LOW");
    bitWrite(relaisStatus, relais, HIGH); //Relais an
    relaisChange = true;
    bitChanged = true;
  }

  if (bitRead(relaisStatus, relais) == HIGH && relais != 0 && relais <= 8 && !bitChanged)
  {
    //Serial.println("HIGH");
    bitWrite(relaisStatus, relais, LOW); //Relais aus
    relaisChange = true;
  }
}


//Sendet aktuelle Pool-Temperatur
void sendPoolTemp()
{
  int success;
  int laenge;
  int ergebniss;

  //Serial.println(("UDP_Temp_Pool"));
  String temp = "PoolTemp_";
  temp.concat(onewire_pool_char);
  laenge = temp.length() + 1;
  char buf1[laenge];
  temp.toCharArray(buf1, laenge);
  //Serial.print("Send Temp: ");
  Serial.println(buf1);
  
  ether.sendUdp(buf1, sizeof(buf1), localPort, loxone, loxonePort);
}

//Sendet aktuelle Platten-Temperatur
void sendPlattenTemp()
{
  int success;
  int laenge;
  int ergebniss;

  //Serial.println(("UDP_Temp_Platten"));
  String temp = "PlattenTemp_";
  temp.concat(onewire_platten_char);
  laenge = temp.length() + 1;
  char buf1[laenge];
  temp.toCharArray(buf1, laenge);
  Serial.println(buf1);
  
  ether.sendUdp(buf1, sizeof(buf1), localPort, loxone, loxonePort);
}

//Sendet Relais Status
bool sendRelais()
{
  bool success;
  int laenge;
  int ergebniss;

  Serial.println(("UDP_Relais"));
  String udprelais = "Relais_";
  udprelais.concat(relaisStatus);
  laenge = udprelais.length() + 1;
  char buf2[laenge];
  udprelais.toCharArray(buf2, laenge);
  Serial.println(buf2);
  
  ether.sendUdp(buf2, sizeof(buf2), localPort, loxone, loxonePort);
}

// Testet ob eine Zahl empfangen wurde, für das Schalten der Relais
//String Test INT
bool verifyString(const String & s/*const char * string*/)
{
   if(((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;
   char * p ;
   strtol(s.c_str(), &p, 10) ;
   
   return (*p == 0) ;
}

//Lesen des UDP Streams
void readUDP(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port,const char *data, uint16_t len){
  Serial.print(("received: "));
  Serial.println(data);
  
  bool verify = verifyString(data);
  Serial.print("Verify: ");
  Serial.println(verify);
  if (verifyString(data) /*&& !lockState*/)
  {
    int relais = atoi(data);
    relais--;
    //passt den zähler an byte an
    Serial.print("INT: ");
    Serial.println(relais);
    relaisByteWriter(relais);
    relaisSwitch();
  }
}

//Lesen der Temperatur
void readTemperatur()
{
  sensorTemp.requestTemperatures(); // Temperatur 1Wire-Sensoren
  //  1Wire-Sensor (im Pool)
  float onewire_pool_float = sensorTemp.getTempC(poolProbe);
  int onewire_pool_int = (int)(onewire_pool_float + .5); // Float in Integer wandeln mit kaufmännischer Rundung
  poolTemp = onewire_pool_int; //Gloabs setzen
  sprintf(onewire_pool_char, "%d", onewire_pool_int); // Integer in String wandeln

  float onewire_platten_float = sensorTemp.getTempC(plattenProbe);
  int onewire_platten_int = (int)(onewire_platten_float + .5); // Float in Integer wandeln mit kaufmännischer Rundung
  plattenTemp = onewire_platten_int; //Gloabs setzen
  sprintf(onewire_platten_char, "%d", onewire_platten_int);

  if ((poolTempBuffer != poolTemp) || (plattenTempBuffer != plattenTemp))
  {
    poolTempBuffer = poolTemp;
    plattenTempBuffer = plattenTemp;
    tempChange = true;
  }
  else
  {
    tempChange = false;
  }
}

/******************************** Loop ***********************************/

void loop()
{
  readTemperatur(); //Auslesen der Temperatur
 
  //Senden der Temperatur
  if (tempChange)
  {
    sendPoolTemp();
    sendPlattenTemp();
    tempChange = false;
  }

  //Senden des RelaisStatus
  if (relaisChange)
  {
    sendRelais();
    relaisChange = false;
  }

  time.update();
  //this must be called for ethercard functions to work.
  ether.packetLoop(ether.packetReceive());
}
