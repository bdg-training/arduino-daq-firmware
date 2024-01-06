# arduino-daq-firmware
Turn your Arduino into a Data Acquisition (DAQ) Board.

## Configuration
||Constants|Description|
|---|---|---|
|Digtial Inputs|const int DIS[] = {2, 4, 7, 8};||
|Internal Pull-Ups|const bool DIS_PULL_UPS[] = {false, false, false, false};|false=floating, true=pull up|
|Analog Inputs|const int AIS[] = {A0, A1, A6, A7};||
|Digital Outputs|const int DOS[] = {10, 11, 12, 16};||
|Analog Outputs|const int AOS[] = {3, 5, 6, 9};||

## Communication
115200 baud
||Request (Command)|Response|Parameter|
|---|---|---|---|
||help|||
|Identify Board|*idn?|BDG GmbH,arduino-daq-firmware,1,20240106|Manufacturer,Model,Serial Number,Firmware Level,Options|
|digitalRead()|{"read":"di"}|{"di":[false,false,false,false]}|false, true|
|analogRead()|{"read":"ai"}|{"ai":[400,368,383,373]}|0..1023|
|Config|{"diInterval":0}|{"status":"ok"}|0[ms]=off|
|Config|{"aiInterval":0}|{"status":"ok"}|0[ms]=off|
|Config|{"onChange":true}|{"status":"ok"}|false=send every ms, true=send on change|
|digitalWrite()|{"do":[1,0,0,0]}||-1=keep state, 0=off, 1=on, 2=toggle|
|analogWrite()|{"ao":[255,0,0,0]}||-1=keep state, 0=off, 255=on|

## Boards
|Name|Pin|Function|
|---|---|---|
|Arduino Nano|D13|LED_BUILTIN|
||D18/A4|I2C-SDA|
||D19/A5|I2C-SCL|