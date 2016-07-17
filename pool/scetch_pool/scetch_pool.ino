
#include <UIPEthernet.h>
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
#define PHSensorPin 0



/*
      1-Wire
*/
OneWire  oneWireTemp(9);
DallasTemperature sensorTemp(&oneWireTemp);
DeviceAddress poolProbe = {0x28, 0xFF, 0x35, 0x3B, 0x93, 0x15, 0x03, 0xFF};
DeviceAddress plattenProbe ={0x28, 0xFF, 0x46, 0xFC, 0x4C, 0x04, 0x00, 0xEF};
/*
    Ethernet Shield
*/

const /*PROGMEM*/ uint8_t mac[] = {0x90, 0xFF, 0xFF, 0x00, 0xA1, 0xAE}; //Change 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
const /*PROGMEM*/ IPAddress ip (192, 168, 178, 100); //Adresse des Arduino
const /*PROGMEM*/ int localPort = 8888;      // local port to listen on

const /*PROGMEM*/ IPAddress loxone(192, 168, 178, 20);//Anpassen auf Loxone
const /*PROGMEM*/ int loxonePort = 7000; //Port der Loxone

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP udp;

Timer time;

//******************************Constants**************************

//*****************************Globale Variables***************************
byte relaisStatus = B00000000;  //Erststatus der Relais
int poolTemp, poolTempBuffer, plattenTemp, plattenTempBuffer = 0;  //Pool Temperatur als Int
//uint8_t sekunden = 600;
char onewire_pool_char[4];  //Pool Temperatur Char
char onewire_platten_char[4]; // Platten Temperatur char
bool firstRunTemp = true;  //FÜr den ersten durchgang
bool firstRunRelais = true;  //Für den ersten durchgang
bool tempSend = false;
bool relaisSend = false; //Zeigt an ob Wert gesendet wurde
bool tempChange = false; //Zeigt an wenn Temperatur sich geändert hat
bool relaisChange = false; //Zeigt an ob ein Relais geschaltet wurde
bool initUDPState = false;
bool lockState = false; // Soll verhindern, dass mehrfaches senden die Relays "irritiert"

/******************** SETUP *******************/
void setup() {
  //Start Serial-Monitor
  Serial.begin(9600);

  // start the Ethernet and UDP:
  Ethernet.begin(mac, ip);
  //Serial.println(("Ethernet initiated"));

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
  //Ändert den Status des Relais (An = 11 oder Aus = 10)
  if (/*bitRead(relaisStatus, relais) == LOW &&*/ relais != 0 && relais != 81 && relais != 80)
  {
    //Serial.println("LOW");
    
    // Schaltwert identifizieren
    int schaltwert = relais% 10;
    // Relais identifizieren
    int relaiswert = (relais / 10) % 10;
    
    if (schaltwert == 0)
    {
      bitWrite(relaisStatus, relaiswert, LOW);
    }
    else
    {
      bitWrite(relaisStatus, relaiswert, HIGH);
    }
      
    //bitWrite(relaisStatus, relais, HIGH); //Relais an
    relaisChange = true;
    relaisSend = false;
    bitChanged = true;
  }

  if (bitRead(relaisStatus, relais) == HIGH && relais != 0 && relais <= 8 && !bitChanged)
  {
    //Serial.println("HIGH");
    bitWrite(relaisStatus, relais, LOW); //Relais aus
    relaisChange = true;
    relaisSend = false;
  }
}

bool initUDP()
{
  int success;
  int ergebniss;

  //Serial.println(("Init UDP"));
  success = udp.beginPacket(loxone, loxonePort);
  //Serial.print(("Init beginPacket: "));
  //Serial.println(success ? "success" : "failed");
  if (!success)
  {
    udp.stop();
  }
  else
  {
    ergebniss = udp.write("Init");
    success = udp.endPacket();
    //Serial.print("Init endPacket: ");
    //Serial.println(success ? "success" : "failed");
  }

  if (ergebniss != 0)
  {
    udp.stop();
    return true;
  }

  udp.stop();
  return false;
}


