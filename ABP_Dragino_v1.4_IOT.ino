/* ************************************************************** 
 * Arduino sketch LoraWAN Node DHT11 Temp and Hum .
 * Author: Nikolai Manchev
 * Organisation: Eniware Open Source
 * Dragino v1.4 + X1276 + DHT11
 * 
 * *************************************************************/
#include <SPI.h>
#include <Wire.h>
#define BUILTIN_LED 25
// define the activation method ABP or OTAA
#define ACT_METHOD_ABP

// show debug statements; comment next line to disable debug statements
//#define DEBUG

/* **************************************************************
* keys for device
* *************************************************************/
static const uint8_t PROGMEM NWKSKEY[16] = { 0x6A, 0x87, 0x9F, 0x4D, 0x83, 0xD6, 0x54, 0x97, 0xA6, 0xF7, 0xD8, 0x06, 0xB6, 0x41, 0x47, 0x3F };
static const uint8_t PROGMEM APPSKEY[16] = { 0x7C, 0xAD, 0xA4, 0xFF, 0x73, 0x8E, 0x21, 0x69, 0xDE, 0xAA, 0xAB, 0xA5, 0x5E, 0x35, 0x55, 0xD2 };
static const uint32_t DEVADDR = 0x26011657;

/* **************************************************************
 * user settings
 * *************************************************************/
// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 10; //time in seconds
 
unsigned long starttime;
unsigned long cycle_length = TX_INTERVAL * 1000UL; // cycle in secs;

// Uses LMIC libary by Thomas Telkamp and Matthijs Kooijman (https://github.com/matthijskooijman/arduino-lmic)
// Pin mappings based upon PCB Doug Larue
#include <lmic.h>
#include <hal/hal.h>

// Declare the job control structures
static osjob_t sendjob;

// These callbacks are only used in over-the-air activation, so they are
// left empty when ABP (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
#ifdef ACT_METHOD_ABP
  void os_getArtEui (u1_t* buf) { }
  void os_getDevEui (u1_t* buf) { }
  void os_getDevKey (u1_t* buf) { }
#else
  void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
  void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
  void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}
#endif

/* ************************************************************** 
 * Pin mapping
 * *************************************************************/
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};


/* ************************************************************** 
 * Sensor setup
 * *************************************************************/

#include "DHT.h"
#define DHTPIN 5   // what digital pin we're connected to

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// moisture variable
int moisture = 0;
// temperature in Celcius
int tempC = 0;
// temperature in Fahrenheid
int tempF = 0;

DHT sensor(DHTPIN, DHTTYPE);

unsigned int counter = 0; 

// data to send
static uint8_t dataTX[2];

/* **************************************************************
 * setup
 * *************************************************************/
void setup() {
  //Set baud rate
  Serial.begin(115200);
  // Wait (max 10 seconds) for the Serial Monitor
  while ((!Serial) && (millis() < 10000)){ }
  Serial.println(F("Ардуино драгино версия 1.4 сензор за темп. и влага DHT 11"));

  
  init_node();
  init_sensor();
    

  starttime = millis();

}


/* **************************************************************
 * loop
 * *************************************************************/
void loop() {
  
  do_sense();

  // check if need to send
  if ((millis() - starttime) > cycle_length) { build_data(); do_send(); starttime = millis(); }

  
}


/* **************************************************************
 * sensor code, typical would be init_sensor(), do_sense(), build_data()
 * *************************************************************/
/* **************************************************************
 * init the sensor
 * *************************************************************/
void init_sensor() {
  sensor.begin(); // reset sensor
  delay(1000); // give some time to boot up 
}

/* **************************************************************
 * do the reading
 * *************************************************************/
void do_sense() {
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  moisture = sensor.readHumidity();
  // Read temperature as Celsius (the default)
  tempC = sensor.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  tempF = sensor.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(moisture) || isnan(tempC) || isnan(tempF)) {
    #ifdef DEBUG   
      Serial.println("Грешпа при четене на DHT сензора");
    #endif  
    return;
  }

  
  #ifdef DEBUG
    Serial.print(F(" temp Celcius:"));
    Serial.print(tempC);
    Serial.print(F(" temp Fahrenheit:"));
    Serial.print(tempF);
    Serial.print(F(" moisture:"));
    Serial.print(moisture);
    Serial.println(F(""));
  #endif
}

/* **************************************************************
 * build data to transmit in dataTX
 *
 * Suggested payload function for this data
 *
 * function Decoder(bytes, port) {
 *  var temp = parseInt(bytes[0]);
 *  var moisture = parseInt(bytes[1]);
 *  
 *  return { temp: temp,
 *           moisture: moisture };
 * }
 * *************************************************************/
void build_data() {
  dataTX[0] = tempC;
  dataTX[1] = moisture;
}

/* **************************************************************
 * radio code, typical would be init_node(), do_send(), etc
 * *************************************************************/
