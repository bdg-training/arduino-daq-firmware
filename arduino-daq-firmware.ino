
// https://github.com/bdg-training/arduino-daq-firmware

#include <SPI.h>
#include <Ethernet.h>

// 115200 baud

// nc -v 192.168.1.177 1000
// command: help
// command: *idn?
// <Manufacturer>,<Model>,<Serial Number>,<Firmware Level>,<Options>
#define IDN "BDG GmbH,arduino-daq-firmware,2,20250312"

/*
  Arduino Nano

  D13 = LED_BUILTIN
  D18/A4 = I2C-SDA
  D19/A5 = I2C-SCL


  Keyestudio W5500

  Serial communication D0(RX),D1(TX)
  External interruption D2(interrupt 0),D3(interrupt 1)
  PWM interface D3,D5,D6,D9,D10,D11
  SPI communication D10(SS),D11(MOSI),D12(MISO),D13(SCK)
  IIC communication A4(SDA),A5(SCL)
  Analog interface A0(D14),A1(D15),A2(D16),A3(D17),A4(D18),A5(D19)
*/

// pin config
// Nano IO-Box
const int DIS[] = {2, 4, 7, 8};
// false=floating
// true=pull up
const bool DIS_PULL_UPS[] = {false, false, false, false};
const int AIS[] = {A0, A1, A6, A7};
const int DOS[] = {10, 11, 12, 16};
// PWM-Outputs
const int AOS[] = {3, 5, 6, 9};
#define ETHERNET false

// CB2025
/*const int DIS[] = {};
  // false=floating
  // true=pull up
  const bool DIS_PULL_UPS[] = {};
  const int AIS[] = {};
  const int DOS[] = {};
  // PWM-Outputs
  const int AOS[] = {3};
  #define ETHERNET true*/

int readMillisDi = 50;
int readMillisAi = 0;
bool onChange = true;
#define TIMESTAMP false

byte MAC[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress IP(192, 168, 1, 177);
IPAddress SUBNET(255, 255, 0, 0);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress DNS(192, 168, 1, 1);
#define PORT 1000
EthernetServer server(PORT);

#define LF 10
#define CR 13
#define RX_SIZE  64
byte RxIndex = 0;
char RxBuffer[RX_SIZE + 1];

#define CMD_IDN   "*idn?"
#define CMD_HELP  "help"
#define CMD_DI_INTERVAL "\"diinterval\":"
#define CMD_AI_INTERVAL "\"aiinterval\":"
#define CMD_READ  "\"read\":\""
#define CMD_DO    "\"do\":["
#define CMD_AO    "\"ao\":["
#define CMD_ON_CHANGE  "\"onchange\":"

unsigned long delayInfo = 0;
unsigned long delayDi = 0;
unsigned long delayAi = 0;
String oldDiState = "";
String oldAiState = "";
String temp;
int aosValues[sizeof(AOS) / sizeof(int)];
EthernetClient client;
// 0=disconnected
// 1=serial
// 2=tcp
byte connection;

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  delay(50);
  Serial.begin(115200);
  //Serial.println(" ");
  //Serial.println(IDN);

  for (byte i = 0; i < sizeof(DIS) / sizeof(int); i++) {
    pinMode(DIS[i], INPUT);

    /*Serial.print("DI: ");
      Serial.println(DIS[i]);*/

    if (i < sizeof(DIS_PULL_UPS) / sizeof(bool))
    {
      if (DIS_PULL_UPS[i] == true)
      {
        pinMode(DIS[i], INPUT_PULLUP);
      }
    }
  }

  for (byte i = 0; i < sizeof(DOS) / sizeof(int); i++) {
    pinMode(DOS[i], OUTPUT);

    /*Serial.print("DO: ");
      Serial.println(DOS[i]);*/
  }

  /*for (byte i = 0; i < sizeof(AIS) / sizeof(int); i++) {
    Serial.print("AI: ");
    Serial.println(AIS[i]);
    }*/

  for (byte i = 0; i < sizeof(AOS) / sizeof(int); i++) {
    pinMode(AOS[i], OUTPUT);

    /*Serial.print("AO: ");
      Serial.println(AOS[i]);*/
  }

  oldDiState = GetDis();
  oldAiState = GetAis();

  if (ETHERNET)
  {
    // You can use Ethernet.init(pin) to configure the CS pin
    Ethernet.init(10);  // Most Arduino shields
    // start the Ethernet connection and the server:
    Ethernet.begin(MAC, IP, DNS, GATEWAY, SUBNET);
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.");
      while (true) {
        delay(1);
      }
    }

    // start the server
    server.begin();
  }
  else
  {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  }
}


