
/*
 * TRL  ver 1.1c   6Jul2016
 * 
 * 
 * */



 
 /* *******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello, world!", that
 * will be processed by The Things Network server.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1, 
*  0.1% in g2). 
 *
 * Change DEVADDR to a unique address! 
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h, default is:
 *   #define CFG_sx1272_radio 1
 * for SX1272 and RFM92, but change to:
 *   #define CFG_sx1276_radio 1
 * for SX1276 and RFM95.
 *
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <stdarg.h>

#include <Wire.h>         //http://arduino.cc/en/Reference/Wire



/* ************************************************************************************** */
/*  Enable debug prints to serial monitor on port 0 */
#define MY_DEBUG            // used by MySensor
#define MY_DEBUG1           // used in this program, level 1 debug
#define MY_DEBUG2           // used in this program, level 2 debug
#define snprintf_P(s, f, ...) snprintf((s), (f), __VA_ARGS__)

#define SKETCHNAME "LMiC Test"
#define SKETCHVERSION "1.1c"

/* ************************************************************************************** 
 *          TTN Keys
 * ************************************************************************************** */

//const char *devAddr = "26021E31";   // device 1
//const char *devAddr = "26021B11";   // device 2
//const char *nwkSKey = "CB935C8DA69E93AE0BA64293A4CD02E9";
//const char *appSKey = "5E935EB19334D0D6B38032E96C299D2A";


/* ************************************************************************************** */
// LoRaWAN Application identifier (AppEUI)
// Not used in this example
static const u1_t APPEUI[8]  = { 0x70, 0xb3, 0xd5, 0x7e, 0xd0, 0x00, 0x03, 0x8b };

/* ************************************************************************************** */
// LoRaWAN DevEUI, unique device ID (LSBF)
// Not used in this example
static const u1_t DEVEUI[8]  = { 0x00, 0x04, 0xa3, 0xff, 0xfe, 0x31, 0x00, 0x02 };


/* ************************************************************************************** */
// LoRaWAN NwkSKey, network session key (NwkSKey)
// Use this key for The Things Network
static const u1_t DEVKEY[16] = {0xCB, 0x93, 0x5C, 0x8D, 0xA6, 0x9E, 0x93, 0xAE, 0x0B, 0xA6, 0x42, 0x93, 0xA4, 0xCD, 0x02, 0xE9 };
//static const u1_t DEVKEY = 0xCB935C8DA69E93AE0BA64293A4CD02E9;
// const char *nwkSKey = "CB935C8DA69E93AE0BA64293A4CD02E9";

/* ************************************************************************************** */
// LoRaWAN AppSKey, application session key (AppSKey)
// Use this key to get your data decrypted by The Things Network
static const u1_t ARTKEY[16] = { 0x5E, 0x93, 0x5E, 0xB1, 0x93, 0x34, 0xD0, 0xD6, 0xB3, 0x80, 0x32, 0xE9, 0x6C, 0x29, 0x9D, 0x2A };
//static const u1_t ARTKEY = 0x5E935EB19334D0D6B38032E96C299D2A;
//const char *appSKey = "5E935EB19334D0D6B38032E96C299D2A";

/* ************************************************************************************** */
// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
static const u4_t DEVADDR = 0x26021B11 ; // <-- Change this address for every node!
//const char *devAddr = "26021E31";   // device 1
//const char *devAddr = "26021B11";   // device 2


