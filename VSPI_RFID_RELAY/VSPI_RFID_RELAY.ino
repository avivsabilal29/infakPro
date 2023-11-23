#include "BluetoothSerial.h"
#include <esp_adc_cal.h>
String device_name = "VBIOT-" + String(ESP.getEfuseMac(), HEX);
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif
BluetoothSerial SerialBT;
boolean confirmRequestPending = true;
void BTConfirmRequestCallback(uint32_t numVal) {
  confirmRequestPending = true;
  Serial.println(numVal);
}
void BTAuthCompleteCallback(boolean success) {
  confirmRequestPending = false;
  if (success) {
    Serial.println("Pairing success!!");
  } else {
    Serial.println("Pairing failed, rejected by user!!");
  }
}

int batt_cent;
// ESP32 LilyGO-T-SIM7000G pins definition
#define MODEM_UART_BAUD 115200
#define MODEM_DTR 25
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4
#define LED_PIN 12

#define SCK  18
#define MISO  19
#define MOSI  23
#define CS  5
#define SDA_PIN 21
#define RST_PIN 15
#define RELAY 14
//
#define ADC_PIN 35
int vref = 1100;
uint32_t timeStamp = 0;

// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial
// Set serial for AT commands (to the SIM7000 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM7000   // Modem is SIM7000
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 5           // Time ESP32 will go to sleep (in seconds)

// Include after TinyGSM definitions
#include <TinyGsmClient.h>
#include <SPI.h>
#include <MFRC522.h>
int relay1 = LOW;
SPIClass spi = SPIClass(VSPI);
MFRC522 mfrc522(SDA_PIN, RST_PIN);   // Create MFRC522 instance.


const char apn[] = "internet";
const char server[] = "infakpro.com"; // domain name: example.com, maker.ifttt.com, etc
const char resource[] = "/post-data.php";         // resource path, for example: /post-data.php
const int  port = 80;
String apiKeyValue = "InfakProFaisal23";
String auth = "";

// Layers stack
TinyGsm sim_modem(SerialAT);
TinyGsmClient client(sim_modem);

// SPECIAL VOID TO MANAGE MODEM SIM7000G
long ubahmaxmin(long x, long in_min, long in_max, long out_min, long out_max) {
  // if input is smaller/bigger than expected return the min/max out ranges value
  if (x < in_min)
    return out_min;
  else if (x > in_max)
    return out_max;
  // map the input to the output range.
  // round up if mapping bigger ranges to smaller ranges
  else if ((in_max - in_min) > (out_max - out_min))
    return (x - in_min) * (out_max - out_min + 1) / (in_max - in_min + 1) + out_min;
  // round down if mapping smaller ranges to bigger ranges
  else
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void turnModemOn() {
  digitalWrite(LED_PIN, LOW);

  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);  //Datasheet Ton mintues = 1S
  digitalWrite(MODEM_PWRKEY, HIGH);
}

void turnModemOff() {
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1500);  //Datasheet Ton mintues = 1.2S
  digitalWrite(MODEM_PWRKEY, HIGH);

  digitalWrite(LED_PIN, LOW);
}

void setupModem() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  turnModemOff();
  delay(1000);
  turnModemOn();
  delay(5000);
}

void enableGPS(void) {
  // Set Modem GPS Power Control Pin to HIGH ,turn on GPS power
  // Only in version 20200415 is there a function to control GPS power
  sim_modem.sendAT("+CGPIO=0,48,1,1");
  if (sim_modem.waitResponse(10000L) != 1) {
    DBG("Set GPS Power HIGH Failed");
  }
  sim_modem.enableGPS();
}

void disableGPS(void) {
  // Set Modem GPS Power Control Pin to LOW ,turn off GPS power
  // Only in version 20200415 is there a function to control GPS power
  sim_modem.sendAT("+CGPIO=0,48,1,0");
  if (sim_modem.waitResponse(10000L) != 1) {
    DBG("Set GPS Power LOW Failed");
  }
  sim_modem.disableGPS();
}

void modemPowerOn() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1000);  //Datasheet Ton mintues = 1S
  digitalWrite(MODEM_PWRKEY, LOW);
}

void modemPowerOff() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1500);  //Datasheet Ton mintues = 1.2S
  digitalWrite(MODEM_PWRKEY, LOW);
}

void modemRestart() {
  modemPowerOff();
  modemPowerOn();
}
// SPECIAL VOID TO MANAGE MODEM SIM7000G