void loop() {
  CheckRxData();

  if (onChange == false)
  {
    oldDiState = "";
    oldAiState = "";
  }

  if (readMillisDi > 0 && millis() - delayDi >= readMillisDi)
  {
    delayDi = millis();
    temp = GetDis();

    if (temp != oldDiState)
    {
      oldDiState = temp;

      switch (GetConnection())
      {
        case 1:
          Serial.println(temp);
          break;

        case 2:
          client.println(temp);
          break;
      }
    }
  }

  if (readMillisAi > 0 && millis() - delayAi >= readMillisAi)
  {
    delayAi = millis();
    temp = GetAis();

    if (temp != oldAiState)
    {
      oldAiState = temp;

      switch (GetConnection())
      {
        case 1:
          Serial.println(temp);
          break;

        case 2:
          client.println(temp);
          break;
      }
    }
  }

  if (ETHERNET)
  {
    if (millis() - delayInfo >= 2000)
    {
      delayInfo = millis();

      if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
      }
    }
  }
  else if (millis() - delayInfo >= 500)
  {
    delayInfo = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}


void CheckRxData()
{
  if (RxIndex >= RX_SIZE)
  {
    RxIndex = 0;
  }

  byte rx;

  switch (GetConnection())
  {
    case 0:
      return;

    case 1:
      if (Serial.available() > 0)
      {
        rx = Serial.read();
      }
      else
      {
        return;
      }
      break;

    case 2:
      if (client.available())
      {
        rx = client.read();
      }
      else
      {
        return;
      }
      break;
  }

  if (RxIndex == 0 && (rx == LF || rx == CR))
  {
    return;
  }

  if (rx == LF || rx == CR)
  {
    RxBuffer[RxIndex++] = 0;

    if (ETHERNET)
    {
      digitalWrite(LED_BUILTIN, HIGH);
    }

    OnDataReceived();
    delayInfo = millis();
    RxIndex = 0;
  }
  else
  {
    RxBuffer[RxIndex++] = rx;
  }
}


void OnDataReceived()
{
  String json = RxBuffer;
  json.replace(" ", "");
  json.toLowerCase();

  //Serial.println(json);

  if (json.startsWith("{"))
  {
    json.remove(0, 1);
  }

  int loopCnt = 0;

  do
  {
    if (json.startsWith(","))
    {
      json.remove(0, 1);
    }


    if (json.startsWith(CMD_IDN))
    {
      switch (GetConnection())
      {
        case 1:
          Serial.println(IDN);
          break;

        case 2:
          client.println(IDN);
          break;
      }

      return;
    }
    else if (json.startsWith(CMD_HELP))
    {
      switch (GetConnection())
      {
        case 1:
          Serial.print("--- ");
          Serial.print(CMD_HELP);
          Serial.println(" ---");

          if (ETHERNET)
          {
            Serial.print("IP: ");
            Serial.print(Ethernet.localIP());
            Serial.print(":");
            Serial.println(PORT);

            Serial.print("Subnet: ");
            Serial.println(Ethernet.subnetMask());
            Serial.println("");
          }

          Serial.print("*idn?\t");
          Serial.println(IDN);

          Serial.println("{\"read\":\"di\"}         (di=false/true, do=false/true, ai=0..1023, ao=0..255)");
          Serial.println("{\"diInterval\":0}      (0[ms]=off)");
          Serial.println("{\"aiInterval\":0}      (0[ms]=off)");
          Serial.println("{\"onChange\":true}     (false=send every ms, true=send on change)");
          Serial.println("{\"do\":[1,0,0,0]}      (-1=keep state, 0=off, 1=on, 2=toggle)");
          Serial.println("{\"ao\":[255,0,0,0]}    (-1=keep state, 0=off, 255=on)");
          Serial.println("------------");
          break;

        case 2:
          client.print("--- ");
          client.print(CMD_HELP);
          client.println(" ---");

          if (ETHERNET)
          {
            client.print("IP: ");
            client.print(Ethernet.localIP());
            client.print(":");
            client.println(PORT);

            client.print("Subnet: ");
            client.println(Ethernet.subnetMask());
            client.println("");
          }

          client.print("*idn?\t");
          client.println(IDN);

          client.println("{\"read\":\"di\"}         (di=false/true, do=false/true, ai=0..1023, ao=0..255)");
          client.println("{\"diInterval\":0}      (0[ms]=off)");
          client.println("{\"aiInterval\":0}      (0[ms]=off)");
          client.println("{\"onChange\":true}     (false=send every ms, true=send on change)");
          client.println("{\"do\":[1,0,0,0]}      (-1=keep state, 0=off, 1=on, 2=toggle)");
          client.println("{\"ao\":[255,0,0,0]}    (-1=keep state, 0=off, 255=on)");
          client.println("------------");
          break;
      }

      return;
    }

    if (json.startsWith(CMD_DI_INTERVAL))
    {
      json = ConfigDiInterval(json);
    }

    if (json.startsWith(CMD_AI_INTERVAL))
    {
      json = ConfigAiInterval(json);
    }

    if (json.startsWith(CMD_ON_CHANGE))
    {
      json = ConfigOnChange(json);
    }

    if (json.startsWith(CMD_READ))
    {
      json = ReadInputs(json);
    }

    if (json.startsWith(CMD_DO))
    {
      json = SetDos(json);
    }

    if (json.startsWith(CMD_AO))
    {
      json = SetAos(json);
    }


    if (json.startsWith("}"))
    {
      return;
    }

    //Serial.println(json);

    if (loopCnt > 20)
    {
      // extended json
      WriteStatus("error unsupported json node");
      return;
    }

    loopCnt++;

  } while (json != "");
}


