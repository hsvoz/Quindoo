#include <SPI.h>
#include <SD.h>
#include <SimpleDHT.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// constants -----------------------------------

const byte pinDHT11 = 2;
const byte chipSelect = 10;
const int _measurementPeriod = 10000; //perioda mereni v ms
const TimeSpan _measurementLength(0, 10, 0, 0); //delka mereni v dny, hodiny, minuty, sekundy
const byte co2Zero = 55;
const byte _buttonPin = 3;

const int _screenWidth = 128;
const byte _screenHeight = 8;
const byte _oledReset = 4;
const byte _screenAddress = 0x3C;

// ---------------------------------------------


// global variables ----------------------------

RTC_DS1307 _RTC;

TimeSpan _measurementDuration;
TimeSpan _readyDuration;
DateTime _enterReady;
DateTime _startTime;

bool _RUN = true;
bool _buttonPressed = false;

enum : byte
{
  STARTUP,
  ENTER_READY,
  READY,
  START,
  RUNNING,
  INTERRUPTED,
} _statusOfMeasurement;

char _buff[10 + 1];

SimpleDHT11 dht11(pinDHT11);

Adafruit_SSD1306 _display(_screenWidth, _screenHeight, &Wire, _oledReset);

char _dataFileName[34];

// ---------------------------------------------


void setup() {

  delay(1000);

  _RTC.begin();
  delay(500);

  delay(500);

  _statusOfMeasurement = ENTER_READY;

  pinMode(_buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(_buttonPin), buttonPressed, HIGH);
  
  _buff[sizeof(_buff)-1] = '\0';

  if (!SD.begin(chipSelect))
  {
    return;
  }
  delay(500);

  if (!solveDataFileName())
  {
    // fallback kdyz nemuzeme vytvorit spravne jmeno datoveho souboru
    _dataFileName[0] = '\0';
    strncat(_dataFileName, "datalog.txt", sizeof(_dataFileName) - 1);
  }

  File dataFile = SD.open(_dataFileName, FILE_WRITE);
  if(dataFile)
  {
    char header[48];
    header[0] = '\0';
    strncat(header, "cas,teplota[°C],vlhkost[%],CO2[ppm],CO[-]", sizeof(header) - 1);
    
    dataFile.println(header);
    dataFile.close();
  }

  _display.begin(SSD1306_SWITCHCAPVCC, _screenAddress);
  _display.display();
  _display.setTextSize(1);
  _display.setTextColor(WHITE);
  _display.clearDisplay();

  _startTime = _RTC.now();
}


void loop() {

  if (!_RUN)
  {
    delay(2000);
    return;
  }

  if (_statusOfMeasurement == ENTER_READY)
  {
    _enterReady = _RTC.now();
    _statusOfMeasurement = READY;
    return;
  }
  else if (_statusOfMeasurement == READY) // mereni pripraveno
  {
    if (_buttonPressed)
    {
      delay(100);
      _readyDuration = _readyDuration + (_RTC.now() - _enterReady);
      _buttonPressed = false;
      _statusOfMeasurement = START;
      return;
    }
    else
    {
      _buff[0] = '\0';
      strncat(_buff, "ready", sizeof(_buff) - 1);
      displayMsg();
      delay(2000);
      return;
    }
  }
  else if (_statusOfMeasurement == START) // start mereni
  {
    _statusOfMeasurement = RUNNING;
    return;
  }
  else if (_statusOfMeasurement == RUNNING) // mereni probiha
  {
    if (_buttonPressed)
    {
      delay(100);
      _buttonPressed = false;
      _statusOfMeasurement = INTERRUPTED;
      return;
    }
    else
    {
      // nothing here
    }
  }
  else if (_statusOfMeasurement == INTERRUPTED) // mereni preruseno
  {
    _statusOfMeasurement = ENTER_READY;
    return;
  }

  _measurementDuration = _RTC.now() - _startTime;

  if (_measurementDuration.totalseconds() - _readyDuration.totalseconds() < _measurementLength.totalseconds())
  {
    byte temperature = 0;
    byte humidity = 0;
    int co2ppm = 0;
    int coValue = 0;
    
    measureDht11(&temperature, &humidity);
    measureCo2Co(co2ppm, coValue);   

    storeMeasurement(temperature, humidity, co2ppm, coValue);

    for(byte i = 0; i <= 5 ; ++i)
    {
      if(_buttonPressed)
        break;
        
      _display.clearDisplay();
      delay(100);
      displayMeasurement(i, temperature, humidity, co2ppm, coValue >= 220 ? true : false);
      delay((_measurementPeriod - 2000 - 100) / 6);
    } 
  }
  else
  {
    _buff[0] = '\0';
    strncat(_buff, "KONEC", sizeof(_buff)-1);
    displayMsg();
    
    _RUN = false;
  }
}

void measureDht11(byte* temperature, byte* humidity)
{
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(temperature, humidity, NULL)) != SimpleDHTErrSuccess)
  {
  }
}

