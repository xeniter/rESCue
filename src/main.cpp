#include <Arduino.h>
#include <Logger.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "BatteryMonitor.h"
#include "Buzzer.h"
////#include "SoundController.h"
#include "ILedController.h"
#include "Ws28xxController.h"
#include "BleServer.h"
#include "CanBus.h"
#include "AppConfiguration.h"


long mainLoop = 0;
long loopTime = 0;
long maxLoopTime = 0;
int new_forward  = LOW;
int new_backward = LOW;
int new_brake    = LOW;
int idle         = LOW;
double idle_erpm = 10.0;

int lastFake = 4000;
int lastFakeCount = 0;
VescData vescData;

HardwareSerial vesc(2);

//ILedController *ledController;

#if defined(CANBUS_ENABLED)
 CanBus * canbus = new CanBus(&vescData);
#endif //CANBUS_ENABLED
 BatteryMonitor *batMonitor = new BatteryMonitor(&vescData);

BleServer *bleServer = new BleServer();

int port = 65102;  // standard vesc port
WiFiServer WifiServer(port);

// Declare the local logger function before it is called.
void localLogger(Logger::Level level, const char* module, const char* message);

#if defined(CANBUS_ENABLED)
void fakeCanbusValues() {
    if(millis() - lastFake > 3000) {
        vescData.tachometer= random(0, 30);
        vescData.inputVoltage = random(43, 50);
        vescData.dutyCycle = random(0, 100);
        if(lastFakeCount > 10) {
            vescData.erpm = random(-100, 200);
        } else {
            vescData.erpm = 0;//random(-100, 200);
        }
        vescData.current = random(0, 10);
        vescData.ampHours = random(0, 100);
        vescData.mosfetTemp = random(20, 60);
        vescData.motorTemp = random(20, 40);
        vescData.adc1 = 0.5;
        vescData.adc2 = 0.5;
        vescData.switchState = 0;
        lastFake = millis();
        lastFakeCount++;
    }
}
#endif

void setup() {
  Logger::setOutputFunction(localLogger);

  AppConfiguration::getInstance()->readPreferences();
    AppConfiguration::getInstance()->config.sendConfig = false;
  Logger::setLogLevel(AppConfiguration::getInstance()->config.logLevel);
  if(Logger::getLogLevel() != Logger::SILENT) {
      Serial.begin(VESC_BAUD_RATE);
  }

  if(AppConfiguration::getInstance()->config.otaUpdateActive) {
     return;
  }

  /*
  ledController = LedControllerFactory::getInstance()->createLedController(&vescData);

  pinMode(PIN_FORWARD, INPUT);
  pinMode(PIN_BACKWARD, INPUT);
  pinMode(PIN_BRAKE, INPUT);
  */

  vesc.begin(VESC_BAUD_RATE, SERIAL_8N1, VESC_RX_PIN, VESC_TX_PIN, false);
  delay(50);
#ifdef CANBUS_ENABLED
  // initializes the CANBUS
  canbus->init();
#endif //CANBUS_ENABLED

  // initializes the battery monitor
  batMonitor->init();
  // initialize the UART bridge from VESC to BLE and the BLE support for Blynk (https://blynk.io)
#ifdef CANBUS_ONLY
  bleServer->init(canbus->stream, canbus);
#else
  bleServer->init(&vesc);
#endif

  /*
  // initialize the LED (either COB or Neopixel)
  ledController->init();

  Buzzer::getInstance()->startSequence();
  ledController->startSequence();
  */

  char buf[128];
  snprintf(buf, 128, " sw-version %d.%d.%d is happily running on hw-version %d.%d",
    SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH,
    HARDWARE_VERSION_MAJOR, HARDWARE_VERSION_MINOR);
  Logger::notice("rESCue", buf);

  WiFi.mode(WIFI_STA);
  WiFi.begin("xeniter", "dipolts_wifi");

  ArduinoOTA.setHostname("rESCueST");
  ArduinoOTA.begin();

  WifiServer.begin();

  uint num_pixels = 22;
  Adafruit_NeoPixel front_leds(num_pixels, 4, NEO_GRB + NEO_KHZ800);
  for (int i = 0; i < num_pixels; i++) {
      front_leds.setPixelColor(i, Adafruit_NeoPixel::Color(200, 200, 200)); // RGB 
  }
  front_leds.show();

  
  Adafruit_NeoPixel back_leds(num_pixels, 2, NEO_GRB + NEO_KHZ800);
  for (int i = 0; i < num_pixels; i++) {
      back_leds.setPixelColor(i, Adafruit_NeoPixel::Color(200, 0, 0)); // RGB 
  }
  back_leds.show();
}

