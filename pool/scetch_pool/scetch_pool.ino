#include <UIPEthernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimerOne.h>

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
  VIN         5V    orangenes Kabel

  Relais

  PIN 1       Port 1  //Unbelegt wegen Serial Monitor
  PIN 2       Port 2
  PIN 3       Port 3
  PIN 4       Port 4
  PIN 5       Port 5
  PIN 6       Port 6
  PIN 7       Port 7
  PIN 8       Port 8
*/

/*
   *************************DECLARATIONS****************
*/

/*
      1-Wire
*/
OneWire  oneWireTemp(9);
DallasTemperature sensorTemp(&oneWireTemp);

/*
    Ethernet Shield
*/

uint8_t mac[] = {0x90, 0xFF, 0xFF, 0x00, 0xA1, 0xAE}; //Change 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
//static byte gwip[] = {192, 168, 177, 1}; //Gateway adresse
IPAddress ip (192, 168, 177, 178);
static int localPort = 8888;      // local port to listen on

IPAddress loxone(192, 168, 177, 13);//Anpassen auf Loxone
static int loxonePort = 7000;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP udp;



//******************************Constants**************************

//*****************************Globale Variables***************************
byte relaisStatus = B00000000;
int poolTemp = 0;
int poolTempBuffer = 0;
int sekunden = 600;
char onewire_pool_char[4];
bool firstRunTemp = true;
bool firstRunRelais = true;
bool tempSend = false;
bool relaisSend = false;
bool tempChange = false;
bool relaisChange = false;
bool initUDPState = false;





void setup() {
  //Start Serial-Monitor
  Serial.begin(9600);

  // start the Ethernet and UDP:
  Ethernet.begin(mac, ip);
  Serial.println("Ethernet initiated");

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

  // 1-Wire
  sensorTemp.begin();

  //Timer
  Timer1.initialize(sekunden * 1000000);
  Timer1.attachInterrupt(timer);
}

//*****************************Helpers*********************

void timer()
{
	Serial.println("Timer");
	sendTemp();
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

  Serial.println("Init UDP");
  success = udp.beginPacket(loxone, loxonePort);
  Serial.print("Init beginPacket: ");
  Serial.println(success ? "success" : "failed");
  if (!success)
  {
    udp.stop();
  }
  else
  {
    ergebniss = udp.write("Init");
    success = udp.endPacket();
    Serial.print("Init endPacket: ");
    Serial.println(success ? "success" : "failed");
  }

  if (ergebniss != 0)
  {
    udp.stop();
    return true;
  }

  udp.stop();
  return false;
}


//Sendet aktuelle Temperatur
bool sendTemp()
{
  int success;
  int laenge;
  int ergebniss;

  Serial.println("UDP_Temp");
  String temp = "Temp_";
  temp.concat(onewire_pool_char);
  laenge = temp.length() + 1;
  char buf1[laenge];
  temp.toCharArray(buf1, laenge);
  success = udp.beginPacket(loxone, loxonePort);
  Serial.print("Temp beginPacket: ");
  Serial.println(success ? "success" : "failed");
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
    Serial.print("Temp endPacket: ");
    Serial.println(success ? "success" : "failed");
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
  int success;
  int laenge;
  int ergebniss;

  Serial.println("UDP_Relais");
  String udprelais = "Relais_";
  udprelais.concat(relaisStatus);
  laenge = udprelais.length() + 1;
  char buf2[laenge];
  udprelais.toCharArray(buf2, laenge);
  success = udp.beginPacket(loxone, loxonePort);
  Serial.print("Relais beginPacket: ");
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
    Serial.print("Relais endPacket: ");
    Serial.println(success ? "success" : "failed");
  }

  if (ergebniss > 0)
  {
    udp.stop();
    return true;
  }

  udp.stop();
  return false;
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
    Serial.print("received: ");
    Serial.println(packetBuffer);
    int relais = atoi(packetBuffer);
    relais--;
    //passt den zähler an byte an
    Serial.print("INT: ");
    Serial.println(relais);
    relaisByteWriter(relais);
    relaisSwitch();
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
  sensorTemp.requestTemperatures(); // Temperatur 1Wire-Sensor
  float onewire_pool_float = sensorTemp.getTempCByIndex(0); //  1Wire-Sensor (im Pool)
  int onewire_pool_int = (int)(onewire_pool_float + .5); // Float in Integer wandeln mit kaufmännischer Rundung
  poolTemp = onewire_pool_int; //Gloabs setzen
  sprintf(onewire_pool_char, "%d", onewire_pool_int); // Integer in String wandeln
  if (poolTempBuffer != poolTemp)
  {
    poolTempBuffer = poolTemp;
    tempChange = true;
    tempSend = false;
    Serial.println("Temp Change");
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
    bool successTemp = sendTemp();
    if (successTemp)
    {
      tempSend = true;
      if (firstRunTemp)
      {
        sendTemp(); //extra zweimal senden damit startwert vorhanden ist
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

}
