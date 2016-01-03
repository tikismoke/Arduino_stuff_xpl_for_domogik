#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <xPL.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <Tools.h>

/////////////////////////////
//VARS
xPL xpl;
unsigned long timer = 0; 
// Ethernet shield attached to pins 10, 11, 12, 13
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
IPAddress ip(192, 168, 0, XX);
IPAddress broadcast(XX, XX, XX, XX);
EthernetUDP Udp;
// Data wire is plugged into pin 9 on the Arduino with a 4.7k between data and 5V
#define ONE_WIRE_BUS 9
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
//the time we give the PIR sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 30;        
//the time when the sensor outputs a low impulse
long unsigned int lowIn;         
//the amount of milliseconds the sensor has to be low 
//before we assume all motion has stopped
long unsigned int pause = 500;  
//boolean lockLow = true;
//boolean takeLowTime;
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    // variable for reading the pin status

int pir_Pin = 2;    //the digital pin 2 connected to the  sensor's output
int led_rouge_Pin = 8;    //Use digital outpout 8 for a red led
int led_verte_Pin = 7;    //Use digital outpout 7 for a green led
int led_jaune_Pin = 6;    //Use digital outpout 6 for a yellow led
int photocellPin = 0; // the cell and 10K pulldown are connected to A0
int photocellReading; // the analog reading from the analog resistor divider
int etat_rouge_DEL = LOW;
int etat_verte_DEL = LOW;
int etat_jaune_DEL = LOW;

void setup()
{
  pinMode(pir_Pin, INPUT);
  pinMode(led_rouge_Pin,OUTPUT);
  pinMode(led_verte_Pin,OUTPUT);
  pinMode(led_jaune_Pin,OUTPUT);
  digitalWrite(pir_Pin, LOW);
  Serial.begin(115200);
  Ethernet.begin(mac, ip);
  Udp.begin(xpl.udp_port);  
  sensors.begin();
  xpl.SendExternal = &SendUdPMessage;  // pointer to the send callback 
  xpl.SetSource_P(PSTR("domogik"), PSTR("arduino"), PSTR("legoenfant")); // parameters for hearbeat message
  Serial.print("arduino is at ");
  Serial.println(Ethernet.localIP());
  //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
    for(int i = 0; i < calibrationTime; i++){
      Serial.print(".");
      delay(1000);
      }
    Serial.println(" done");
    Serial.println("SENSOR ACTIVE");
    delay(50);
 }

void loop()
{
  xpl.Process();  // heartbeat management
  pir();    //look PIR state and send xpl message if needed
  digitalWrite(led_rouge_Pin,etat_rouge_DEL);        // Utiliser la valeur de la variable etatDEL pour controler la broche led_rouge_PIN.
  digitalWrite(led_verte_Pin,etat_verte_DEL);        // Utiliser la valeur de la variable etatDEL pour controler la broche led_verte_PIN.
  digitalWrite(led_jaune_Pin,etat_jaune_DEL);        // Utiliser la valeur de la variable etatDEL pour controler la broche led_jaune_PIN.
  // Example of sending an xPL Message every 60 second
   if ((millis()-timer) >= 60000){
     lumiere(); //send UV xpl message
     temperature(); //send T° 1-wire xpl message
     timer = millis();  
   } 
}