String ConfigDiInterval(String json)
{
  json.replace(CMD_DI_INTERVAL, "");

  String value = "";
  bool found = false;

  for (int i = 0; i < json.length(); i++ )
  {
    if (json[i] == '}' || json[i] == ',' || found)
    {
      found = true;
      continue;
    }

    value += json[i];
  }

  readMillisDi = value.toInt();

  if (readMillisDi < 0)
  {
    readMillisDi = 0;
  }

  json.remove(0, value.length());

  if (json.startsWith("}"))
  {
    // end of json
    WriteStatus("ok");
  }

  return json;
}


String ConfigAiInterval(String json)
{
  json.replace(CMD_AI_INTERVAL, "");

  String value = "";
  bool found = false;

  for (int i = 0; i < json.length(); i++ )
  {
    if (json[i] == '}' || json[i] == ',' || found)
    {
      found = true;
      continue;
    }

    value += json[i];
  }

  readMillisAi = value.toInt();

  if (readMillisAi < 0)
  {
    readMillisAi = 0;
  }

  json.remove(0, value.length());

  if (json.startsWith("}"))
  {
    // end of json
    WriteStatus("ok");
  }

  return json;
}


String ConfigOnChange(String json)
{
  json.replace(CMD_ON_CHANGE, "");

  if (json.startsWith("true"))
  {
    onChange = true;
    json.remove(0, 4);

    oldDiState = GetDis();
    oldAiState = GetAis();
  }
  else if (json.startsWith("false"))
  {
    onChange = false;
    json.remove(0, 5);
  }
  else
  {
    WriteStatus("error");
    return "";
  }

  if (json.startsWith("}"))
  {
    // end of json
    WriteStatus("ok");
  }

  return json;
}


String ReadInputs(String json)
{
  json.replace(CMD_READ, "");

  if (json.startsWith("di"))
  {
    oldDiState = GetDis();

    switch (GetConnection())
    {
      case 1:
        Serial.println(oldDiState);
        break;

      case 2:
        client.println(oldDiState);
        break;
    }
  }
  else if (json.startsWith("do"))
  {
    switch (GetConnection())
    {
      case 1:
        Serial.println(GetDos());
        break;

      case 2:
        client.println(GetDos());
        break;
    }
  }
  else if (json.startsWith("ai"))
  {
    oldAiState = GetAis();

    switch (GetConnection())
    {
      case 1:
        Serial.println(oldAiState);
        break;

      case 2:
        client.println(oldAiState);
        break;
    }
  }
  else if (json.startsWith("ao"))
  {
    switch (GetConnection())
    {
      case 1:
        Serial.println(GetAos());
        break;

      case 2:
        client.println(GetAos());
        break;
    }
  }
  else
  {
    WriteStatus("error");
    return "";
  }

  // end of json
  return "}";
}


String GetDis()
{
  String json = "{\"di\":[";

  for (byte i = 0; i < sizeof(DIS) / sizeof(int); i++) {

    if (digitalRead(DIS[i]))
    {
      json += "true,";
    }
    else
    {
      json += "false,";
    }
  }

  if (json.endsWith(","))
  {
    json.remove(json.length() - 1, 1);
  }

  if (TIMESTAMP)
  {
    json += "]," + GetTime() + "}";
  }
  else
  {
    json += "]}";
  }

  //Serial.println(json);
  return json;
}


String GetDos()
{
  String json = "{\"do\":[";

  for (byte i = 0; i < sizeof(DOS) / sizeof(int); i++) {

    if (digitalRead(DOS[i]))
    {
      json += "true,";
    }
    else
    {
      json += "false,";
    }
  }

  if (json.endsWith(","))
  {
    json.remove(json.length() - 1, 1);
  }

  if (TIMESTAMP)
  {
    json += "]," + GetTime() + "}";
  }
  else
  {
    json += "]}";
  }

  //Serial.println(json);
  return json;
}


