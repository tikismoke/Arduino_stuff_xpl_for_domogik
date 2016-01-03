#include <OneWire.h>
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <xPL.h>

/////////////////////////////
//VARS
#define DEBUG (1)

struct device {
  int index;
  byte addr[8];
  int model_s;
  int is_ds18x2x;
  float last_value;
};

xPL xpl;
unsigned long timer = 0; 
// Ethernet shield attached to pins 10, 11, 12, 13
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
IPAddress ip(192, 168, 0, XX);
IPAddress broadcast(XX, XX, XX, XX);
EthernetUDP Udp;

OneWire ds(9);  // on pin 10 (a 4.7K resistor is necessary)

struct device devices[8];
int device_count = 0;

void debug_byte(byte b) {
  if(DEBUG) {
    Serial.print(b, HEX);
  }
}

void debug_float(float f) {
  if(DEBUG) {
    Serial.print(f);
  }
}

void debug_int(int i) {
  if(DEBUG) {
    Serial.print(i);
  }
}

void debug(char* str) {
  if(DEBUG) {
    Serial.print(str); 
  }
}

void find_devices() {
  byte addr[8];
  int i, j;

  ds.reset_search();
  delay(250);
  i = 0;
  while(ds.search(addr)) {  
    
    devices[i].index = i;
    debug("Found device: ");
    
    if(OneWire::crc8(addr, 7) != addr[7]) {
      debug("Invalid checksum! Skipping.\n");
      continue; 
    }
    
    for(j=0; j < 8; j++) {
      devices[i].addr[j] = addr[j];
      debug("0x");
      debug_byte(addr[j]);
      if(j < 7) {
        debug(":"); 
      }
    }
    unsigned long *lval = (unsigned long *)addr;
    Serial.println(*lval);
  
    debug(" Type: ");

    devices[i].is_ds18x2x = 1;
    
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        debug("DS18S20 or DS1820");
        devices[i].model_s = 1;
        break;
      case 0x28:
        debug("DS18B20");
        devices[i].model_s = 0;
        break;
      case 0x22:
        debug("DS1822");
        devices[i].model_s = 0;
        break;
      default:
        debug("Not a DS18x2x family device.");
        devices[i].is_ds18x2x = 0;
        return;
    } 
    
    debug("\n");
    i++;
  }
  

  
  device_count = i;
}

/*
  This function based on the temperature reading example
  from the OneWire Arduino library example
*/
void read_temperature(struct device* dev) {
  int i;
  byte present = 0;
  byte data[12];
  float celsius;
  
  ds.reset();
  ds.select(dev->addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end
  
  delay(1000); // maybe 750ms is enough, maybe not
  
  // we might do a ds.depower() here, but the reset will take care of it.
  present = ds.reset();
  
  ds.select(dev->addr);
  ds.write(0xBE);         // Read Scratchpad

  // read 9 bytes;
  for ( i = 0; i < 9; i++) {
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (dev->model_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  if (celsius>-100){
    if (celsius<80){
      if (dev->last_value!=celsius){
        dev->last_value=celsius;
        Serial.print("Device: ");
        Serial.print(dev->index);
        unsigned long *lval = (unsigned long *)dev->addr;
        Serial.print(" : ");
        Serial.println(*lval);
        xPL_Message msg;
        //send xpl message
        msg.hop = 1;
        msg.type = XPL_TRIG;
        msg.SetTarget_P(PSTR("*"));
        msg.SetSchema_P(PSTR("sensor"), PSTR("basic"));
        delay(50);
        if (*lval==737420072){
          msg.AddCommand_P(PSTR("device"),PSTR("temp_aller_chauffage"));
          if (celsius<26){
            //alerte_nabz("0013d382e743");// liloublue
            //alerte_nabz("0019db0006d4");// tikismoke
          }
          Serial.print("Aller chaudiere");
        }
        else if (*lval==2335385640){
          msg.AddCommand_P(PSTR("device"),PSTR("temp_retour_chauffage"));
          Serial.print("Retour chaudiere");
        }
        else if (*lval==2504003624){
          msg.AddCommand_P(PSTR("device"),PSTR("temp_retour_plancher"));
          Serial.print("Retour plancher");
        }
        else if (*lval==2516377896){
          msg.AddCommand_P(PSTR("device"),PSTR("temp_aller_plancher"));
          Serial.print("Aller plancher");
        }
        else if (*lval==2335216168){
         msg.AddCommand_P(PSTR("device"),PSTR("temp_exterieur"));
         //correct sensor from 1.5°
         celsius=celsius-1.5;
         Serial.print("Temp Exterieur");
        }
        msg.AddCommand_P(PSTR("type"),PSTR("temp"));
        //Convert value received to char in var temp_chaudiere
        char temp_chaudiere[10];
        ftoa(temp_chaudiere,celsius, 2);
        if (temp_chaudiere!="0.6"){
          msg.AddCommand("current",temp_chaudiere);
          xpl.SendMessage(&msg);
        }
        Serial.print(" | Temp: ");
        Serial.print(temp_chaudiere);
        Serial.print("\n");  
       }
    }
  }
}

void SendUdPMessage(char *buffer)
{
    Udp.beginPacket(broadcast, xpl.udp_port);
    Udp.write(buffer);
    Udp.endPacket(); 
}

// Float support is hard on arduinos
// (http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1164927646) with tweaks
char *ftoa(char *a, double f, int precision)
{
  long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
  char *ret = a;
  long heiltal = (long)f;
  
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  return ret;
}

void setup(void) {
  // power on the temperature sensor
  // pin 8 is power pin for temperature sensor
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  Serial.begin(115200);
  Ethernet.begin(mac, ip);
  Udp.begin(xpl.udp_port);  
  xpl.SendExternal = &SendUdPMessage;  // pointer to the send callback 
  xpl.SetSource_P(PSTR("domogik"), PSTR("arduino"), PSTR("entree")); // parameters for hearbeat message
  Serial.print("arduino is at ");
  Serial.println(Ethernet.localIP());
  delay(2000); // Wait a bit so we can show the serial Serial
  
  find_devices();
  
}

void loop(void) {
   xpl.Process();  // heartbeat management
  Serial.println("Number of device found :");
  Serial.println(device_count);
  //5 probe present loop until all are found
  while (device_count!=5){
    Serial.println("Not find all device :");
    Serial.println("Number of device found :");    Serial.println(device_count);
    find_devices();
    delay(5000);
  }
  int i;
  for(i=0; i < device_count; i++) {
    Serial.println("New request");
    read_temperature(&(devices[i]));
  }
  //getojntoken();
 delay(10000);
}
