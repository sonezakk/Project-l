#include <ESP8266WiFi.h>
#include <SPI.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ACROBOTIC_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FirebaseArduino.h>
#include <ThingSpeak.h>
#include <BlynkSimpleEsp8266.h>      // BLYNK
#include <TridentTD_LineNotify.h>    //LINE
#define OLED_RESET -1
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define pgm_read_float(addr)            pgm_read_float_aligned(addr)
#define ONE_WIRE_BUS D4
#define DS18B20 D4

Adafruit_ADS1015 ads;      // ADS1115  12BIT  //
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const  float SaturationValueTab[41] PROGMEM = {      //saturation dissolved oxygen concentrations at various temperatures
  14.46, 14.22, 13.82, 13.44, 13.09,
  12.74, 12.42, 12.11, 11.81, 11.53,
  11.26, 11.01, 10.77, 10.53, 10.30,
  10.08, 9.86,  9.66,  9.46,  9.27,
  9.08,  8.90,  8.73,  8.57,  8.41,
  8.25,  8.11,  7.96,  7.82,  7.69,
  7.56,  7.43,  7.30,  7.18,  7.07,
  6.95,  6.84,  6.73,  6.63,  6.53,
  6.41,
};
//Timer//
WidgetLED led(V3);
WidgetLED ledlink(V7);
BlynkTimer timer;
//
//wifi//
//const char *ssid = "WSNCOM";       // SSID is set
//const char *password = "1q2w3e4r"; // Password is set
//const char* ssid = "iot";             // SSID is set
//const char* password = "C0mputinG";
const char *ssid = "tonesone2";       // SSID is set
const char *password = "0910345985";


//blynk//
char auth[] = "JX0m0y_m5FomW9eEFnhhZK7pTfNlA8L9";
//thinkspeak//
unsigned long  myChannelNumber = 906994 ;
const char   * myWriteAPIKey      = "CMXEW2OZ01GTM63S";     // thingspeak API key,
//const char* ssid      = "Tonson";
//const char* password  = "0819581355";
const char* server    = "api.thingspeak.com";
WiFiClient client;
//firebase//
#define FIREBASE_HOST "phtest001-c2161.firebaseio.com"
#define FIREBASE_AUTH "heujtA2uqm5EqzeW5OtIiwBRUNalRXI2lgklDcnn"
//line//
#define LINE_TOKEN  "jhtwVDXhh4MHTj7EaQgZeTCCVGJrbdGYtQ2215YL4yu"
//time//
char ntp_server1[20] = "pool.ntp.org";
char ntp_server2[20] = "time.nist.gov";
char ntp_server3[20] = "time.uni.net.th";

int timezone = 7 * 3600; //grobal time
int dst = 0;
//int adc = A0;
int relayD0 = D0;
int statusmorter = 0;
//Blynk//

void setup()
{

  Serial.begin(115200);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
  Serial.println("");
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  configTime(timezone, dst, ntp_server1, ntp_server2, ntp_server3);
  Serial.println("\nWaiting for time");
  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.display();
  display.setTextColor(WHITE, BLACK);
  pinMode(relayD0, OUTPUT);
  sensors.begin();
  ads.begin();           // run ads1115
  Blynk.begin(auth,  ssid, password);

  timer.setInterval(1000L , onoffsensor);   //on-off  1s on app //
  LINE.setToken(LINE_TOKEN);
  ThingSpeak.begin(client);
  digitalWrite(D0, HIGH);
  sensorDo();
}

void loop()
{

  static unsigned long Showblynk = millis();
  if ((millis() - Showblynk) > 1500)
  {
    Showblynk = millis();
    Blynk.run();
    timer.run();
  }

  static unsigned long Showtime = millis();


  if ((millis() - Showtime) > 900000)
  { //1*60*1000
    Showtime = millis();
    time_t now = time(nullptr);
    struct tm *p_tm = localtime(&now);
    Serial.print(daynows());
    Serial.print("\t");
    Serial.println(timenows());
    oledtime();
    oledstatus();
    //LINE.notify(" Do : " + String(sensorDo()) + " mg/L" + "temp : " + String(sensorTemp()) + "'C");
    moterdelay(sensorDo(), 5.00, 4.50);

    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    //thinkspeakconnection(sensorPh(), sensorDo(), sensorTemp());


  }

}