void setup() {
  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  SerialMon.begin(115200);
  SPI.begin(SCK, MISO, MOSI, CS);      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("Approximate your card to the reader...");
  SerialBT.print("Approximate your card to the reader...");
  Serial.println();
  delay(100);

  // Set SIM module baud rate and UART pins
  SerialAT.begin(MODEM_UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100);

  // Start BT Serial SSP
  SerialBT.enableSSP();
  // SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  // SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin(device_name);  //Bluetooth device name
  SerialBT.println("VBIOT_MAC_ADDR: " + String(ESP.getEfuseMac(), HEX));
  while (millis() < 20000) {
    SerialBT.print(".");
    delay(1000);
  }

  // ADC INIT
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);  //Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
    vref = adc_chars.vref;
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
  } else {
    Serial.println("Default Vref: 1100mV");
  }

  // Modem INIT
  //modemRestart();
  SerialBT.println("\nSOFT RESET MODEM...");
  sim_modem.restart();
  disableGPS();
  delay(1000);

  SerialBT.println("VBIOT_MAC_ADDR: " + String(ESP.getEfuseMac(), HEX));
  SerialBT.println("> Check whether Modem is online");
  //test modem is online ?
  uint32_t timeout = millis();
  while (!sim_modem.testAT()) {
    SerialBT.print(".");
    if (millis() - timeout > 60000) {
      SerialBT.println("> It looks like the modem is not responding, trying to restart");
      modemRestart();
      timeout = millis();
    }
  }
  SerialBT.println("[OK] Modem is online");

  //test sim card is online ?
  timeout = millis();
  SerialBT.print("> Get SIM card status");
  while (sim_modem.getSimStatus() != SIM_READY) {
    SerialBT.print(".");
    if (millis() - timeout > 60000) {
      SerialBT.println("It seems that your SIM card has not been detected. Has it been inserted?");
      SerialBT.println("If you have inserted the SIM card, please remove the power supply again and try again!");
      return;
    }
  }
  SerialBT.println();
  SerialBT.println("> SIM card exists");

  String res = sim_modem.getIMEI();
  SerialBT.print("IMEI:");
  SerialBT.println(res);
  SerialBT.println();

  //Set mobile operation band
  sim_modem.sendAT("+CBAND=ALL_MODE");
  sim_modem.waitResponse();

  sim_modem.setPreferredMode(3);
  sim_modem.setNetworkMode(2);

  RegStatus status;
  timeout = millis();
  while (status != REG_OK_HOME && status != REG_OK_ROAMING) {
    int16_t sq = sim_modem.getSignalQuality();
    status = sim_modem.getRegistrationStatus();
    if (status == REG_DENIED) {
      SerialBT.println("> The SIM card you use has been rejected by the network operator. Please check that the card you use is not bound to a device!");
      return;
    } else {
      SerialBT.print("Signal:");
      SerialBT.println(sq);
    }

    if (millis() - timeout > 360000) {
      if (sq == 99) {
        SerialBT.println("> It seems that there is no signal. Please check whether the"
                         "LTE antenna is connected. Please make sure that the location has 2G/NB-IOT signal\n"
                         "SIM7000G does not support 4G network. Please ensure that the USIM card you use supports 2G/NB access");
        return;
      }
      timeout = millis();
    }
    delay(500);
  }

  //Start GPRS Connect
  SerialBT.println("\nWaiting for network...");
  if (!sim_modem.waitForNetwork()) {
    SerialBT.print("...");
    delay(10000);
    return;
  }
  if (sim_modem.isNetworkConnected()) {
    SerialBT.println("Network connected");
  }
  SerialBT.println("\n---Starting GPRS TEST---\n");
  SerialBT.println("Connecting to: " + String(apn));
  if (!sim_modem.gprsConnect(apn)) {
    SerialBT.print("...");
    delay(10000);
    return;
  }
  SerialBT.print("GPRS status: ");
  if (sim_modem.isGprsConnected()) {
    SerialBT.println("connected");
  } else {
    SerialBT.println("not connected");
  }
  //End GPRS Connect

  SerialBT.println("Obtain the APN issued by the network");
  sim_modem.sendAT("+CGNAPN");
  if (sim_modem.waitResponse(3000, res) == 1) {
    res = res.substring(res.indexOf(",") + 1);
    res.replace("\"", "");
    res.replace("\r", "");
    res.replace("\n", "");
    res.replace("OK", "");
    SerialBT.print("The APN issued by the network is:");
    SerialBT.println(res);
  }

  sim_modem.sendAT("+CNACT=1");
  sim_modem.waitResponse();

  res = sim_modem.getLocalIP();
  sim_modem.sendAT("+CNACT?");
  if (sim_modem.waitResponse("+CNACT: ") == 1) {
    sim_modem.stream.read();
    sim_modem.stream.read();
    res = sim_modem.stream.readStringUntil('\n');
    res.replace("\"", "");
    res.replace("\r", "");
    res.replace("\n", "");
    sim_modem.waitResponse();
    SerialBT.print("The current network IP address is:");
    SerialBT.println(res);
  }

  sim_modem.sendAT("+CPSI?");
  if (sim_modem.waitResponse("+CPSI: ") == 1) {
    res = sim_modem.stream.readStringUntil('\n');
    res.replace("\r", "");
    res.replace("\n", "");
    sim_modem.waitResponse();
    SerialBT.print("The current network parameter is:");
    SerialBT.println(res);
  }
  SerialBT.println("Done Network Init...");
}