void loop() {
    loopTime = millis() - mainLoop;
    mainLoop = millis() ;
    if(loopTime > maxLoopTime) {
        maxLoopTime = loopTime;
    }

    if(AppConfiguration::getInstance()->config.otaUpdateActive) {
    return;
  }
  if(AppConfiguration::getInstance()->config.sendConfig) {
      bleServer->sendConfig();
      AppConfiguration::getInstance()->config.sendConfig = false;
  }  if(AppConfiguration::getInstance()->config.saveConfig) {
      AppConfiguration::getInstance()->savePreferences();
      AppConfiguration::getInstance()->config.saveConfig = false;
  }


  ArduinoOTA.handle();

  WiFiClient client = WifiServer.available();

  if (client) {
    if(client.connected())
    {
      //Serial.println("Client Connected");
      Logger::error(LOG_TAG_BLESERVER, "Client Connected");
    }

    while(client.connected()){      
      while(client.available()>0){
        // read data from the connected client
        vesc.write(client.read()); 
      }
      //Send Data to connected client
      while(vesc.available()>0)
      {
        client.write(vesc.read());
      }
    }
    client.stop();
    //Serial.println("Client disconnected");    
    Logger::error(LOG_TAG_BLESERVER, "Client disconnected");
  }


/*
#ifdef CANBUS_ENABLED
  #ifdef FAKE_VESC_ENABLED
    fakeCanbusValues();
  #endif

  new_forward  = vescData.erpm > idle_erpm ? HIGH : LOW;
  new_backward = vescData.erpm < -idle_erpm ? HIGH : LOW;
  idle         = (abs(vescData.erpm) < idle_erpm && vescData.switchState == 0) ? HIGH : LOW;
  new_brake    = (abs(vescData.erpm) > idle_erpm && vescData.current < -4.0) ? HIGH : LOW;
#else
  new_forward  = digitalRead(PIN_FORWARD);
  new_backward = digitalRead(PIN_BACKWARD);
  new_brake    = digitalRead(PIN_BRAKE);
  idle         = new_forward == LOW && new_backward == LOW;
#endif
*/

#ifdef CANBUS_ENABLED
 #ifndef FAKE_VESC_ENABLED
  canbus->loop();
 #endif
#endif

  // // is motor brake active?
  // if(new_brake == HIGH) {
  // // flash backlights
  //   ledController->changePattern(Pattern::RESCUE_FLASH_LIGHT, new_forward == HIGH, false);
  // }

  // // call the led controller loop
  // ledController->loop(&new_forward, &new_backward, &idle);

  // measure and check voltage
  batMonitor->checkValues();

  // call the VESC UART-to-Bluetooth bridge
#ifdef CANBUS_ENABLED
  bleServer->loop(&vescData, loopTime, maxLoopTime);
#else
  bleServer->loop();
#endif
}

void localLogger(Logger::Level level, const char* module, const char* message) {
  Serial.print(F("FWC: ["));
  Serial.print(Logger::asString(level));
  Serial.print(F("] "));
  if (strlen(module) > 0) {
      Serial.print(F(": "));
      Serial.print(module);
      Serial.print(" ");
  }
  Serial.println(message);
}