//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui (u1_t* buf) {
    memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui (u1_t* buf) {
    memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey (u1_t* buf) {
    memcpy(buf, DEVKEY, 16);
}

uint8_t mydata[] = "Hello, World!";
static osjob_t sendjob;
static osjob_t keepalivejob;


#ifdef __AVR_ATmega1284P__      // MoteinoMega LoRa
// Pin mapping
lmic_pinmap pins = 
{
  .nss  = 4,
  .rxtx = 0,    // Not connected on RFM92/RFM95
  .rst  = 3,    // Needed on RFM92/RFM95 ??
  .dio  = {2, 22, 21},
};
#define myLED 15
#define MyFlashCS 23

#elif __SAMD21G18A__            // RocketScream
// Pin mapping
lmic_pinmap pins = 
{
  .nss  = 5,
  .rxtx = 0,    // Not connected on RFM92/RFM95
  .rst  = 0,    // Needed on RFM92/RFM95 ??
  .dio  = {2, 6, 0},
};

#define myLED 13  // Is 13 on M0
#define MyFlashCS 4

#else
#error Wrong Processor defined
#endif



/* ************************************************************************************** */
#ifdef MY_DEBUG
void hwDebugPrint(const char *fmt, ... ) 
{
  char fmtBuffer[300];
  va_list args;
  va_start (args, fmt );
  va_end (args);
#ifdef __SAMD21G18A__            // RocketScream
  vsnprintf(fmtBuffer, 299, fmt, args); 
#else   
  vsnprintf_P(fmtBuffer, 299, fmt, args);
#endif 
  va_end (args);
  Serial.print(fmtBuffer);
  Serial.flush();
}



#endif

/* ************************************************************************************** */
/* These are use for local debug of code, hwDebugPrint is defined in MyHwATMega328.cpp */
#ifdef MY_DEBUG
#define debug(x,...) hwDebugPrint(x, ##__VA_ARGS__)
#else
#define debug(x,...)
#endif

#ifdef MY_DEBUG1
#define debug1(x,...) hwDebugPrint(x, ##__VA_ARGS__)
#else
#define debug1(x,...)
#endif

#ifdef MY_DEBUG2
#define debug2(x,...) hwDebugPrint(x, ##__VA_ARGS__)
#else
#define debug2(x,...)
#endif

/* ************************************************************************************** */
//debug(PSTR(" %s \n"), compile_file);

/* ************************************************************************************** */
void onEvent (ev_t ev) 
{
    //debug_event(ev);

    switch(ev) 
    {
      // scheduled data sent (optionally data received)
      // note: this includes the receive window!
      case EV_TXCOMPLETE:
          // use this event to keep track of actual transmissions
          debug(PSTR("Event EV_TXCOMPLETE, time: %u \n"), millis() / 1000);

          
          if(LMIC.dataLen) 
          { // data received in rx slot after tx
//            debug_buf(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
              debug(PSTR("Data Received! \n"));
          }
          break;
          
       case EV_JOIN_FAILED:
          debug(PSTR("Event EV_JOIN_FAILED, time: %u \n"), millis() / 1000);
          break;

       case EV_RXCOMPLETE:
          debug(PSTR("Event EV_RXCOMPLETE, time: %u \n"), millis() / 1000);
          break;

       case EV_LINK_DEAD:
          debug(PSTR("Event EV_LINK_DEAD, time: %u \n"), millis() / 1000);
          break;

       case EV_RESET:
          debug(PSTR("Event EV_RESET, time: %u \n"), millis() / 1000);
          break;

       case EV_JOINING:
          debug(PSTR("Event EV_JOINING, time: %u \n"), millis() / 1000);
          break;

       case EV_REJOIN_FAILED:
          debug(PSTR("Event EV_REJOIN_FAILED, time: %u \n"), millis() / 1000);
          break;

       case EV_JOINED:
          debug(PSTR("Event EV_JOINED, time: %u \n"), millis() / 1000);
          break;

       case EV_BEACON_FOUND:
          debug(PSTR("Event EV_BEACON_FOUND, time: %u \n"), millis() / 1000);
          break;

       case EV_SCAN_TIMEOUT:
          debug(PSTR("Event EV_SCAN_TIMEOUT, time: %u \n"), millis() / 1000);
          break;

       case EV_LOST_TSYNC:
          debug(PSTR("Event EV_LOST_TSYNC, time: %u \n"), millis() / 1000);
          break;

       case EV_BEACON_TRACKED:
          debug(PSTR("Event EV_BEACON_TRACKED, time: %u \n"), millis() / 1000);
          break;

       case EV_BEACON_MISSED:
          debug(PSTR("Event EV_BEACON_MISSED, time: %u \n"), millis() / 1000);
          break;

       default:
          debug(PSTR("Event EV_OTHER, time: %u \n"), millis() / 1000);
          break;
    }
}


/* ************************************************************************************** */
#define KeepAliveTime   60      // time is sec to restart this task
void do_keepalive(osjob_t* k)
{
      debug(PSTR("\n***  Keep-Alive-Time: %u \n"), millis() / 1000);
      
   // Re-Schedule a timed job to run this task again at the given timestamp (absolute system time)
    os_setTimedCallback(k, os_getTime()+sec2osticks(KeepAliveTime), do_keepalive); 
} 


/* ************************************************************************************** */
void do_send(osjob_t* j)
{
      debug(PSTR("\nTime: %u\nSend, txCnhl: %u \n"), millis() / 1000, LMIC.txChnl);
      debug(PSTR("Opmode check: "));
      
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & (1 << 7)) 
    {
      debug(PSTR("OP_TXRXPEND, <-- Msg not sent yet \n"));
    } 
    else 
    {
      debug(PSTR("ok to send\n"));
      // Prepare upstream data transmission at the next possible time.
      LMIC.pendTxConf = true;
      LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
   }
    
    // Schedule a timed job to run this task again at the given timestamp (absolute system time)
    os_setTimedCallback(j, os_getTime()+sec2osticks(20), do_send);       
}