void SendUdPMessage(char *buffer)
{
    Udp.beginPacket(broadcast, xpl.udp_port);
    Udp.write(buffer);
    Udp.endPacket(); 
}
//fonction pour PIR détection mouvement et xpl
void pir(){
  val = digitalRead(pir_Pin);  // read input value
  if (val == HIGH) {            // check if the input is HIGH
    etat_rouge_DEL = HIGH;  // turn LED ON
    if (pirState == LOW) {
      xPL_Message msg;
     //send xpl message
      msg.hop = 1;
      msg.type = XPL_TRIG;
      msg.SetTarget_P(PSTR("*"));
      msg.SetSchema_P(PSTR("lighting"), PSTR("device"));
      msg.AddCommand_P(PSTR("device"),PSTR("movelegoenfant"));
      msg.AddCommand("level","0"); //100 pour OFF
      xpl.SendMessage(&msg);
     //send xpl message
     //makes sure we wait for a transition to LOW before any further output is made:
      pirState = HIGH;
      Serial.println("---");
      Serial.print("motion detected at ");
      Serial.print(millis()/1000);
      Serial.println(" sec"); 
      delay(50);
    }         
  }
  else {
      //Change Led status
      etat_rouge_DEL = LOW;
      if (pirState == HIGH){
        // we have just turned off
        pirState = LOW;
        xPL_Message msg;
        //send xpl message
        msg.hop = 1;
        msg.type = XPL_TRIG;
        msg.SetTarget_P(PSTR("*"));
        msg.SetSchema_P(PSTR("lighting"), PSTR("device"));
        msg.AddCommand_P(PSTR("device"),PSTR("movelegoenfant"));
        msg.AddCommand("level","100"); //100 pour OFF
        xpl.SendMessage(&msg);
        Serial.print("motion ended at ");
        Serial.print(millis()/1000);
        Serial.println(" sec"); 
      }
  }
}
//fonction pour luminosité et xpl
void lumiere() {
  photocellReading = analogRead(photocellPin);
  Serial.print("Analog reading = ");
  Serial.print(photocellReading); // the raw analog reading
  // We'll have a few threshholds, qualitatively determined
  if (photocellReading < 10) {
    Serial.println(" - Noir");    
    //Change Led status
    etat_verte_DEL = LOW;
    etat_jaune_DEL = LOW;
  } else if (photocellReading < 200) {
    Serial.println(" - Sombre");
    //Change Led status
    etat_verte_DEL = HIGH;
    etat_jaune_DEL = LOW;
  } else if (photocellReading < 500) {
    Serial.println(" - Lumiere");
    //Change Led status
    etat_verte_DEL = HIGH;
    etat_jaune_DEL = HIGH;
  } else if (photocellReading < 800) {
    Serial.println(" - Lumineux");
    //Change Led status
    etat_verte_DEL = HIGH;
    etat_jaune_DEL = HIGH;
  } else {
    Serial.println(" - Tres lumineux");
    //Change Led status
    etat_verte_DEL = HIGH;
    etat_jaune_DEL = HIGH;
  }
  char photocell_lego_enfant[10];
  ftoa(photocell_lego_enfant,photocellReading, 2);
  xPL_Message msg;
  //send xpl message
      msg.hop = 1;
      msg.type = XPL_TRIG;
      msg.SetTarget_P(PSTR("*"));
      msg.SetSchema_P(PSTR("sensor"), PSTR("basic"));
      msg.AddCommand_P(PSTR("device"),PSTR("lumlegoenfant"));
      msg.AddCommand_P(PSTR("type"),PSTR("illuminance")); //need to convert in %
      msg.AddCommand("current",photocell_lego_enfant); 
      xpl.SendMessage(&msg);
  delay(50);
}
//fonction pour t° onewire et xpl
void temperature()
{
  sensors.requestTemperatures(); // Send the command to get temperatures
     Serial.print("sensors.requestTemperatures ");
     float temponewire = sensors.getTempCByIndex(0);
     char temp_lego_enfant[10];
     ftoa(temp_lego_enfant,temponewire, 2);
     Serial.println(temp_lego_enfant);
  xPL_Message msg;
  //send xpl message
     msg.hop = 1;
     msg.type = XPL_TRIG;
     msg.SetTarget_P(PSTR("*"));
     msg.SetSchema_P(PSTR("sensor"), PSTR("basic"));
     msg.AddCommand_P(PSTR("device"),PSTR("templegoenfant"));
     msg.AddCommand_P(PSTR("type"),PSTR("temp"));
     msg.AddCommand("current",temp_lego_enfant);
     xpl.SendMessage(&msg);
     delay(50);
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

