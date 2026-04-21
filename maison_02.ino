#include <LCD_I2C.h>
#include <OneButton.h>
#include <DHT.h>
#include <HCSR04.h>
#include <AccelStepper.h>

#define MOTOR_INTERFACE_TYPE 4
#define IN_1 31
#define IN_2 33
#define IN_3 35
#define IN_4 37

int LED_PIN = 9;
static float distance = 22.0;
float temp = 0.0;
float hum = 0.0;
unsigned short luminosity = 0;
static int minVal = 1023;
static int maxVal = 0;
int readResistance;
int motorOpening = 2038;
int motorClosing = 0;
bool ledStatePin = false;
bool clickState = false;
bool doubleClickState = false;
int distancePercent;
int minLevel = 20;
int maxLevel = 25;

#define DHTPIN 7
#define DHTTYPE DHT11
#define TRIGGER_PIN 12
#define ECHO_PIN 11
LCD_I2C lcd(0x27, 16, 2);
#define PIN_INPUT 4

AccelStepper myStepper(MOTOR_INTERFACE_TYPE, IN_1, IN_3, IN_2, IN_4);
OneButton button(PIN_INPUT, true, true);
HCSR04 hc(TRIGGER_PIN, ECHO_PIN);
DHT dht(DHTPIN, DHTTYPE);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  myStepper.setMaxSpeed(500);      // Vitesse max en pas/seconde
  myStepper.setAcceleration(100);  // Accélération en pas/seconde²
  myStepper.setSpeed(200);         // Vitesse constante en pas/seconde
  myStepper.setCurrentPosition(2038);
  dht.begin();
  button.attachClick(click);
  button.attachDoubleClick(doubleClick);
  lcd.begin();
  lcd.backlight();
}
enum EtatAppli { ETAT_OUVERTURE,
                 ETAT_FERMETURE,
                 ETAT_FERME,
                 ETAT_OUVERT,
                 ETAT_ARRET };
EtatAppli etatActuel = ETAT_FERMETURE;
enum EtatLCD { ETAT_DISTANCE,
               ETAT_CALIBRATION,
               ETAT_TEMPERATURE,
               ETAT_VANNE,
};
EtatLCD etatLcd = ETAT_VANNE;

void loop() {
  unsigned long currentTime = millis();
  button.tick();
  readResistance = analogRead(A0);
  distancePercent = map(myStepper.currentPosition(), motorClosing, motorOpening, 0, 100);
  readDistance(currentTime);
  displayInMonitor(currentTime);
  bool light = resistanceValue(currentTime);
  ledState(light);
  readDHTValue(currentTime);
  lcdState(currentTime);
}
void lcdState(unsigned long ct) {
  switch (etatLcd) {
    case ETAT_DISTANCE:
      etatDistance(ct);
      break;
    case ETAT_TEMPERATURE:
      etatTemperature(ct);
      break;
    case ETAT_CALIBRATION:
      etatCalibration(ct);
      break;
    case ETAT_VANNE:
      valveState(ct);
      break;
  }
}
void valveState(unsigned long ct) {
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    if (distance < minLevel && etatActuel != ETAT_OUVERT) {
      etatActuel = ETAT_OUVERTURE;
    } else if (distance > maxLevel && etatActuel != ETAT_FERME) {
      etatActuel = ETAT_FERMETURE;
    }
  }

  switch (etatActuel) {
    case ETAT_OUVERTURE:
      etatOuverture(ct, motorOpening);
      break;
    case ETAT_FERMETURE:
      etatFermeture(ct, motorClosing);
      break;
    case ETAT_OUVERT:
      etatOuvert(ct);
      break;
    case ETAT_FERME:
      etatFerme(ct);
      break;
    case ETAT_ARRET:
      etatArret(ct);
      break;
  }
}
bool resistanceValue(unsigned long ct) {
  static unsigned long lastTime = 0;
  int rate = 1000;
  int rateLight = 30;
  bool lightState = false;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    luminosity = map(readResistance, minVal,maxVal, 0, 100);
    if (luminosity < rateLight) {
      lightState = true;
    } else {
      lightState = false;
    }
  }
  return lightState;
}

void readDHTValue(unsigned long ct) {
  static unsigned long lastTime = 0;
  int rate = 5000;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    hum = dht.readHumidity();
    // Lecture de la température en °C
    temp = dht.readTemperature();
  }
}
void readDistance(unsigned long ct) {
  static unsigned long lastTime = 0;
  int rate = 250;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    distance = hc.dist();
  }
}
void distanceForMotorState(unsigned long ct) {
  static unsigned long lastTime = 0;
  int rate = 200;
  if (ct - lastTime >= rate) {
    if (distance < minLevel && etatActuel != ETAT_OUVERT) {
      etatActuel = ETAT_OUVERTURE;
    } else if (distance > maxLevel && etatActuel != ETAT_FERME) {
      etatActuel = ETAT_FERMETURE;
    }
  }
}
void click() {
  clickState = true;
}
void doubleClick() {
  doubleClickState = true;
}
void displayInMonitor(unsigned long ct) {
  int rate = 3000;
  static unsigned long lastTime = 0;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    Serial.print("Lum:");
    Serial.print(luminosity);
    Serial.print(",Min:");
    Serial.print(minVal);
    Serial.print(",Max:");
    Serial.print(maxVal);
    Serial.print(",Dist:");
    Serial.print(distance);
    Serial.print(",T:");
    Serial.print(temp);
    Serial.print(",H:");
    Serial.print(hum);
    Serial.print("Van:");
    Serial.println(distancePercent);
  }
}
void displayLCD(String textLigneOne, String textLigneTwo, int valueLigneOne, String valueLigneTwo, String unitOne = "", String unitTwo = "", int decimal = 0) {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(textLigneOne);
  lcd.print(valueLigneOne);
  lcd.print(unitOne);
  lcd.setCursor(0, 1);
  lcd.print(textLigneTwo);
  lcd.print(valueLigneTwo);
  lcd.print(unitTwo);
}
void calibration(int light) {
  if (light < minVal) {
    minVal = light;
  }
  if (light > maxVal) {
    maxVal = light;
  }
}
void etatOuverture(unsigned long ct, int raceDistance) {
  blinkingTask(ct);
  static unsigned long lastTime = 0;
  static bool firstTime = true;
  const int rate = 250;
  if (firstTime) {
    firstTime = false;
    myStepper.moveTo(raceDistance);
  }
  myStepper.run();
  if (myStepper.distanceToGo() == 0) {
    if (raceDistance == motorOpening) {
      etatActuel = ETAT_OUVERT;
      firstTime = true;
    }
  }
  if (ct - lastTime >= rate) {
    lastTime = ct;
    displayLCD("Vanne: ", "Etat: ", distancePercent, "Ouverture", " %");
  }
  if (clickState) {
    clickState = false;
    etatActuel = ETAT_ARRET;
    firstTime = true;
  }
}

