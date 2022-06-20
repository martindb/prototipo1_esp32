#ifndef FUNCTIONS
#define FUNCTIONS

void serial_init() {
  Serial.begin(SERIALBPS);
  Serial.setTimeout(2000);
  Serial2.setRxBufferSize(2048);
  Serial2.begin(SERIAL2BPS, SERIAL_8N1, 16, 17, false);
  Serial2.setTimeout(20);
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
  Serial.printf("\nWifi: Connecting to %s ", SSID);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
}

void mqtt_init() {
  mqttClient.setServer(MQTT_BROKER_ADRESS, MQTT_PORT);
}

void gprs_init() {
  modem.setBaud(SERIAL2BPS);
  modem.init();
  //modem.restart();
}

void general_init() {
  serial_init();
  gprs_init();
  wifi_init();
  mqtt_init();
}

boolean wifi_check() {

  int retries = 0;
  WiFi.begin(SSID, PASSWORD);
  int wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED)
  {
    retries++;
    if (retries == 300)
    {
      // Giving up after 15 seconds
      return false;
    }
    delay(50);
    wifiStatus = WiFi.status();
  }
  return true;
}

boolean gprs_connect(int timeout) {

  if (modem.waitForNetwork(timeout * 1000L))
  {
    Serial.println("### waitForNetwork ok");
    if (modem.isGprsConnected()) {
      Serial.println("### gprsConnected ok");
      return true;
    } else {
      if (modem.gprsConnect(APN, GPRSUSER, GPRSPASS))
      {
        Serial.println("### gprsConnect ok");
        return true;
      }
      else
      {
        Serial.println("### gprsConnect ERROR");
        return false;
      }
    }
  }
  else
  {
    Serial.println("### waitForNetwork ERROR");
    return false;
  }
}

boolean gprs_check() {
  int retries = 0;
  gprs_connect(60);
  boolean conn = modem.isGprsConnected();
  while (!conn)
  {
    retries++;
    if(retries == 3) {
      Serial.println("### no conecta");
      return false;
    }
    Serial.printf("### intento %s\n", retries);
    // modem.init();
    modem.restart();
    gprs_connect(60);
    conn = modem.isGprsConnected();
  }
  Serial.println("### conecto!");
  return true;
}

boolean send_message(const char *message)
{
  boolean result = true;

  for (int i = 0; i < phone_number_count; i++)
  {
    result &= modem.sendSMS(numbers[i], message);
  }
  Serial.printf("### sendSMS %d\n", result);
  return result;
}

boolean mqtt_check(Client &client) {
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
  http_client.stop();
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

void temp(DallasTemperature* tline, StaticJsonDocument<800>& document, const char* sensor, const char* line) {
  tline->begin();
  int tlsensors = tline->getDeviceCount();
  tline->requestTemperatures();

  document[sensor][line]["quantity"] = tlsensors;
  for (int i = 0; i <= tlsensors; i++)
  {
    DeviceAddress saddr;
    tline->getAddress(saddr, i);
    char address[17];
    sprintf(address, "%02X%02X%02X%02X%02X%02X%02X%02X", saddr[0], saddr[1], saddr[2], saddr[3], saddr[4], saddr[5], saddr[6], saddr[7]);
    document[sensor][line][address] = tline->getTempC(saddr);
  }
}

double sleep_calc(double vcc, int pbat) {
  if (vcc > 4)
  {
    return SLEEPVCC;
  }
  else
  {
    if (pbat >= 50)
    {
      return SLEEPBAT;
    }
    else
    {
      return SLEEPLOWBAT;
    }
  }
}

void set_wakeup(double vcc) {
  if (vcc < 4)
  {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1); // wakeup cuando vuelve VCC
  }
  else
  {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0); // wakeup cuando se va VCC
  }
}

#endif