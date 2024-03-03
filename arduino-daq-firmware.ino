
// https://github.com/bdg-training/arduino-daq-firmware

// 115200 baud
// command: help

// command: *idn?
// <Manufacturer>,<Model>,<Serial Number>,<Firmware Level>,<Options>
#define IDN "BDG GmbH,arduino-daq-firmware,1,20240303"

// Arduino Nano
// D13 = LED_BUILTIN
// D18/A4 = I2C-SDA
// D19/A5 = I2C-SCL

// pin config
const int DIS[] = {2, 4, 7, 8};
// false=floating
// true=pull up
const bool DIS_PULL_UPS[] = {false, false, false, false};
const int AIS[] = {A0, A1, A6, A7};
const int DOS[] = {10, 11, 12, 16};
// PWM-Outputs
const int AOS[] = {3, 5, 6, 9};

int readMillisDi = 50;
int readMillisAi = 0;
bool onChange = true;
#define TIMESTAMP false

#define LF 10
#define CR 13
#define RX_SIZE  512
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

unsigned long delayLed = 0;
unsigned long delayDi = 0;
unsigned long delayAi = 0;
String oldDiState = "";
String oldAiState = "";
String temp;
int aosValues[sizeof(AIS) / sizeof(int)];

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  delay(50);
  Serial.begin(115200);
  //Serial.println(" ");
  //Serial.println("setup()");

  for (byte i = 0; i < sizeof(DIS) / sizeof(int); i++) {
    pinMode(DIS[i], INPUT);

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
  }

  for (byte i = 0; i < sizeof(AOS) / sizeof(int); i++) {
    pinMode(AOS[i], OUTPUT);
  }

  oldDiState = GetDis();
  oldAiState = GetAis();

  digitalWrite(LED_BUILTIN, LOW);
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
      Serial.println(temp);
    }
  }

  if (readMillisAi > 0 && millis() - delayAi >= readMillisAi)
  {
    delayAi = millis();
    temp = GetAis();

    if (temp != oldAiState)
    {
      oldAiState = temp;
      Serial.println(temp);
    }
  }

  if (millis() - delayLed >= 500)
  {
    delayLed = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}


void CheckRxData()
{
  if (Serial.available() > 0)
  {
    if (RxIndex >= RX_SIZE)
    {
      RxIndex = 0;
    }
    byte rx = Serial.read();

    if (RxIndex == 0 && (rx == LF || rx == CR))
    {
      return;
    }

    if (rx == LF || rx == CR)
    {
      RxBuffer[RxIndex++] = 0;
      digitalWrite(LED_BUILTIN, HIGH);
      OnDataReceived();
      delayLed = millis();
      RxIndex = 0;
    }
    else
    {
      RxBuffer[RxIndex++] = rx;
    }
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
      Serial.println(IDN);
      return;
    }
    else if (json.startsWith(CMD_HELP))
    {
      Serial.print("--- ");
      Serial.print(CMD_HELP);
      Serial.println(" ---");

      Serial.print("*idn?\t");
      Serial.println(IDN);

      Serial.println("{\"read\":\"di\"}         (di=false/true, do=false/true, ai=0..1023, ao=0..255)");
      Serial.println("{\"diInterval\":0}      (0[ms]=off)");
      Serial.println("{\"aiInterval\":0}      (0[ms]=off)");
      Serial.println("{\"onChange\":true}     (false=send every ms, true=send on change)");
      Serial.println("{\"do\":[1,0,0,0]}      (-1=keep state, 0=off, 1=on, 2=toggle)");
      Serial.println("{\"ao\":[255,0,0,0]}    (-1=keep state, 0=off, 255=on)");
      Serial.println("------------");
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
    Serial.println(oldDiState);
  }
  else if (json.startsWith("do"))
  {
    Serial.println(GetDos());
  }
  else if (json.startsWith("ai"))
  {
    oldAiState = GetAis();
    Serial.println(oldAiState);
  }
  else if (json.startsWith("ao"))
  {
    Serial.println(GetAos());
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
  Serial.print("{\"status\":\"");
  Serial.print(text);
  Serial.println("\"}");
}