void etatFerme(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 250;
  digitalWrite(LED_PIN, LOW);
  if (ct - lastTime >= rate) {
    lastTime = ct;
    displayLCD("Vanne: ", "Etat: ", 0, "Ferme", " %");
  }
  if (clickState) {
    clickState = false;
    etatLcd = ETAT_TEMPERATURE;
    return;
  }
  distanceForMotorState(ct);
}
void etatOuvert(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 250;
  digitalWrite(LED_PIN, LOW);
  if (ct - lastTime >= rate) {
    lastTime = ct;
    displayLCD("Vanne: ", "Etat: ", 100, "Ouvert", " %");
  }
  if (clickState) {
    clickState = false;
    etatLcd = ETAT_TEMPERATURE;
    return;
  }
  distanceForMotorState(ct);
}
void etatArret(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 250;
  digitalWrite(LED_PIN, LOW);
  if (ct - lastTime >= rate) {
    lastTime = ct;
    displayLCD("Vanne: ", "Etat: ", distancePercent, "Arret", " %");
  }
  if (clickState) {
    clickState = false;
    etatActuel = ETAT_OUVERTURE;
  }
}
void blinkingTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 50;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    ledStatePin = !ledStatePin;
    digitalWrite(LED_PIN, ledStatePin);
  }
}

void etatFermeture(unsigned long ct, int raceDistance) {
  blinkingTask(ct);
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    myStepper.moveTo(raceDistance);
  }
  myStepper.run();
  if (myStepper.distanceToGo() == 0) {
    if (raceDistance == motorClosing) {
      etatActuel = ETAT_FERME;
      firstTime = true;
    }
  }
  static unsigned long lastTime = 0;
  const int rate = 250;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    displayLCD("Vanne: ", "Etat: ", distancePercent, "Fermeture", "%");
  }
  if (clickState) {
    clickState = false;
    etatActuel = ETAT_ARRET;
    firstTime = true;
  }
}
void etatTemperature(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 250;
  if (ct - lastTime >= rate) {
    lastTime = ct;
    readDHTValue(ct);
    displayLCD("Temp : ", "Hum : ", temp, String(hum), " C", " %");
  }
  if (clickState) {
    clickState = false;
    etatLcd = ETAT_DISTANCE;
  }
  if (doubleClickState) {
    doubleClickState = false;
    etatLcd = ETAT_CALIBRATION;
  }
  transitionValveState(ct);
}
void etatDistance(unsigned long ct) {
  static unsigned long lastTime = 0;
  const int rate = 250;
  static bool firstTime = true;
  if (firstTime) {
    digitalWrite(LED_PIN, LOW);
  }
  if (ct - lastTime >= rate) {
    lastTime = ct;
    readDistance(ct);
    displayLCD("Lum: ", "Dist: ", luminosity, String(distance), " %", " cm");
  }
  if (clickState) {
    clickState = false;
    etatLcd = ETAT_VANNE;
    firstTime = true;
  }
  if (doubleClickState) {
    doubleClickState = false;
    etatLcd = ETAT_CALIBRATION;
    firstTime = true;
  }
  transitionValveState(ct);
}
void etatCalibration(unsigned long ct) {
  static unsigned long lastTime = 0;
  static bool firstTime = true;
  const int rate = 250;
  if (firstTime) {
    firstTime = false;
    minVal = 1023;
    maxVal = 0;
  }
  if (ct - lastTime >= rate) {
    lastTime = ct;
    calibration(readResistance);
    displayLCD("Lum min: ", "Lum max: ", minVal, String(maxVal));
  }
  if (clickState) {
    clickState = false;
    etatLcd = ETAT_TEMPERATURE;
    firstTime = true;
  }
  transitionValveState(ct);
}
void ledState(bool ledState) {
  if (etatActuel != ETAT_OUVERTURE && etatActuel != ETAT_FERMETURE) {
    if (ledState) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }
}
void transitionValveState(unsigned long ct) {
  static unsigned long lastTime = 0;
  int rate = 200;
  if (ct - lastTime >= rate) {
    if (etatLcd != ETAT_VANNE) {
      if (distance < minLevel && etatActuel != ETAT_OUVERT) {
        etatLcd = ETAT_VANNE;
        etatActuel = ETAT_OUVERTURE;
      } else if (distance > maxLevel && etatActuel != ETAT_FERME) {
        etatLcd = ETAT_VANNE;
        etatActuel = ETAT_FERMETURE;
      }
    }
  }
}
