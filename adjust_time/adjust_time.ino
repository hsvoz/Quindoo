#include <RTClib.h>


int _RUN = true;
RTC_DS1307 _RTC;
 
void setup()
{
    Serial.begin(9600);
    delay(1000);
    
    _RTC.begin();
    delay(1000);
    
    if (! _RTC.isrunning()) 
    {
      Serial.println("Hodiny nenalezeny");
      Serial.flush();
    }
}
 
void loop()
{
    if (!_RUN)
    {
        delay(1000);
        return;
    }

    DateTime settime(2022, 2, 3, 18, 01, 00);  //cas ve formatu rok, mesic, den, hodiny, minuty, sekundy
    _RTC.adjust(settime);
    Serial.println("Cas nastaven na " +
    String(settime.day(),DEC) + "." + String(settime.month(),DEC) + "."+ String(settime.year(),DEC)
    + " " 
    + String(settime.hour(),DEC) + ":"+ String(settime.minute(),DEC) + ":"+ String(settime.second(),DEC));
    
    _RUN = false;
}