String GetAis()
{
  String json = "{\"ai\":[";

  for (byte i = 0; i < sizeof(AIS) / sizeof(int); i++) {

    json += analogRead(AIS[i]);
    json += ",";
  }

  if (json.endsWith(","))
  {
    json.remove(json.length() - 1, 1);
  }

  if (TIMESTAMP)
  {
    json += "]," + GetTime() + "}";
  }
  else
  {
    json += "]}";
  }

  //Serial.println(json);
  return json;
}


String GetAos()
{
  String json = "{\"ao\":[";

  for (byte i = 0; i < sizeof(aosValues) / sizeof(int); i++) {

    json += aosValues[i];
    json += ",";
  }

  if (json.endsWith(","))
  {
    json.remove(json.length() - 1, 1);
  }

  if (TIMESTAMP)
  {
    json += "]," + GetTime() + "}";
  }
  else
  {
    json += "]}";
  }

  //Serial.println(json);
  return json;
}


String SetDos(String json)
{
  json.replace(CMD_DO, "");

  int no = 0;

  do
  {
    if (no >= sizeof(DOS) / sizeof(int))
    {
      WriteStatus("error invalid length");
      return "";
    }

    int port = DOS[no];

    /*Serial.print("do");
      Serial.print(no);
      Serial.print("(port");
      Serial.print(port);
      Serial.print(")");*/

    if (json.startsWith("-1"))
    {
      //Serial.println("=keep");
      json.remove(0, 2);
    }
    else if (json.startsWith("1"))
    {
      //Serial.println("=true");
      digitalWrite(port, true);
      json.remove(0, 1);
    }
    else if (json.startsWith("2"))
    {
      //Serial.println("=toggle");
      digitalWrite(port, !digitalRead(port));
      json.remove(0, 1);
    }
    else
    {
      //Serial.println("=false");
      digitalWrite(port, false);
      json.remove(0, 1);
    }

    if (json.startsWith(","))
    {
      json.remove(0, 1);
    }

    no++;

  } while (json.startsWith("]") == false);

  // remove ]
  json.remove(0, 1);

  if (json.startsWith("}"))
  {
    // end of json
    WriteStatus("ok");
  }

  return json;
}


String SetAos(String json)
{
  json.replace(CMD_AO, "");

  int no = 0;

  do
  {
    if (no >= sizeof(AOS) / sizeof(int))
    {
      WriteStatus("error invalid length");
      return "";
    }

    int port = AOS[no];

    /*Serial.print("ao");
      Serial.print(no);
      Serial.print("(port");
      Serial.print(port);
      Serial.print(")");*/

    String value = "";
    bool found = false;

    for (int i = 0; i < json.length(); i++ )
    {
      if (json[i] == ',' || json[i] == ']' || found)
      {
        found = true;
        continue;
      }

      value += json[i];
    }

    json.remove(0, value.length());

    if (json.startsWith(","))
    {
      json.remove(0, 1);
    }

    int pwm = value.toInt();
    bool ignore = false;

    if (pwm < 0)
    {
      ignore = true;
    }
    else if (pwm > 255)
    {
      pwm = 255;
    }

    if (ignore == false)
    {
      //Serial.println(pwm);
      analogWrite(port, pwm);
      aosValues[no] = pwm;
    }

    no++;

  } while (json.startsWith("]") == false);

  // remove ]
  json.remove(0, 1);

  if (json.startsWith("}"))
  {
    // end of json
    WriteStatus("ok");
  }

  return json;
}


String GetTime()
{
  String ret = "\"time\":";
  ret += millis();
  return ret;
}


void WriteStatus(String text)
{
  switch (GetConnection())
  {
    case 1:
      Serial.print("{\"status\":\"");
      Serial.print(text);
      Serial.println("\"}");
      break;

    case 2:
      client.print("{\"status\":\"");
      client.print(text);
      client.println("\"}");
      break;
  }
}


byte GetConnection()
{
  if (connection != 2) {
    client = server.available();

    if (client)
    {
      connection = 2;
      //Serial.println("tcp client connected");
    }
  }

  switch (connection)
  {
    case 0:
      if (Serial.available() > 0)
      {
        connection = 1;
        //Serial.println("serial client connected");
      }
      break;

    case 2:
      if (client.connected() == false)
      {
        connection = 0;
        //Serial.println("tcp client disconnected");
      }
      break;
  }

  return connection;
}
