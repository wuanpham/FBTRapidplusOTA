#include "updateOTA.h"

int fwVersion = 0;
bool flagUpdate = false;
statusUpdate statusUpdate_t = none;
String fwUrl = "", fwName = "";

static void rebootEspWithReason(String reason)
{
    info_displayln(reason);
    delay(1000);
    ESP.restart();
}

void performUpdate(Stream &updateSoure, size_t updateSize)
{
    uint8_t header[32];
    updateSoure.readBytes(header, 32);
    for(int i=0;i<32;i++){
        Serial.printf("%02X ", header[i]);
    }
    Serial.println();

    if (!Update.begin(updateSize))
    {
        Update.printError(Serial);
        return;
    }

    updateSoure.setTimeout(15000);

    size_t written = Update.writeStream(updateSoure);

    info_displayln("Written: " + String(written));

    if (written != updateSize)
    {
        info_displayln("Written only: " + String(written) + "/" + String(updateSize) + ". Retry?");
    }

    if (!Update.end())
    {
        Update.printError(Serial);
    }
        
    if (Update.isFinished())
    {
        info_displayln("Update successfully completed. Rebooting...");
    }
    else
    {
        info_displayln("Update not finished? Something went wrong!");
    }   
}


void updateFromFS(fs::FS &fs)
{
    File updateBin = fs.open("/update.bin");
    if (updateBin)
    {
        if (updateBin.isDirectory())
        {
            info_displayln("Error, update.bin is not a file");
            updateBin.close();
            return;
        }

        size_t updateSize = updateBin.size();

        if (updateSize > 0)
        {
            info_displayln("Trying to start update");
            performUpdate(updateBin, updateSize);
        }
        else
        {
            info_displayln("Error, file is empty");
        }

        updateBin.close();
        info_displayln("Removing update file");
        fs.remove("/update.bin");

        rebootEspWithReason("Rebooting to completed OTA update");   
    }
    else
    {
        info_displayln("Could not load update.bin form spiffs root");
    }
}

bool downloadFirmware()
{
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    bool status = false;
    info_displayln(fwUrl);
    File f = SPIFFS.open("/update.bin", "w");
    if (f)
    {
        http.begin(fwUrl);
        int httpCode = http.GET();

        if (httpCode > 0)
        {
            if (httpCode == HTTP_CODE_OK)
            {
                info_displayln("Downloading...");
                http.writeToStream(&f);
                status = true;
            }
        }
        else
        {
            info_displayf("HTTP GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            f.close();
        }
    }
    else
    {
        info_displayln("Failed to open file");
    }
    
    http.end();

    return status;
}

void checkFirmware()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(baseUrl + checkFile);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            info_displayln(payload);
            DynamicJsonDocument json(1024);
            deserializeJson(json, payload);
            fwVersion = json["versionCode"].as<int>();
            fwName = json["fileName"].as<String>();
            fwUrl = baseUrl + fwName;
            if (fwVersion > currentVersion)
            {
                info_displayln("Firmware update available");
                flagUpdate = true;
            }
            else
            {
                info_displayln("You have the lasted version");
                flagUpdate = false;
            }
        }
        http.end();
    }
}

void beginOTA()
{
    if (!SPIFFS.begin(true))
    {
        info_displayln("SPIFFS Mount Failed");
        rebootEspWithReason("SPIFFS Mount Failed");
    } 
}

void updateOTA()
{
     if ((WiFi.status() == WL_CONNECTED) && statusUpdate_t == update)
    {
        _displayCLD.waittingUpdate();
        if (SPIFFS.exists("/update.bin")) 
        {
            SPIFFS.remove("/update.bin");
            info_displayln("Removed existing update file");
        }
        if (downloadFirmware()) 
        {
            info_displayln("Download completed");
            updateFromFS(SPIFFS);
        } 
        else 
        {
            info_displayln("Download failed");
        }
    }
}

void firmwareUpdate(void)
{
    if ((WiFi.status() == WL_CONNECTED) && statusUpdate_t == update)
    {
        _displayCLD.waittingUpdate();
        WiFiClientSecure client;
        client.setInsecure();
        t_httpUpdate_return ret = httpUpdate.update(client, fwUrl);

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            /* code */
            break;
        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            /* code */
            break;
        
        default:
            break;
        }
        rebootEspWithReason("OTA done!");
    }
}