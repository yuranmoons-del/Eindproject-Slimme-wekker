#define BLYNK_TEMPLATE_ID "TMPL5aEu6Tk0T"
#define BLYNK_TEMPLATE_NAME "Eindproject"
#define BLYNK_AUTH_TOKEN "EW_BXPL3qtj6aQj5kqeEcNoeogMXmtme"

char ssid[] = "embed";
char pass[] = "weareincontrol";

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <BH1750.h>
#include <time.h>

LiquidCrystal_I2C lcd(0x27,16,2);
BH1750 licht;
DHT dht(4,DHT11);

#define BUZZER 9
#define KNOP 10
#define LED 5

#define TRIG 12
#define ECHO 13

bool weekend = false;
bool handmatig = false;
bool laatsteKnop = HIGH;

bool alarmActief = false;
bool geblokkeerd = false;

int fase = 0;
unsigned long faseStart = 0;

bool alarmGestoptPrint = false; 

long meetAfstand() {
  digitalWrite(TRIG,LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG,LOW);
  long duur = pulseIn(ECHO,HIGH);
  long afstand = duur*0.034/2; 
  return afstand;
}

void alarmStoppen() {
  noTone(BUZZER);
  alarmActief = false;
  fase = 0;
  geblokkeerd = true; 
  Blynk.virtualWrite(V4,0); 

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Opgestaan!");
  lcd.setCursor(0,1);
  lcd.print("Wekker uit");
  alarmGestoptPrint = true;
  Serial.println("Opgestaan! Wekker uit");
}

void alarmStarten() {
  alarmActief = true;
  fase = 1;
  faseStart = millis();
  tone(BUZZER,500);
  Blynk.virtualWrite(V4,1);
}

void alarmUpdate() {
  if(!alarmActief) return;

  unsigned long nu = millis();
  unsigned long dt = nu - faseStart;

  if(meetAfstand()<6){
    alarmStoppen();
    return;
  }

  switch(fase){
    case 1: 
      if(dt>=3000){
        noTone(BUZZER);
        fase=2;
        faseStart=nu;
      }
      break;
    case 2:
      if(dt>=5000){
        fase=3;
        faseStart=nu;
        tone(BUZZER,1200);
      }
      break;
    case 3:
      if(dt>=3000){
        noTone(BUZZER);
        fase=4;
        faseStart=nu;
      }
      break;
    case 4: 
      if(dt>=5000){
        fase=3;
        faseStart=nu;
        tone(BUZZER,1200);
      }
      break;
  }
}

BLYNK_WRITE(V6){ if(param.asInt()==1) alarmStoppen(); } 
BLYNK_WRITE(V7){ handmatig=true; weekend=param.asInt(); }

void setup(){
  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();

  pinMode(KNOP,INPUT_PULLUP);
  pinMode(LED,OUTPUT);
  pinMode(TRIG,OUTPUT);
  pinMode(ECHO,INPUT);

  dht.begin();
  licht.begin();

  Serial.begin(115200);
  configTime(3600,0,"pool.ntp.org");
  Blynk.begin(BLYNK_AUTH_TOKEN,ssid,pass);
}

void loop(){
  Blynk.run();
  alarmUpdate();

  float temp=dht.readTemperature();
  float lux=licht.readLightLevel();

  bool knop=digitalRead(KNOP);
  if(knop==LOW && laatsteKnop==HIGH){
    handmatig=true;
    weekend=!weekend;
    Blynk.virtualWrite(V7,weekend);
  }
  laatsteKnop=knop;

  struct tm tijd;
  getLocalTime(&tijd);
  if(!handmatig) weekend=(tijd.tm_wday==0 || tijd.tm_wday==6);

  bool verwarmingAan=(weekend && temp<20);
  digitalWrite(LED,verwarmingAan);

  static unsigned long printStart=0;
  if(!alarmActief && !alarmGestoptPrint){ 
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("T:");
    lcd.print(temp);
    lcd.print(" L:");
    lcd.print((int)lux);
    lcd.setCursor(0,1);
    lcd.print(weekend ? "WKND" : "WEEK");
  }

  if(alarmGestoptPrint){
    if(printStart==0) printStart=millis();
    else if(millis()-printStart>=5000){
      alarmGestoptPrint=false;
      printStart=0;
    }
  }

  Blynk.virtualWrite(V1,temp);
  Blynk.virtualWrite(V2,lux);
  Blynk.virtualWrite(V3,weekend); 
  Blynk.virtualWrite(V8,!weekend);
  Blynk.virtualWrite(V5,verwarmingAan);

  if(lux<5) geblokkeerd=false;

  if(!alarmActief && !geblokkeerd && weekend && lux>50) alarmStarten();
  if(!alarmActief && !geblokkeerd && !weekend && lux>=30) alarmStarten();

  delay(100);
}