//Sendet aktuelle Pool-Temperatur
bool sendPoolTemp()
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
  success = udp.beginPacket(loxone, loxonePort);
  //Serial.print(("Temp_Pool beginPacket: "));
  //Serial.println(success ? "success" : "failed");
  if (!success)
  {
    udp.stop();
    return false;
  }
  else
  {
    ergebniss = udp.write(buf1);
    success = udp.endPacket();
    udp.stop();
    //Serial.print(("Temp_Pool endPacket: "));
    //Serial.println(success ? "success" : "failed");
  }

  if (ergebniss != 0)
  {
    udp.stop();
    return true;
  }

  udp.stop();
  return false;
}

//Sendet aktuelle Platten-Temperatur
bool sendPlattenTemp()
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
  success = udp.beginPacket(loxone, loxonePort);
  //Serial.print(("Temp_Platten beginPacket: "));
  //Serial.println(success ? "success" : "failed");
  if (!success)
  {
    udp.stop();
    return false;
  }
  else
  {
    ergebniss = udp.write(buf1);
    success = udp.endPacket();
    udp.stop();
    //Serial.print(("Temp_Platten endPacket: "));
    //Serial.println(success ? "success" : "failed");
  }

  if (ergebniss != 0)
  {
    udp.stop();
    return true;
  }

  udp.stop();
  return false;
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
  success = udp.beginPacket(loxone, loxonePort);
  Serial.print(("Relais beginPacket: "));
  Serial.println(success ? "success" : "failed");
  if (!success)
  {
    udp.stop();
    return false;
  }
  else
  {
    ergebniss = udp.write(buf2);
    success = udp.endPacket();
    udp.stop();
    Serial.print(("Relais endPacket: "));
    Serial.println(success ? "success" : "failed");
  }

  if (ergebniss > 0)
  {
    udp.stop();
    lockState = false;
    return true;
  }

  udp.stop();
  return false;
}

// Testet ob eine Zahl gesedet wird, für das Schalten der Relais
int verify(char * string)
{
  int x = 0;
  int len = strlen(string);
  while (x < len) {
    if (!isdigit(*(string + x)))
      return 1;
    ++x;
  }

  return 0;
}

//Lesen des UDP Streams
void readUDP()
{
  udp.begin(localPort);
  int packetSize = udp.parsePacket();
  char packetBuffer[packetSize + 1];
  if (packetSize > 0)
  {
    udp.read(packetBuffer, packetSize + 1);
    //finish reading this packet:
    udp.flush();
    udp.stop();
    //Serial.print(("received: "));
    //Serial.println(packetBuffer);

    if (verify(packetBuffer) && !lockState)
    {
      int relais = atoi(packetBuffer);
      relais--;
      //passt den zähler an byte an
      //Serial.print("INT: ");
      //Serial.println(relais);
      relaisByteWriter(relais);
      relaisSwitch();
    }
  }
  else
  {
    udp.stop();
  }

  udp.stop();
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
    tempSend = false;
    //Serial.println(("Temp Change"));
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
  //relaisSwitch(); //Auslesen der Relais

  //UDP init
  if (!initUDPState)
  {
    bool successInitUDP = initUDP();
    if (successInitUDP)
      initUDPState = true;
  }

  //Senden der Temperatur
  if (((tempChange && !tempSend) || firstRunTemp) && initUDPState)
  {
    bool successPoolTemp = sendPoolTemp();
    bool successPlattenTemp = sendPlattenTemp();
    if (successPoolTemp && successPlattenTemp)
    {
      tempSend = true;
      if (firstRunTemp)
      {
        sendPoolTemp(); //extra zweimal senden damit startwert vorhanden ist
        sendPlattenTemp();
        firstRunTemp = false;
      }
    }
    else
    {
      tempSend = false;
    }
  }

  //Senden des RelaisStatus
  if (((relaisChange && !relaisSend) || firstRunRelais) && initUDPState)
  {
    bool successRelais = sendRelais();
    if (successRelais)
    {
      relaisSend = true;
      if (firstRunRelais)
      {
        sendRelais(); //extra zweimal senden damit startwert vorhanden ist
        firstRunRelais = false;
      }
    }
    else
    {
      relaisSend = false;
    }
  }

  readUDP(); //Lesen der Daten von Loxone
  time.update();
}