/* **************************************************************
 * init the Node
 * *************************************************************/
void init_node() {
  #ifdef VCC_ENABLE
     // For Pinoccio Scout boards
     pinMode(VCC_ENABLE, OUTPUT);
     digitalWrite(VCC_ENABLE, HIGH);
     delay(1000);
  #endif

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  #ifdef ACT_METHOD_ABP
    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
      // On AVR, these values are stored in flash and only copied to RAM
      // once. Copy them to a temporary buffer here, LMIC_setSession will
      // copy them into a buffer of its own again.
      uint8_t appskey[sizeof(APPSKEY)];
      uint8_t nwkskey[sizeof(NWKSKEY)];
      memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
      memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
      LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
      // If not running an AVR with PROGMEM, just use the arrays directly
      LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif

    #if defined(CFG_eu868)
      // Set up the channels used by the Things Network, which corresponds
      // to the defaults of most gateways. Without this, only three base
      // channels from the LoRaWAN specification are used, which certainly
      // works, so it is good for debugging, but can overload those
      // frequencies, so be sure to configure the full frequency range of
      // your network here (unless your network autoconfigures them).
      // Setting up channels should happen after LMIC_setSession, as that
      // configures the minimal channel set.
      // NA-US channels 0-71 are configured automatically
      LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
      LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
      LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
      // TTN defines an additional channel at 869.525Mhz using SF9 for class B
      // devices' ping slots. LMIC does not have an easy way to define set this
      // frequency and support for class B is spotty and untested, so this
      // frequency is not configured here.
    #elif defined(CFG_us915)
      // NA-US channels 0-71 are configured automatically
      // but only one group of 8 should (a subband) should be active
      // TTN recommends the second sub band, 1 in a zero based count.
      // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
      LMIC_selectSubBand(1);
    #endif

    #if defined(DEBUG)
      LMIC_disableChannel(1);
      LMIC_disableChannel(2);
      LMIC_disableChannel(3);
      LMIC_disableChannel(4);
      LMIC_disableChannel(5);
      LMIC_disableChannel(6);
      LMIC_disableChannel(7);
      LMIC_disableChannel(8);
    #endif
    
    // Enable data rate adaptation
     LMIC_setAdrMode(1);

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF7,14);
  #endif

  #ifdef ACT_METHOD_OTAA
    // got this fix from forum: https://www.thethingsnetwork.org/forum/t/over-the-air-activation-otaa-with-lmic/1921/36
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
  #endif

}


void sendCommand(unsigned char command)
{
  Wire.beginTransmission(0x3C); // oled adress
  Wire.write(0x80); // command mode
  Wire.write(command);
  Wire.endTransmission();
}

/* **************************************************************
 * send the message
 * *************************************************************/
void do_send() {

  Serial.print(millis());
  Serial.print(F(" Изпращане.. "));  
 

  send_message(&sendjob);

  // wait for send to complete
  Serial.print(millis());
  Serial.print(F(" Изчакване.. "));     
 
  while ( (LMIC.opmode & OP_JOINING) or (LMIC.opmode & OP_TXRXPEND) ) { os_runloop_once();  }
  Serial.print(millis());
  Serial.println(F(" Предаването завършено"));
}
  
/* *****************************************************************************
* send_message
* ****************************************************************************/
void send_message(osjob_t* j) {
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("TXRXЗабавяне, не се изпраща"));     
  } else {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, dataTX, sizeof(dataTX), 0);
    Serial.println(F("Packet опашка"));      
  }
}

/*******************************************************************************/
void onEvent (ev_t ev) {
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("Времето за сканиране изтече"));
      
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("Открит е LoRaWAN маяк"));
   
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("Липсва LoRaWAN маяк"));
   
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("Следене за LoRaWAN маяци"));
   
      break;
    case EV_JOINING:
      Serial.println(F("Присъединяване"));
    
      break;
    case EV_JOINED:
      Serial.println(F("Успешно присъединяване"));
      LMIC_setLinkCheckMode(0);
      break;
    case EV_RFU1:
      Serial.println(F("EV_RFU1"));
   
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("Неуспешно присъединяване"));
  
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("Неуспешно повторно присъединяване"));
   
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("Предаването завършено (изчакване на RX прозореца)"));
          if (LMIC.dataLen) {
        // data received in rx slot after tx
        Serial.print(F("Предадени дани: "));   
        Serial.write(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
        Serial.println();                
      }
      // schedule next transmission
          os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), send_message);
          delay(1000);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("Изгубено времево синхронизиране"));
   
      break;
    case EV_RESET:
      Serial.println(F("Ресет"));
    
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial.println(F("Предаването Завършено"));
   
      break;
    case EV_LINK_DEAD:
      Serial.println(F("Липса на мрежа (линк)"));
    
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("Активна мрежа (жив-линк)"));
   
      break;
    default:
      Serial.println(F("Непознато събитие "));
        break;

        

  }
    
}
