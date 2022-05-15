#ifndef FUNCTIONS
#define FUNCTIONS

// uint32_t calculateCRC32(const uint8_t *data, size_t length)
// {
//   uint32_t crc = 0xffffffff;
//   while (length--)
//   {
//     uint8_t c = *data++;
//     for (uint32_t i = 0x80; i > 0; i >>= 1)
//     {
//       bool bit = crc & 0x80000000;
//       if (c & i)
//       {
//         bit = !bit;
//       }

//       crc <<= 1;
//       if (bit)
//       {
//         crc ^= 0x04c11db7;
//       }
//     }
//   }

//   return crc;
// }

void serial_init() {
  Serial.begin(SERIALBPS);
  Serial2.begin(SERIAL2BPS);
  Serial.setTimeout(2000);
  Serial2.setTimeout(2000);
  // Wait for serial to initialize.
  while (!Serial)
  {
  }
  while(!Serial2)
  {
  }
}

void wifi_init() {
  // // Try to read WiFi settings from RTC memory
  // bool rtcValid = false;
  // if (ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData)))
  // {
  //   // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
  //   uint32_t crc = calculateCRC32(((uint8_t *)&rtcData) + 4, sizeof(rtcData) - 4);
  //   if (crc == rtcData.crc32)
  //   {
  //     rtcValid = true;
  //   }
  // }

  // Serial.printf("\nConnecting to: %s ", SSID);
  // WiFi.persistent(false);
  // WiFi.mode(WIFI_STA);
  // if (rtcValid)
  // {
  //   // The RTC data was good, make a quick connection
  //   WiFi.begin(SSID, PASSWORD, rtcData.channel, rtcData.bssid, true);
  //   if(rtcData.gsmcheck >= GSMCHECK) {
  //     rtcData.gsmcheck = 0;
  //   }
  // }
  // else
  // {
  //   // The RTC data was not valid, so make a regular connection
  //   WiFi.begin(SSID, PASSWORD);
  //   rtcData.gsmcheck = 0;
  // }
  // Serial.printf("\n### gsmcheck: %d\n", rtcData.gsmcheck);
  Serial.printf("\nConnecting to: %s ", SSID);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
}

void mqtt_init() {
  mqttClient.setServer(MQTT_BROKER_ADRESS, MQTT_PORT);
}

boolean wifi_check() {

  int retries = 0;
  int wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED)
  {
    retries++;
    if (retries == 100)
    {
      // Quick connect is not working, reset WiFi and try regular connection
      WiFi.disconnect();
      delay(10);
      // WiFi.forceSleepBegin();
      // delay(10);
      // WiFi.forceSleepWake();
      // delay(10);
      WiFi.begin(SSID, PASSWORD);
    }
    if (retries == 600)
    {
      // Giving up after 30 seconds
      return false;
    }
    delay(50);
    wifiStatus = WiFi.status();
  }

  // // Write current connection info back to RTC
  // rtcData.channel = WiFi.channel();
  // memcpy(rtcData.bssid, WiFi.BSSID(), 6); // Copy 6 bytes of BSSID (AP's MAC address)
  // rtcData.gsmcheck++;
  // rtcData.crc32 = calculateCRC32(((uint8_t *)&rtcData) + 4, sizeof(rtcData) - 4);
  // ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));
  return true;
}

boolean mqtt_check(WiFiClient &client) {
  unsigned int timeout = 15;
  unsigned int counter = 0;
  mqttClient.setClient(client);
  while (!mqttClient.connected())
  {
    mqttClient.connect(HOSTNAME, MQTT_USER, MQTT_PASS);
    counter++;
    Serial.print(".");
    delay(100);
    if (counter > timeout)
    {
      Serial.printf("\nCan't connect to mqtt %s\n", MQTT_BROKER_ADRESS);
      return false;
    }
  }
  Serial.printf("\nConnected to mqtt %s\n", MQTT_BROKER_ADRESS);
  return true;
}

boolean internet_check(Client &client, String host, int port) {
  HttpClient http_client = HttpClient(client, host, port);
  http_client.beginRequest();
  http_client.get("/");
  http_client.endRequest();
  int statusCode = http_client.responseStatusCode();
  if (statusCode >= 200 && statusCode < 400)
  {
    Serial.printf("\nInternet OK (%s:%d - %d)\n", host.c_str(), port, statusCode);
    return true;
  } else {
    Serial.printf("\nInternet NOK (%s:%d - %d)\n", host.c_str(), port, statusCode);
    return false;
  }
}

double voltage(int pin) {
  double v = 0;
  for (unsigned int i = 0; i < 10; i++)
  {
    v = v + analogRead(pin);
    delay(5);
  }
  v = v / 10 / 4095 * 3.3 * 191 / 91 * 0.98883;
  v = (int)(v * 100 + 0.5) / 100.0;
  return v;
}

int bat_percentage(double voltage) {
  int perc;
  double vmin = 3;
  double vmax = 4.15;

  perc = ((voltage - vmin) / (vmax - vmin)) * 100;

  if(perc > 100)
    return 100.0f;
  
  if(perc < 0)
    return 0;

  return perc;
}

boolean vac_presence(int pin) {
  pinMode(pin, INPUT_PULLUP);
  if (digitalRead(pin) == 0) {
    return true;
  } else
  {
    return false;
  }
}

#endif