void measureCo2Co(int& co2ppm, int& coValue)
{
  int co2now[10];
  int co2raw = 0;
  int co2comp = 0;

  for (int x = 0; x < 10; ++x)                  //mereni 10x behem 2s
  {
    co2now[x] = analogRead(A0);
    delay(200);
  }

  //vypocet prumeru
  int sum = 0;                                  
  for(int x = 0; x < 10; ++x)
  {
    sum += co2now[x];  
  }
  co2raw = sum/10;
  
  co2comp = co2raw - co2Zero;
  co2ppm = map(co2comp, 0, 1023, 400, 5000);

  
  coValue = analogRead(A1);
}

void storeMeasurement(byte temperature, byte humidity, int co2ppm, int coValue)
{
  File dataFile = SD.open(_dataFileName, FILE_WRITE);
  if(dataFile)
  {
    DateTime now = _RTC.now();
    snprintf(_buff, sizeof(_buff), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    dataFile.print(_buff);
    dataFile.print(",");
    dataFile.print(temperature, DEC);
    dataFile.print(",");
    dataFile.print(humidity, DEC);
    dataFile.print(",");
    dataFile.print(co2ppm, DEC);
    dataFile.print(",");
    dataFile.print(coValue);
    //dataFile.print(",");
    //dataFile.print(hsFreeMemory(), DEC); 
    dataFile.println("");
    
    dataFile.close();
  }
}

void displayMeasurement(byte what, byte temperature, byte humidity, int co2ppm, bool coPresence)
{
  char airQuality[16];
  airQuality[0] = '\0';
   
  if (co2ppm > 2000)
  {
    strncat(airQuality, "SMRT", sizeof(airQuality) - 1);
  }
  else if (co2ppm > 1000 && co2ppm < 2000)
  {
    strncat(airQuality, "vyvetrat!", sizeof(airQuality) - 1);
  }
  else if (co2ppm > 500 && co2ppm < 1000)
  {
    strncat(airQuality, "prijatelna", sizeof(airQuality) - 1);
  }
  else if (co2ppm > 350 && co2ppm < 500)
  {
    strncat(airQuality, "vyborna", sizeof(airQuality) - 1);
  }

  if (what == 0)
  {
    char doba[16];
    TimeSpan dur = _measurementDuration - _readyDuration;
    doba[sizeof(doba) - 1] = '\0';
    snprintf(doba, sizeof(doba) - 1, "%02d:%02d:%02d", dur.hours(), dur.minutes(), dur.seconds());
    _display.setCursor(10,0);
    _display.println("Doba:");
    _display.setCursor(65,0);
    _display.println(doba);
  }
  else if (what == 1)
  {
    _display.setCursor(10,0);
    _display.println("Teplota:"); 
    _display.setCursor(65,0);
    _display.println(temperature, DEC);
    _display.println(" C");
  }  
  else if (what == 2)
  {
    _display.setCursor(10,0);
    _display.println("Vlhkost:"); 
    _display.setCursor(65,0);
    _display.print(humidity, DEC);
    _display.println(" %");
  }
  else if (what == 3)
  {
    _display.setCursor(10,0);
    _display.println("CO2:");
    _display.setCursor(65,0);
    _display.print(co2ppm, DEC);
    _display.println(" PPM");
  }
  else if (what == 4)
  {
    _display.setCursor(10,0);
    _display.println("CO:");
    _display.setCursor(65,0);
    _display.println(coPresence ? "Nebezpeci" : "OK");
  }
  else if (what == 5)
  {
    _display.setCursor(10,0);
    _display.println("Kvalita:");
    _display.setCursor(65,0);
    _display.println(airQuality);
  }

  /*else if (what == 6)
  {
    _display.setCursor(10,0);
    _display.println("Memory:");
    _display.setCursor(65,0);
    _display.print(hsFreeMemory(), DEC);
    _display.println(" B");
  }*/

  _display.display();
}

void displayMsg()
{
  _display.clearDisplay();
  _display.setCursor(50,0);
  _display.println(_buff);
  _display.display();
}

void buttonPressed()
{
  _buttonPressed = true;
}

bool solveDataFileName()
{
  // vytvori jmeno pro novy datalog file a zajisti, ze adresare existuji
  // cesta k souboru je tvorena datumem a casem:
  // hs/{yyyymmdd}/{hhmmss}/datalog.txt

  DateTime curTime = _RTC.now();

  snprintf(_dataFileName, sizeof(_dataFileName), "hs/%04d%02d%02d/%02d%02d%02d", 
    curTime.year(), curTime.month(), curTime.day(),
    curTime.hour(), curTime.minute(), curTime.second());
  if (!SD.exists(_dataFileName))
  {
    if (!SD.mkdir(_dataFileName))
    {
      return false;
    }
  }

  strncat(_dataFileName, "/datalog.txt", sizeof(_dataFileName) - strlen(_dataFileName));
  if (SD.exists(_dataFileName))
  {
    SD.remove(_dataFileName); // v jedne sekunde muze zacit pouze jedno mereni => vzdy novy datalog file
  }

  return true;  
}
