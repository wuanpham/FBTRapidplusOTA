#ifndef _UPDATEOTA_H_
#define _UPDATEOTA_H_

#include "define.h"
#include "displayCLD.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Update.h>
#include "FS.h"
#include "SPIFFS.h"



void beginOTA(void);
void updateOTA(void);

extern bool flagUpdate;

#endif