//process
void moterdelay(float data, float max, float min)
{
  if (data >= max)
  {
    controlmoter(0);
    led.off();
    LINE.notify("ปั้มหยุดทำงาน");
    //LINE.notify(" Do : " + String(sensorDo()) + " mg/L" + "temp : " + String(sensorTemp()) + "'C");
  }
  if (data <= min)
  {
    controlmoter(1);
    led.on();
    LINE.notify("ปั้มกำลังทำงาน");
   // LINE.notify(" Do : " + String(sensorDo()) + " mg/L" + "temp : " + String(sensorTemp()) + "'C");
  }
}

//read ph
float sensorPh()
{
  int16_t  val = ads.readADC_SingleEnded(0);
  float PH = (val * 0.0067) + 2.6002;
  //float PH = (val * 0.0211) + 2.5819;
  char PH1[7];
  // ออกจอ OLED//
  oleddisplay("PH", PH);
  dtostrf(PH, 4, 2, PH1);
  //  serialprint("PH OUTPUT > " + String(PH));
  Serial.println("Analog read PH : " + (String)val + " PH  : " + (String)PH);
  sendfirebase("PH", PH);
  //Blynk.virtualWrite(V0, PH1);
  //ThingSpeak.setField(1, PH);
  return PH;

}

// read DO
float sensorDo()
{
  char DoBlynk[16];
  float calibrate = 0.00;
  int16_t  sensorValue = ads.readADC_SingleEnded(1);
  float doValue = 0.0;
  float temp = sensorTemp();
  float p;
  // Serial.println("DO :"   + (String)sensorValue + "volts : " +(String)volts + "DO : "+(String)Do);
  p = SaturationValueTab[0];
  doValue = pgm_read_float(&SaturationValueTab[0] + (int)temp);
  float volts = (5000 * ((float)sensorValue / 1024.00)) ;
  float Do = (doValue * volts ) / 1127.6 ;
  Serial.println( "temp : = " + (String)temp);
  //Serial.println(readtemp());
  oleddisplay("DO", Do);
  Serial.println("DO : " + (String)Do);
  sendfirebase("DO", Do);
  dtostrf(Do, 2, 2 , DoBlynk);
  Blynk.virtualWrite(V1, DoBlynk);
  ThingSpeak.setField(2, Do);
  LINE.notify(" Do : " + String(Do) + " mg/L" + "temp : " + String(temp) + "'C");
  return Do;
}

// temp
float sensorTemp()
{

  float temp;
  char  tempBlynk[16];
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);
  sendfirebase("Temp", temp);
  dtostrf(temp, 2, 2 , tempBlynk);
  Blynk.virtualWrite(V2, tempBlynk);
  ThingSpeak.setField(3, temp);
  return temp;
}

// control morter
void controlmoter(int status)
{
  sendfirebase("MO", status);
  statusmorter = status;
  digitalWrite(relayD0, status);
  serialprint("morter status > " + String(status));
  ThingSpeak.setField(4, status);


}

// firebase
void sendfirebase(String type, float val)
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root[type] = val;
  root["DAY"] = daynows() + " " + timenows();
  String name = Firebase.push(type, root);
}

// แสดงจอ Serial
void serialprint(String msg)
{
  Serial.println("debug  : " + msg);
}

// OLEDTIME//
void oledtime()
{
  display.setTextSize(1);
  display.setCursor(0, 1);
  display.print("DAY  : ");
  display.print(daynows());
  display.setCursor(0, 10);
  display.print("TIME : ");
  display.print(timenows());
  display.display();
}

// oledsensor
void oleddisplay(String type, float msg)
{
  if (type == "PH")
  {
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(type + ":");
    display.print(msg);
    display.display();
  }

  if (type == "DO")
  {
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.print(type + ":");
    display.print(msg);
    display.display();
  }
}

//oledmorter
void oledstatus()
{
  display.setTextSize(1);
  display.setCursor( 100, 50);
  //display.print("MO :");
  if (statusmorter == 1)
  {
    display.print(" ON ");
  }
  else
  {
    display.print(" OFF ");
  }
}

// วันที่//
String daynows()
{
  time_t now = time(nullptr);
  struct tm *day = localtime(&now);
  String daynow = "";
  daynow += String(day->tm_mday);
  daynow += "/";
  daynow += String(day->tm_mon + 1);
  daynow += "/";
  daynow += String(day->tm_year + 1900 + 543);
  return daynow;
}

//เวลา//
String timenows()
{
  time_t now = time(nullptr);
  struct tm *times = localtime(&now);
  String timenow = "";
  timenow += String(times->tm_hour);
  timenow += ":";
  timenow += String(times->tm_min);
  timenow += ":";
  timenow += String(times->tm_sec);
  return timenow;

}


//
void onoffsensor()
{
  if (ledlink.getValue())
  {
    ledlink.off();
  }
  else {
    ledlink.on();
  }
}
