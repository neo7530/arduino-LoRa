#include <SPI.h>
#include <LoRa_cad.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

ISR(WDT_vect) {
}

volatile bool dio0_rise;
volatile bool dio1_rise;
volatile bool tx = false;

volatile uint8_t rxbuff[255];
volatile int len = 0;

// Scanning
//volatile uint8_t spread = 0;
//volatile bool scan = true;

byte dio0_pin = 2;
byte dio1_pin = 3;

//setup repeater things
const long frequency = 434000000;  // LoRa Frequency in Hz
const long bandwidth = 125000;  //Hz
int sf = 9;
int sf1 = sf;
const int cr = 5;
const int txpwr = 20;
const int preamble = 35;

int ctr = 0;
int repeat = 9800;

void setup() {
  // disable ADC
  ADCSRA = 0;

  Serial.begin(57600);
  while (!Serial);

  pinMode(dio0_pin, INPUT);
  pinMode(dio1_pin, INPUT);

  // Set sleep to full power down.  Only external interrupts or 
  // the watchdog timer can wake the CPU!
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // wdt sleep settings:
  // 10.8 Watchdog Timer
  WDTCSR |= 1<<WDCE | 1<<WDE;

  // Table 10-3. Watchdog Timer Prescale Select
  //WDTCSR = WDTO_8S; // 1<<WDP3 | 1<<WDP0;           // 8 seconds
  //WDTCSR = WDTO_4S; // 1<<WDP3;                     // 4 seconds
  //WDTCSR = WDTO_2S; // 1<<WDP2 | 1<<WDP1 | 1<<WDP0; // 2 seconds
  //WDTCSR = WDTO_1S; // 1<<WDP2 | 1<<WDP1;           // 1 seconds
  
  //WDTCSR = WDTO_30MS;
  WDTCSR = WDTO_15MS;

  WDTCSR |= 1<<WDIE;

  // Setup external interrupt INT0 and INT1
  //
  //12.2 Register Description
  //12.2.1 EICRA – External Interrupt Control Register A
  EICRA = 0;
  //EICRA |= (1<<ISC10); // Any logical change on INT1 generates an interrupt request.
  //EICRA |= (1<<ISC00); // Any logical change on INT0 generates an interrupt request.
  EICRA |= 1<<ISC11 | 1<<ISC10; // The rising edge of INT1 generates an interrupt request.
  EICRA |= 1<<ISC01 | 1<<ISC00; // The rising edge of INT0 generates an interrupt request.

  //12.2.2 EIMSK – External Interrupt Mask Register
  EIMSK = 0;
  EIMSK |= (1<<INT1); // Bit 1 – INT1: External Interrupt Request 1 Enable
  EIMSK |= (1<<INT0); // Bit 0 – INT0: External Interrupt Request 0 Enable

  if (!LoRa.begin(frequency)) {
    Serial.println(F("LoRa init failed. Check your connections."));
    while (true);                       // if failed, do nothing
  }

  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bandwidth);
  LoRa.setCodingRate4(cr);
  LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setPreambleLength(preamble);
  //LoRa.setTxPower(14,PA_OUTPUT_RFO_PIN); //RA-02 Module
  //LoRa.enableCrc();

  Serial.println(F("LoRa init succeeded."));
  Serial.println();
  Serial.println(F("LoRa Simple Gateway"));
  Serial.println(F("Only receive messages from nodes"));
  Serial.println(F("Tx: invertIQ enable"));
  Serial.println(F("Rx: invertIQ disable"));
  Serial.println();
  Serial.flush();
  
  

  // initiates the first cadMode
  LoRa.cadMode();
}

void go_to_sleep() {
   // reset wdt counter
   wdt_reset();

   //sleep_mode: Put the device into sleep mode, taking care of setting the SE bit before, and clearing it afterwards.
   sleep_mode();
}

ISR (INT0_vect){
  dio0_rise = true;
}

ISR (INT1_vect){
  dio1_rise = true;
}

void parse_packet(){
  // try to parse packet
  //scan = false;
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print(F("Received packet '"));

    // read packet
    len = 0;
    while (LoRa.available()) {
      rxbuff[len] = ((char)LoRa.read()& 0xFF);
      Serial.print(rxbuff[len],HEX);
      Serial.print(F(" "));
      len++;
    }
    tx = true;
    // print RSSI of packet
    Serial.print(F("' with RSSI "));
    Serial.println(LoRa.packetRssi());
  }  
}

void send_message(){
  Serial.println(F("TXing..."));
  LoRa.idle();                          // set standby mode
  LoRa.enableInvertIQ();                // active invert I and Q signals
  LoRa.beginPacket();
  for(int i=0;i<len;i++){
    LoRa.write(rxbuff[i]);
  }
  LoRa.endPacket();
  LoRa.idle();                          // set standby mode
  LoRa.disableInvertIQ();                // active invert I and Q signals

}


void loop() {
   if (LoRa.cadModeActive && dio0_rise){
      // dio0: CadDone
      // dio1: CadDetected
      
      if (dio1_rise){
         dio0_rise = false;
         dio1_rise = false;
         // prepare radio to receive a single packet
         LoRa.setRxSingle();
      } else {

         // nothing detected: wait before initiating the next cadMode
         // put both radio and microcontroller to sleep
         LoRa.sleep();
         go_to_sleep();

         
         // send a message periodically
         //static uint16_t send_message_counter{0};
         //send_message_counter++;
         if (tx){
            //send_message_counter = 0;
            send_message();
            tx = false;
            ctr++;
            //scan = true;
         }

  repeat++;
    if(repeat == 10000){
    sf = 12;
    LoRa.setSpreadingFactor(sf);
      for(int i = 0; i<6; i++){
        String message = "";
        message += "\xFF\xFF\xFF\xFF""\xFF\xFF\xFF\xFF""\xFF\xFF\xFF\xFF""Berlin-LoRa! ID: 0x80000001 ";
        message += "Test Gateway! Messages Repeated: ";
        message += ctr;
        message += " Spreading: ";
        message += sf;
        Serial.println(F("TXing..."));
        LoRa.idle();                          // set standby mode
        LoRa.enableInvertIQ();                // active invert I and Q signals
        LoRa.beginPacket();
        LoRa.print(message);
        LoRa.endPacket();
        LoRa.idle();                          // set standby mode
        LoRa.disableInvertIQ();                // active invert I and Q signals
        Serial.println(F("Beacon sent!"));
        repeat = 0;
        Serial.flush();
        sf--;
        LoRa.setSpreadingFactor(sf);
        delay(1000);
      }
        sf = sf1;
        LoRa.setSpreadingFactor(sf);
    }


         dio0_rise = false;
         dio1_rise = false;
         LoRa.cadMode();
      }
   } else if (LoRa.rxSingleMode){
      // dio0: RxDone
      // dio1: RxTimeout
      
      // only take action if either of the pins went high
      if (dio0_rise || dio1_rise){
         if (dio0_rise){
            parse_packet();
            Serial.flush();
         }
    
         dio0_rise = false;
         dio1_rise = false;
         LoRa.cadMode();
      }
   }

   go_to_sleep();
}