void loop() {
  // Code for RFID

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  //Show UID on serial monitor
  Serial.print("UID tag :");
  SerialBT.print("UID Tag : ");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    SerialBT.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    SerialBT.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message : ");
  SerialBT.print("Message");
  content.toUpperCase();
  if ((content.substring(1) == "E5 FF A4 AC") || (content.substring(1) == "E5 FF A4 AC")) //change here the UID of the card/cards that you want to give access
  {
    Serial.println("Authorized Access");
    auth = "Authorized Access";
    SerialBT.print("Authorized Access");
    Serial.println();
    relay1 = ~ relay1;
    digitalWrite(RELAY, relay1);
    delay(500);
  }

  else   {
    Serial.println("Access Denied");
    SerialBT.print("Access Denied");
    auth = "Access Denied" ;
    delay(3000);
  }

  //BAT ADC
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    uint16_t v = analogRead(ADC_PIN);
    float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    String voltage = "Voltage :" + String(battery_voltage) + "V";
    batt_cent = ubahmaxmin(battery_voltage * 1000, 2800, 4200, 0, 100);

    // When connecting USB, the battery detection will return 0,
    // because the adc detection circuit is disconnected when connecting USB
    /*SerialBT.println(voltage);
      if (voltage == "Voltage :0.00V") {
        SerialBT.println("USB is connected, please disconnect USB.");
      }*/
  }

  enableGPS();
  SerialBT.println("Start positioning . Make sure to locate outdoors.");
  SerialBT.println("The blue indicator light flashes to indicate positioning.");
  float lat2 = 0;
  float lon2 = 0;
  float speed2 = 0;
  float alt2 = 0;
  int vsat2 = 0;
  int usat2 = 0;
  float accuracy2 = 0;
  int year2 = 0;
  int month2 = 0;
  int day2 = 0;
  int hour2 = 0;
  int min2 = 0;
  int sec2 = 0;
  int signal2 = ubahmaxmin(sim_modem.getSignalQuality(), 2, 31, 1, 5);

  while (1) {
    if (sim_modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2, &year2, &month2, &day2, &hour2, &min2, &sec2)) {
      //Adjust GMT7
      hour2 += 7;
      if (hour2 >= 24) {
        hour2 -= 24;
        day2 += 1;
      }
      digitalWrite(LED_PIN, HIGH);
      SerialBT.println("The location has been locked, the latitude and longitude are:");
      SerialBT.println("VBIOT_MAC_ADDR: " + String(ESP.getEfuseMac(), HEX));
      String imsi = sim_modem.getIMSI();
      String imsiPre = "62851";
      String imsiMid = imsi.substring(5, 7);
      String imsiLast = imsi.substring(9);
      String phone = imsiPre + imsiMid + imsiLast;
      SerialBT.println("IMSI: " + imsi);
      SerialBT.println("Phone: " + phone);
      SerialBT.println("\n---GPS LOCATION---\n");
      SerialBT.println("speed: " + String(speed2));
      SerialBT.println("altitude: " + String(alt2));
      SerialBT.println("satellitesVisible : " + String(vsat2));
      SerialBT.println("satellitesUsed: " + String(usat2));
      SerialBT.println("accuracy: " + String(accuracy2));
      SerialBT.println("latitude: " + String(lat2, 8));
      SerialBT.println("longitude: " + String(lon2, 8));
      SerialBT.println("temperature: " + String(batt_cent));
      SerialBT.println("humidity: " + String(batt_cent));
      SerialBT.println("battery: " + String(batt_cent));
      SerialBT.println("signalStrength: " + String(signal2) + " Bar");
      SerialBT.println("status: " + String("OK"));
      SerialBT.println("errorMessage: " + String(""));
      SerialBT.println("recordedAt: " + String(year2) + "-" + String(month2) + "-" + String(day2) + "T" + String(hour2) + ":" + String(min2) + ":" + String(sec2) + "." + "000Z");
      SerialBT.println("\n\n");
      SerialBT.println("\n\nDONE! Reset GPS Antenna");

      for (int i = 20; i > 0; i--) {
        SerialBT.print(String(i) + "...");
        delay(1000);
      }
      break;
    } else {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      //
      int signal1 = ubahmaxmin(sim_modem.getSignalQuality(), 2, 31, 1, 5);
      float lat = 0;
      float lon = 0;
      float accuracy = 0;
      int year = 0;
      int month = 0;
      int day = 0;
      int hour = 0;
      int min = 0;
      int sec = 0;
      SerialBT.println("VBIOT_MAC_ADDR: " + String(ESP.getEfuseMac(), HEX));
      String imsi = sim_modem.getIMSI();
      String imsiPre = "62851";
      String imsiMid = imsi.substring(5, 7);
      String imsiLast = imsi.substring(9);
      String phone = imsiPre + imsiMid + imsiLast;
      SerialBT.println("IMSI: " + imsi);
      SerialBT.println("Phone: " + phone);
      SerialBT.println("\n---GSM LOCATION FIRST---\n");
      sim_modem.getGsmLocation(&lon, &lat, &accuracy, &year, &month, &day, &hour, &min, &sec);
      SerialBT.println("accuracy: " + String(accuracy));
      SerialBT.println("latitude: " + String(lat, 8));
      SerialBT.println("longitude: " + String(lon, 8));
      SerialBT.println("temperature: " + String(batt_cent));
      SerialBT.println("humidity: " + String(batt_cent));
      SerialBT.println("battery: " + String(batt_cent));
      SerialBT.println("signalStrength: " + String(signal1) + " Bar");
      SerialBT.println("status: " + String("OK"));
      SerialBT.println("errorMessage: " + String(""));
      SerialBT.println("recordedAt: " + String(year) + "-" + String(month) + "-" + String(day) + "T" + String(hour) + ":" + String(min) + ":" + String(sec) + "." + "000Z");
      SerialBT.println("\n\n");


      if (sim_modem.isGprsConnected()) {
        SerialMon.println(" OK");
        SerialBT.println("OK");
        if (!client.connect(server, port)) {
          SerialBT.println("Fail");
          SerialMon.println(" Fail");
        } else {
          SerialBT.println("OK");
          SerialMon.println(" OK");

          // Making an HTTP POST request
          SerialMon.println("Performing HTTP POST request...");
          SerialBT.println("Performing HTTP POST request...");
          // Prepare your HTTP POST request data (Temperature in Celsius degrees)
          String httpRequestData = "api_key=" + apiKeyValue + "&value1=" + String(lat, 8)
                                   + "&value2=" + String(lon, 8) + "&value3=" + auth + "";
          // Prepare your HTTP POST request data (Temperature in Fahrenheit degrees)
          //String httpRequestData = "api_key=" + apiKeyValue + "&value1=" + String(1.8 * bme.readTemperature() + 32)
          //                       + "&value2=" + String(bme.readHumidity()) + "&value3=" + String(bme.readPressure()/100.0F) + "";

          // You can comment the httpRequestData variable above
          // then, use the httpRequestData variable below (for testing purposes without the BME280 sensor)
          //String httpRequestData = "api_key=tPmAT5Ab3j7F9&value1=24.75&value2=49.54&value3=1005.14";

          client.print(String("POST ") + resource + " HTTP/1.1\r\n");
          client.print(String("Host: ") + server + "\r\n");
          client.println("Connection: close");
          client.println("Content-Type: application/x-www-form-urlencoded");
          client.print("Content-Length: ");
          client.println(httpRequestData.length());
          client.println();
          client.println(httpRequestData);

          unsigned long timeout = millis();
          while (client.connected() && millis() - timeout < 10000L) {
            // Print available data (HTTP response from server)
            while (client.available()) {
              char c = client.read();
              SerialMon.print(c);
              SerialBT.print("Client Print");
              SerialBT.println(c);
              timeout = millis();
            }
          }
          SerialMon.println();

          // Close client and disconnect
          client.stop();
          SerialMon.println(F("Server disconnected"));
          SerialBT.print("Server disconnected");
          sim_modem.gprsDisconnect();
          SerialMon.println(F("GPRS disconnected"));
          SerialBT.print("GPRS disconnected");
        }
      }
    }
    delay(1000);
  }
  disableGPS();
  SerialMon.println("\nEND OF TEST CYCLE\n\n\n");
  SerialBT.println("\nEND OF TEST CYCLE\n\n\n");
  delay(10);

  // GO TO SLEEP
  SerialMon.println("ESP Sleep Five Second");
  SerialBT.println("ESP Sleep Five Second");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  delay(200);
  esp_deep_sleep_start();
}