/* ************************************************************************************** */
void setup() 
{
  Serial.begin(115200);

#ifdef __AVR_ATmega1284P__      // use for Moteino Mega
  debug(PSTR("\n ** LMiC Starting on a MoteinoMega **\n") );
    // de-select Flash 
  pinMode(MyFlashCS, OUTPUT);
  digitalWrite(MyFlashCS, HIGH);
  // set the LED digital pin as output:
  pinMode(myLED, OUTPUT);
  digitalWrite(myLED, 0);
  
#elif __SAMD21G18A__            // RocketScream
  debug(PSTR("\n ** LMiC Starting on a RocketScream M0**\n") );
  // de-select Flash 
  pinMode(MyFlashCS, OUTPUT);
  digitalWrite(MyFlashCS, HIGH);
  // set the LED digital pin as output:
  pinMode(myLED, OUTPUT);
  digitalWrite(myLED, 0);
#endif

  const char compile_file[]  = __FILE__ ;
  debug(PSTR(" ** %s %s\n"), SKETCHNAME, SKETCHVERSION);
  debug(PSTR(" ** %s \n"), compile_file);
  const char compile_date[]  = __DATE__ ", " __TIME__;
  debug(PSTR(" ** %s \n\n"), compile_date);
  debug(PSTR(" ** Node ID: 0x%lx\n\n"), DEVADDR);

 
  
  // LMIC init
  debug(PSTR("***  LMiC os_init \n"));
  os_init();
  
  // Reset the MAC state. Session and pending data transfers will be discarded.
  debug(PSTR("***  LMIC_reset \n"));
  LMIC_reset();
  
  // Set static session parameters. Instead of dynamically establishing a session 
  // by joining the network, precomputed session parameters are provided.
  debug(PSTR("***  LMIC_setSession \n"));
  LMIC_setSession (0x1, DEVADDR, (uint8_t*)DEVKEY, (uint8_t*)ARTKEY);
  
  // Disable data rate adaptation
  debug(PSTR("***  LMIC_setAdrMode \n"));
  LMIC_setAdrMode(0);
  
  // Disable link check validation
  debug(PSTR("***  LMIC_setLinkCheckMode \n"));
  LMIC_setLinkCheckMode(0);
  
  // Disable beacon tracking
  debug(PSTR("***  LMIC_disableTracking \n"));
  LMIC_disableTracking ();
  
  // Stop listening for downstream data (periodical reception)
  debug(PSTR("***  LMIC_stopPingable \n"));
  LMIC_stopPingable();
  
  // Set data rate and transmit power (note: txpow seems to be ignored by the library)
  debug(PSTR("***  LMIC_setDrTxpow \n"));
  LMIC_setDrTxpow(DR_SF7,20);

// for now only TX on ch 0 , disable all others
  debug(PSTR("***  LMIC_disableChannel, Using Ch = 0 \n"));

  for( u1_t i=1; i<64; i++ ) 
    {
      LMIC_disableChannel(i);
    }

 // do these once, they will re schedule themselves
    do_send(&sendjob);
    do_keepalive(&keepalivejob);
}

   boolean toggle = false;
   
/* ************************************************************************************** */
void loop() 
{

  os_runloop_once();      // run the LMIC scheduler
 
 }

