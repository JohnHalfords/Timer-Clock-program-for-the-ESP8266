//                                             - - o o - -                                       //
//                                           18 November 2021                                    //
//// //// ////     ESP8266 Project Timerclock by John Halfords, The Netherlands      //// //// ////
//// //// ////        Comments and questions welcome at: halfordsj@gmail.com         //// //// ////
//                                                                                               //
//                                       Please read Readme.md                                   //
//                                             - - o o - -                                       //

// What can you expect in 3.2 or higher:
// Offset value for Sunset and Sunrise both + and -
// Requests are welcome!

// What is new in 3.1:
// Settings page that holds the most important Settings for the user myTimeZone, myLatitude, myLongitude, TimesGPIO, GPIOdelay, GPIOhightime, HistoryPage (on/off)
// Added a 'Settings to defaults' with a "Are you sure" page
// Added the function that at first run the data that is stored in EEprom has default values, before it was whatever was on that memory address
// Deleted the Reset button (Restart is better, it's cold, it restarts the SDK)
// Extended the Restart function with a "Are you sure" page
// Added the Latitude and Longitude values to the infopage with a link to check the location on https://www.latlong.net
// Some cosmetics and small bugs nobody noticed ;-)

// What was new in 3.0:
// - Every timer statuschange (ON or OFF) can also fire a http-command, you can use it for example for webhooks or Tasmota
// - Few cosmetics
// - Removed the controls Timer A - D (All ON of All OFF), never used it anyway...
// - Bug fixed in coloring red the timerfields when invalid value was entered

// What was new in 2.2:
// - IsDST not used in Dusk2Dawn, info see "void ChangeSunTimes(bool DoIt)"
// - Resized iFrame for new ESP8266HTTPUpdateServer.h (now able to update FS as well)
// - Last sync time added on info page
// - Restyling info page

// What was new in 2.1:
// - Offset from Sunrise / Sunset
// - Reorganised code for html Timerspage
// - History page

// Includes of Libraries
#include <ESP8266WiFiMulti.h>          // Load library to connect to WiFi and automaticaly choose the strongest network
#include <ESP8266WebServer.h>          // Load library to run the WebServer
#include <ESP8266HTTPUpdateServer.h>   // Load library for updating sketch through http
#include <EEPROM.h>                    // Load library to write to, and read from EEPROM
#include <ezTime.h>                    // Load Very well documented Time libary with everything you possibly want! (https://github.com/ropg/ezTime)
#include <Dusk2Dawn.h>                 // Load Library to define Sunset and Sunrise (https://github.com/dmkishi/Dusk2Dawn)
#include <ESP8266HTTPClient.h>         // Load Library to run as a HTTP cliënt (to send the http-commands)

// Create instances
ESP8266WiFiMulti wifiMulti;            // Create an instance for connecting to wifi
ESP8266WebServer WebServer(80);        // Create an instance for the webserver
ESP8266HTTPUpdateServer WebUpdater;    // Create an instance for the http update server (to upload a new sketch via the website)

// User declarations #########################################################################################
#include "UserDeclarations.h"          // Wifi settings, DebugLevels and pinning can be changed to your design

// Program declarations // DON'T change! Change file "UserDeclarations.h" if your design is different from mine !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
String version$ = "v3.1";                                     // Version of program
String webpageheader, webpagefooter, webpage;                 // Strings for building the webpages
String syncstatus;                                            // String that holds the status of the timesync
unsigned int mySunrise, mySunset;                             // Integer that holds the numerical value of todays Sunrise and Sunset (in minutes from midnight)
String mySunrise$, mySunset$;                                 // String that holds the text value of todays Sunrise and Sunset HH:MM
String SunSet[4], SunRise[4];                                 // String array, element is "checked" when Sunrise/set time is active,  "" (unchecked) when user timer time is set
int SunBits;                                                  // 8 bits that hold the settings of the checkboxes SunSet and SunRise
unsigned int HourTimerOn[4], HourTimerOff[4];                 // Variable arrays for Timers 1-4
unsigned int MinuteTimerOn[4], MinuteTimerOff[4];             // Variable arrays for Timers 1-4
unsigned int MinuteTimerOnOffset[4], MinuteTimerOffOffset[4]; // Variable arrays for Timers 1-4 Offset in case of using Sunset or Sunrise

String HTTPcommandOn[4], HTTPcommandOff[4];                   // Strings that hold the HTTP commands for each timer

String HistoryMsg[20];                                        // String array for the last 20 messages for the History page
String FieldClassOn[4], FieldClassOff[4];                     // String for the html-class (yellow, red, grey or green color) for fields on the Timerpage
String FieldClassOnOffset[4], FieldClassOffOffset[4];         // String for the html-class (yellow, grey or green color) for Offset fields on the Timerpage
String FieldClassSetting[8];                                  // String for the html-class (yellow, red, grey or green color) for fields on the Settingspage
String TempHourTimerOn, TempMinuteTimerOn, TempHourTimerOff;                  // Make temporary strings for functionality in HTML forms
String TempMinuteTimerOff, TempMinuteTimerOnOffset, TempMinuteTimerOffOffset; // Make temporary strings for functionality in HTML forms
String TempTimerOnReadOnly, TempTimerOffReadOnly;                             // Make temporary strings for functionality in HTML forms
String RanBefore;            // If "yEAh" is not written in EEPROM yet, we asume this is the first run and fill settings with Default values
// Program declarations // DON'T change! Change file "UserDeclarations.h" if your design is different from mine !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// Declarations of variables that can be set on the settingspage
String myTimeZone;           // Timezone (2 characters, countrycodes; https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)
float myLatitude;            // Latitude (Value from -180 to 180 with max 2 decimals, must be in decimal degrees (DD), e.g. 52.00, used to determine SunSet and SunRise)
float myLongitude;           // Longitude(Value from -90 to 90   with max 2 decimals, must be in decimal degrees (DD), e.g. 5.00, used to determine SunSet and SunRise)
//                           // Tip: Click your location in Google Maps, wait, click again (no double-click) to see your values
unsigned int TimesGPIO;      // How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
//                           // *(my remote is "fire-and-forget" so i set this value to 3). Value minimum = 1, max = 99
unsigned int GPIOdelay;      // Time in 10 milliseconds to wait for sending the next HIGH-LOW to the same GPIO. Value minimum = 1, max = 99 (Example: 50 = 500 ms)
unsigned int GPIOhightime;   // Time in 10 milliseconds to wait between sending the HIGH and LOW to the GPIO, so actually 'how long is the GPIO HIGH'. Value minimum = 1, max = 99 (Example: 50 = 500 ms)
int HistoryPage;             // Debug messages to Html-page 'History' (format on Historypage is D-M-YYYY HH:MM:SS (local time) + message)
//                           // Integer is 1 when HistoryPage is active,  0 (unchecked) when HistoryPage is not active

Timezone TimerTimeZone;      // Create an instance for ezTime, Timer's timezone

////// Setup
void setup()
{
  Serial.begin(myBaudrate);                      // Start the Serial communication
  if (DebugLevel >= 1) Serial.println("\nStarting Setup...");

  // Declaring output pins
  pinMode(wifiindicator, OUTPUT); //  .         // the wifiindicator blinks during connecting and is ON while connected
  pinMode(timer_a_on,    OUTPUT); //   .
  pinMode(timer_a_off,   OUTPUT); //    .
  pinMode(timer_b_on,    OUTPUT); //     .
  pinMode(timer_b_off,   OUTPUT); //      . <- If your design is different from mine: Change User declarations! NOT these!
  pinMode(timer_c_on,    OUTPUT); //     .
  pinMode(timer_c_off,   OUTPUT); //    .
  pinMode(timer_d_on,    OUTPUT); //   .
  pinMode(timer_d_off,   OUTPUT); //  .

  WiFi.mode(WIFI_STA);                           // Put the ESP8266 in station mode only, AP with: WiFi.mode(WIFI_AP_STA)
  if (DebugLevel >= 1) Serial.println("\nSwitch off WiFi Acces Point");

  StartWiFi();                                  // Subroutine that starts WiFi connection to the stongest SSID given

  // Handle the Pages
  WebServer.on("/", wwwroot);                   // is also the 'info-page'
  WebServer.on("/controls", controlspage);      // page to directly control the timers
  WebServer.on("/settings", settingspage);      // page for settings, reset/restart and update sketch via the website (through http)
  WebServer.on("/timers", timerspage);          // page to set the timer times and check SunSet/Rise
  WebServer.on("/httpcmdpage", httpcmdpage);    // page to set the http commands
  WebServer.on("/history", history);            // page for displaying the last 20 messages
  WebServer.on("/restartpage", restartpage);    // Webpage for Restart Yes/No
  WebServer.on("/defaultspage", defaultspage);  // Webpage for Defauls and Restart Yes/No
  WebServer.onNotFound(PageNotFound);           // when a page is requested that doesn't exist

  WebServer.begin();                            // Start the webserver
  WebUpdater.setup(&WebServer);                 // Start the WebUpdater as part of the WebServer

  ReadFromEEprom();                             // Subroutine that reads timer on and off settings, http-commands and settings from EEPROM
  if (RanBefore != "yEAh") FirstRun();          // If "yEAh" is NOT written in EEPROM yet, we asume this is the 'first run' and fill settings with Default values
  MakeHeaderFooter();                           // Make standard header, Title, Menu and footer used for every webpage

  TimerTimeZone.setLocation(myTimeZone);        // Set timezone for ezTime to the setting you gave on the settingspage
  waitForSync();                                // Wait for time to sync (ezTime)
  ChangeSunTimes(1);                            // Subroutine that changes the Sunrise and Sunset. Argument = 0 every night at 00:00:30. Argument = 1 just do it

  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("TimerClock " + version$ + " Ready!");
  if (HistoryPage == 1) MakeHistory("TimerClock " + version$ + " Ready!");
}

// Main loop
void loop()
{
  WebServer.handleClient();         // Listen for HTTP requests from clients
  ChangeSunTimes(0);                // Subroutine that changes the Sunrise and Sunset. Argument = 0 every night at 00:00:30. Argument = 1 just do it
  CheckTimers();                    // Subroutine that checks if timers match the current time
  events();                         // ezTime, give opportunity to sync time (every 30 minutes)
}
///////////////////  End of Program  //////////////////////


// // // // // // // // // // // // // // // // // // // //


///////////////////  Subroutines  /////////////////////////

// Subroutine that changes the Sunrise and Sunset, argument 0 means every night at 00:00:30, argument 1 means Just Do It! (for startup and after settingschange)
void ChangeSunTimes(bool DoIt)
{
  if ((TimerTimeZone.hour() == 0 && TimerTimeZone.minute() == 0 && TimerTimeZone.second() == 30) || (DoIt == 1)) // Do this every nigth at 00:00:30 (DoIt = 0) or at Startup (DoIt = 1)
  {
    Dusk2Dawn mySunTimes (myLatitude, myLongitude, (-TimerTimeZone.getOffset() / 60));                   // Create instance for Sunrise and Sunset
    //                                                                                                   // Syntax: Dusk2Dawn <location instance name> (Latitude, Longitude, Time offset in minutes to UTC (minus!))

    mySunrise = mySunTimes.sunrise(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), 0); // Store todays Sunrise in Variable mySunrise
    mySunset = mySunTimes.sunset(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), 0);   // Store todays Sunset in Variable mySunset
    //                                                                                                   // Syntax: <instance name>.sunrise(int year, int month, int day, bool <is DST in effect?>)

    // According to the syntax of Dusk2Dawn the 2 lines below should be rigth, however Sunrise and Sunset where +1 in DST time (Bug in Dusk2Dawn?)...
    // mySunrise = mySunTimes.sunrise(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), TimerTimeZone.isDST()); // Store todays Sunrise in Variable mySunrise
    // mySunset = mySunTimes.sunset(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), TimerTimeZone.isDST());   // Store todays Sunset in Variable mySunset

    // Make string for Sunrise and Sunset, for Debuglines and wwwroot page (Info)
    String tempHour$, tempMinute$;
    tempHour$   = String(hour(mySunrise * 60));
    tempMinute$ = String(minute(mySunrise * 60));
    if (hour(mySunrise * 60)   < 10) tempHour$   = "0" + tempHour$;         // Add leading 0 in String
    if (minute(mySunrise * 60) < 10) tempMinute$ = "0" + tempMinute$;       // Add leading 0 in String
    mySunrise$ = tempHour$ + ":" + tempMinute$;                             // Make Sunrise String

    tempHour$   = String(hour(mySunset * 60));
    tempMinute$ = String(minute(mySunset * 60));
    if (hour(mySunset * 60)   < 10) tempHour$   = "0" + tempHour$;          // Add leading 0 in String
    if (minute(mySunset * 60) < 10) tempMinute$ = "0" + tempMinute$;        // Add leading 0 in String
    mySunset$ = tempHour$ + ":" + tempMinute$;                              // Make Sunrise String

    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.println("Set Sunrise and Sunset for today. Sunrise = " + mySunrise$ + " and Sunset = " + mySunset$);
    if (HistoryPage == 1) MakeHistory("Set Sunrise/set for today. " + mySunrise$ + " and " + mySunset$);

    ChangeTimers2Sun();            // Subroutine that changes the timersettings for the timers that have the checkbox Sunrise and/or Sunset
    if (DoIt == 0) delay(2000);    // To prevent that this happens more than once this second, only in case it is triggered bij time = 00:00:30
  }
}


// Subroutine that changes the timersettings for the timers that have the checkbox Sunrise and/or Sunset
void ChangeTimers2Sun()
{
  for (int count = 0; count <= 3; count++)             // Do this 4 times, once for every timer
  {
    if (SunSet[count] == "checked")                    // if the Sunset checkbox is checked you want to change the setting to the Sunset time
    {
      HourTimerOn[count]   = hour(mySunset * 60);      // mySunset is minutes after 0:00 and the hour() function expects seconds, so * 60
      MinuteTimerOn[count] = minute(mySunset * 60);
    }
    else MinuteTimerOnOffset[count]   = 0;             // When the checkbox for Sunset is unchecked, the offset value needs to become 0

    if (SunRise[count] == "checked")                   // if the Sunrise checkbox is checked you want to change the setting to the Sunrise time
    {
      HourTimerOff[count]   = hour(mySunrise * 60);    // mySunrise is minutes after 0:00 and the hour() function expects seconds, so * 60
      MinuteTimerOff[count] = minute(mySunrise * 60);
    }
    else MinuteTimerOffOffset[count]  = 0;             // When the checkbox for Sunrise is unchecked, the offset value needs to become 0
  }
}


// Subroutine that checks if current time mathes one of the timers
void CheckTimers()
{
  // We store the current time in temporary variables now we have all the time to do things with these temporary variables without moving one second in time...
  int HourTemp = TimerTimeZone.hour();
  int MinuteTemp = TimerTimeZone.minute();
  int SecondTemp = TimerTimeZone.second();

  for (int count = 0; count <= 3; count++)  // Do this 4 times, once for every timer
  {
    if (not (HourTemp == 0 && MinuteTemp == 0)) // At 00:00 the timer is assumed to be "not set", so the rest of this Subroutine is not necessary
    {
      // We store the Timer time in temporary variables so we can add and subtract the offset without changing the original values
      int HourTimerOnTemp    = HourTimerOn[count];
      int MinuteTimerOnTemp  = MinuteTimerOn[count];
      int HourTimerOffTemp   = HourTimerOff[count];
      int MinuteTimerOffTemp = MinuteTimerOff[count];

      // Timer ON: Subtract offset from TimerMinutes
      if (MinuteTimerOnOffset[count] > 0)
      {
        // Calculate the Timer ON time to minutes
        int TimerOn_IN_MINUTES = 60 * HourTimerOnTemp;                         // Number of hours times 60
        TimerOn_IN_MINUTES = TimerOn_IN_MINUTES + MinuteTimerOnTemp;           // Adding the minutes

        TimerOn_IN_MINUTES = TimerOn_IN_MINUTES - MinuteTimerOnOffset[count];  // Subtract the offset

        // Calculate back to hours and minutes
        HourTimerOnTemp = hour(TimerOn_IN_MINUTES * 60);                       // Hours back to temporary variable hour
        MinuteTimerOnTemp = minute(TimerOn_IN_MINUTES * 60);                   // Hours back to temporary variable hour
      }

      // Timer OFF: Add offset to TimerMinutes
      if (MinuteTimerOffOffset[count] > 0)
      {
        // Calculate the Timer OFF time to minutes
        int TimerOff_IN_MINUTES = 60 * HourTimerOffTemp;                         // Number of hours times 60
        TimerOff_IN_MINUTES = TimerOff_IN_MINUTES + MinuteTimerOffTemp;          // Adding the minutes

        TimerOff_IN_MINUTES = TimerOff_IN_MINUTES + MinuteTimerOffOffset[count]; // Add the offset

        // Calculate back to hours and minutes
        HourTimerOffTemp = hour(TimerOff_IN_MINUTES * 60);                       // Hours back to temporary variable hour
        MinuteTimerOffTemp = minute(TimerOff_IN_MINUTES * 60);                   // Hours back to temporary variable hour
      }

      // Timer ON
      if (HourTemp == HourTimerOnTemp && MinuteTemp == MinuteTimerOnTemp && SecondTemp == 1) // Let the ESP activate the corresponding GPIO for 'TIMER ON' on second 1
      {
        if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
        if (DebugLevel >= 1) Serial.print("Timer ");
        if (DebugLevel >= 1) Serial.print(String("ABCD").substring(count, count + 1));                      // Mention the Timername (A, B, C or D) rather then the number
        if (DebugLevel >= 1) Serial.println(" ON!");
        if (HistoryPage == 1) MakeHistory("Timer " + String("ABCD").substring(count, count + 1) + " ON!");  // Mention the Timername (A, B, C or D) rather then the number
        if (count == 0) TimerOnOff(timer_a_on, count);
        if (count == 1) TimerOnOff(timer_b_on, count);
        if (count == 2) TimerOnOff(timer_c_on, count);
        if (count == 3) TimerOnOff(timer_d_on, count);
        delay(1000);  // Don't let this happen again this second
      }

      // Timer OFF
      if (HourTemp == HourTimerOffTemp && MinuteTemp == MinuteTimerOffTemp && SecondTemp == 1) // Let the ESP activate the corresponding GPIO for 'TIMER OFF' on second 1
      {
        if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
        if (DebugLevel >= 1) Serial.print("Timer ");
        if (DebugLevel >= 1) Serial.print(String("ABCD").substring(count, count + 1));                       // Mention the Timername (A, B, C or D) rather then the number
        if (DebugLevel >= 1) Serial.println(" OFF!");
        if (HistoryPage == 1) MakeHistory("Timer " + String("ABCD").substring(count, count + 1) + " OFF!");  // Mention the Timername (A, B, C or D) rather then the number
        if (count == 0) TimerOnOff(timer_a_off, count + 10); // +10 to see the difference between ON and OFF in the TimerOnOff subroutine
        if (count == 1) TimerOnOff(timer_b_off, count + 10);
        if (count == 2) TimerOnOff(timer_c_off, count + 10);
        if (count == 3) TimerOnOff(timer_d_off, count + 10);
        delay(1000);  // Don't let this happen again this second
      }
    }
  }
}


// Subroutine that starts WiFi connection to the stongest SSID given
void StartWiFi()
{
  wifiMulti.addAP(WifiSSID1.c_str(), WifiPassw1.c_str());                       // adds Wi-Fi network 1 to the list
  if (WifiSSID2 != "") wifiMulti.addAP(WifiSSID2.c_str(), WifiPassw2.c_str());  // adds Wi-Fi network 2 to the list, if exist
  if (WifiSSID3 != "") wifiMulti.addAP(WifiSSID3.c_str(), WifiPassw3.c_str());  // adds Wi-Fi network 3 to the list, if exist
  if (WifiSSID4 != "") wifiMulti.addAP(WifiSSID4.c_str(), WifiPassw4.c_str());  // adds Wi-Fi network 4 to the list, if exist

  if (DebugLevel >= 1) Serial.print("Connecting to WiFi..");

  while (wifiMulti.run() != WL_CONNECTED) // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
  {
    // WiFi indicator blinking while connecting
    digitalWrite(wifiindicator, HIGH);
    delay(150);
    digitalWrite(wifiindicator, LOW);
    delay(150);
    if (DebugLevel >= 1) Serial.print(".");
  }
  digitalWrite(wifiindicator, HIGH); // WiFi indicator ON when wifi is connected
  if (DebugLevel >= 1) Serial.println(".*");
  if (DebugLevel >= 1) Serial.println("Wifi is ON ...");
  if (DebugLevel >= 1) Serial.print(" and connected to: ");
  if (DebugLevel >= 1) Serial.println(WiFi.SSID());               // Tell us what network we're connected to
  if (DebugLevel >= 1) Serial.print("with IP address : ");
  if (DebugLevel >= 1) Serial.println(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  if (DebugLevel >= 1) Serial.print(" and MAC address : ");
  if (DebugLevel >= 1) Serial.println(WiFi.macAddress());         // Send the MAC address of the ESP8266 to the computer
}


// Subroutine that reads timer on and off settings, http-commands and settings from EEPROM
void  ReadFromEEprom()
{
  EEPROM.begin(2093);
  // Read the timer on and off settings, http-commands and settings from EEPROM (Addresses commented at the end of this Subroutine)
  HourTimerOn[0]    = EEPROM.read(0);
  MinuteTimerOn[0]  = EEPROM.read(1);
  HourTimerOff[0]   = EEPROM.read(2);
  MinuteTimerOff[0] = EEPROM.read(3);
  HourTimerOn[1]    = EEPROM.read(4);
  MinuteTimerOn[1]  = EEPROM.read(5);
  HourTimerOff[1]   = EEPROM.read(6);
  MinuteTimerOff[1] = EEPROM.read(7);
  HourTimerOn[2]    = EEPROM.read(8);
  MinuteTimerOn[2]  = EEPROM.read(9);
  HourTimerOff[2]   = EEPROM.read(10);
  MinuteTimerOff[2] = EEPROM.read(11);
  HourTimerOn[3]    = EEPROM.read(12);
  MinuteTimerOn[3]  = EEPROM.read(13);
  HourTimerOff[3]   = EEPROM.read(14);
  MinuteTimerOff[3] = EEPROM.read(15);
  // Read the Sunrise and Sunsete checkboxes settings
  SunBits           = EEPROM.read(16);
  // Read the Offset in case of using Sunset or Sunrise
  MinuteTimerOnOffset[0]   = EEPROM.read(17);
  MinuteTimerOffOffset[0]  = EEPROM.read(18);
  MinuteTimerOnOffset[1]   = EEPROM.read(19);
  MinuteTimerOffOffset[1]  = EEPROM.read(20);
  MinuteTimerOnOffset[2]   = EEPROM.read(21);
  MinuteTimerOffOffset[2]  = EEPROM.read(22);
  MinuteTimerOnOffset[3]   = EEPROM.read(23);
  MinuteTimerOffOffset[3]  = EEPROM.read(24);

  // Read the HTTPcommands
  for (int count1 = 0; count1 < 4; count1++) // Do this 4 times, for every timer
  {
    // HTTPcommands ON
    for (int count = 0; count < 255; count++)  // Read 255 characters
    {
      HTTPcommandOn[count1] += char(EEPROM.read(count + 25 + (count1 * 510)));  // Read every character and add it to the variable
    }
    HTTPcommandOn[count1].trim();  // Delete leading spaces

    // HTTPcommands OFF
    for (int count = 0; count < 255; count++)  // Independed from the real length, just assume it to be 255
    {
      HTTPcommandOff[count1] += char(EEPROM.read(count + 280 + (count1 * 510)));  // Read every character and add it to the variable
    }
    HTTPcommandOff[count1].trim();  // Delete leading spaces

  }

  // Checking or Unchecking SunSet[0-3] and SunRise[0-3] from the value of SunBits
  for (int count = 0; count < 4; count++)
  {
    SunSet[count]  = "";
    if (bitRead(SunBits, (count * 2) ) == 1)
    {
      SunSet[count]  = "checked";
    }
    SunRise[count]  = "";
    if (bitRead(SunBits, (count * 2) + 1) == 1)
    {
      SunRise[count]  = "checked";
    }
  }

  // myLongitude (Example: 52.00)
  String myLongitude$ = "";                // Empty the string before adding characters to it
  for (int count = 0; count < 7; count++)  // String has max length 7 (max = -179.99)
  {
    myLongitude$ += char(EEPROM.read(count + 2066));  // Read every character and add it to the variable
  }
  myLongitude = myLongitude$.toFloat();

  // myLatitude  (Example: 5.00)
  String myLatitude$ = "";                 // Empty the string before adding characters to it
  for (int count = 0; count < 6; count++)  // String has max length 6 (max = -89.99)
  {
    myLatitude$ += char(EEPROM.read(count + 2073));  // Read every character and add it to the variable
  }
  myLatitude = myLatitude$.toFloat();

  // myTimeZone (Example: NL)
  myTimeZone = char(EEPROM.read(2079));             // Read every character and add it to the variable
  myTimeZone += char(EEPROM.read(2080));            // Read every character and add it to the variable

  // TimesGPIO (Value from 1 to 99)
  TimesGPIO = EEPROM.read(2081);

  // GPIOdelay (Value from 1 to 99)
  GPIOdelay = EEPROM.read(2082);

  // GPIOhightime (Value from 1 to 99)
  GPIOhightime = EEPROM.read(2083);

  // HistoryPage (checked/unchecked) (0 = NO Historypage, 1 = Historypage)
  HistoryPage = EEPROM.read(2084);

  // Read RanBefore adresses (If "yEAh" is not written in EEPROM yet, we asume this is the first run and fill settings with Default values)
  RanBefore =  char(EEPROM.read(2085));              // Read every character and add it to the variable
  RanBefore += char(EEPROM.read(2086));              // Read every character and add it to the variable
  RanBefore += char(EEPROM.read(2087));              // Read every character and add it to the variable
  RanBefore += char(EEPROM.read(2088));              // Read every character and add it to the variable

  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("Done reading settings from memory.");

  /*
    EEPROM Memory, Size 2089 (! 1 more then last address !)

    Addresses and their Variables

    Timers:                             HTTP-commands:
    -------                             --------------
    0  HourTimerOn[0]                    25  - 280 HTTPcommandOn[0]
    1  MinuteTimerOn[0]                  81  - 535 HTTPcommandOff[0]
    2  HourTimerOff[0]                   36  - 790 HTTPcommandOn[1]
    3  MinuteTimerOff[0]                 791 - 1045 HTTPcommandOff[1]
    4  HourTimerOn[1]                   1046 - 1300 HTTPcommandOn[2]
    5  MinuteTimerOn[1]                 1301 - 1555 HTTPcommandOff[2]
    6  HourTimerOff[1]                  1556 - 1810 HTTPcommandOn[3]
    7  MinuteTimerOff[1]                1811 - 2065 HTTPcommandOff[3]
    8  HourTimerOn[2]
    9  MinuteTimerOn[2]                 Configs:
    10 HourTimerOff[2]                  --------
    11 MinuteTimerOff[2]                2066 - 2072 myLongitude
    12 HourTimerOn[3]                   2073 - 2078 myLatitude
    13 MinuteTimerOn[3]                 2079 - 2080 myTimeZone
    14 HourTimerOff[3]                  2081 TimesGPIO
    15 MinuteTimerOff[3]                2082 GPIOdelay
    16 Sunbits                          2083 GPIOhightime
    17 MinuteTimerOnOffset[0]           2084 HistoryPage
    18 MinuteTimerOffOffset[0]
    19 MinuteTimerOnOffset[1]           Various:
    20 MinuteTimerOffOffset[1]          --------
    21 MinuteTimerOnOffset[2]           2085 - 2088 RanBefore
    22 MinuteTimerOffOffset[2]
    23 MinuteTimerOnOffset[3]
    24 MinuteTimerOffOffset[3]
  */


}


// Subroutine that saves Timer values, http-commands and settings to EEPROM (Addresses commented at the end of the READ Subroutine)
void  SaveToEEprom()
{
  // First converting the (Un)checked SunSet[0-3] and SunRise[0-3] to the value of SunBits
  SunBits = 0;
  for (int count = 0; count < 4; count++)
  {
    if (SunSet[count]  == "checked")
    {
      bitSet(SunBits, (count * 2) );
    }

    if (SunRise[count] == "checked")
    {
      bitSet(SunBits, (count * 2) + 1 );
    }
  }

  ChangeTimers2Sun();    // Subroutine that changes the timersettings for the timers that have the checkbox Sunrise and/or Sunset

  // The actual saving
  EEPROM.begin(2093);
  // Save the timer on and off settings
  EEPROM.write(0, HourTimerOn[0]);
  EEPROM.write(1, MinuteTimerOn[0]);
  EEPROM.write(2, HourTimerOff[0]);
  EEPROM.write(3, MinuteTimerOff[0]);
  EEPROM.write(4, HourTimerOn[1]);
  EEPROM.write(5, MinuteTimerOn[1]);
  EEPROM.write(6, HourTimerOff[1]);
  EEPROM.write(7, MinuteTimerOff[1]);
  EEPROM.write(8, HourTimerOn[2]);
  EEPROM.write(9, MinuteTimerOn[2]);
  EEPROM.write(10, HourTimerOff[2]);
  EEPROM.write(11, MinuteTimerOff[2]);
  EEPROM.write(12, HourTimerOn[3]);
  EEPROM.write(13, MinuteTimerOn[3]);
  EEPROM.write(14, HourTimerOff[3]);
  EEPROM.write(15, MinuteTimerOff[3]);
  // Save the Sunrise and Sunsete checkboxes settings
  EEPROM.write(16, SunBits);
  // Save the Offset in case of using Sunset or Sunrise
  EEPROM.write(17, MinuteTimerOnOffset[0]);
  EEPROM.write(18, MinuteTimerOffOffset[0]);
  EEPROM.write(19, MinuteTimerOnOffset[1]);
  EEPROM.write(20, MinuteTimerOffOffset[1]);
  EEPROM.write(21, MinuteTimerOnOffset[2]);
  EEPROM.write(22, MinuteTimerOffOffset[2]);
  EEPROM.write(23, MinuteTimerOnOffset[3]);
  EEPROM.write(24, MinuteTimerOffOffset[3]);

  // Save the HTTPcommands
  for (int count1 = 0; count1 < 4; count1++) // Do this 4 times, for every timer
  {
    // HTTPcommands ON
    for (int count = 0; count < 255; count++)  // Independed from the real length, just assume it to be 255
    {
      if (count  < HTTPcommandOn[count1].length()) EEPROM.write(count + 25 + (count1 * 510), HTTPcommandOn[count1].charAt(count));  // Write the character
      if (count >= HTTPcommandOn[count1].length()) EEPROM.write(count + 25 + (count1 * 510), ' ');                                  // Write a ' ' to occupy the space
    }

    // HTTPcommands OFF
    for (int count = 0; count < 255; count++)  // Independed from the real length, just assume it to be 255
    {
      if (count  < HTTPcommandOff[count1].length()) EEPROM.write(count + 280 + (count1 * 510), HTTPcommandOff[count1].charAt(count));  // Write the character
      if (count >= HTTPcommandOff[count1].length()) EEPROM.write(count + 280 + (count1 * 510), ' ');                                   // Write a ' ' to occupy the space
    }
  }

  // Save the settings

  // myLongitude (Example: "52.00")
  for (int count = 0; count < 7; count++)  // String has max length 7 (max = -179.99)
  {
    EEPROM.write(2066 + count, String(myLongitude).charAt(count));  // Write the character
  }

  // myLatitude (Example: "5.00")
  for (int count = 0; count < 6; count++)  // String has max length 6 (max = -89.99)
  {
    EEPROM.write(2073 + count, String(myLatitude).charAt(count));   // Write the character
  }

  // myTimeZone (Example "NL")
  for (int count = 0; count < 2; count++)  // String has always length 2
  {
    EEPROM.write(2079 + count, myTimeZone.charAt(count));  // Write the character
  }

  // TimesGPIO (Value from 1 to 99)
  EEPROM.write(2081, TimesGPIO);  // Write the value

  // GPIOdelay (Value from 1 to 99)
  EEPROM.write(2082, GPIOdelay);  // Write the value

  // GPIOhightime (Value from 1 to 99)
  EEPROM.write(2083, GPIOhightime);  // Write the value

  // HistoryPage (on/off) (0 = NO Historypage, 1 = Historypage)
  EEPROM.write(2084, HistoryPage);  // Write the value

  // Fill the RanBefore adresses with "yEAh", so we can check if this is the first run and have to fill settings with Default values
  RanBefore = "yEAh";
  for (int count = 0; count < 4; count++)             // Do this 4 times, once for every character
  {
    EEPROM.write(2085 + count, RanBefore.charAt(count));
  }

  EEPROM.end();

  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("Done saving setings to memory.");
  if (HistoryPage == 1) MakeHistory("Done saving setings to memory.");
}

//////////////////// Subroutines that write the pages ////////////////////


// Webpage to directly control the timers
void controlspage()
{
  webpage = webpageheader;
  // To produce server arguments i made hidden fields with a standard value and a visible submit button.
  // The fields are questioned in the if-loop further on.
  webpage += "<br><table style = 'width:275px'><tr><td><h3>Timer A</h3></td><td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_a_on'><input type = 'submit' value = 'ON'></form></td> ";
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_a_off'><input type = 'submit' value = 'OFF'></form> </td></tr> ";
  webpage += "<tr><td><h3>Timer B </h3> </td><td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_b_on'><input type = 'submit' value = 'ON'></form></td> ";
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_b_off'><input type = 'submit' value = 'OFF'></form></td></tr> ";
  webpage += "<tr><td><h3>Timer C </h3> </td><td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_c_on'><input type = 'submit' value = 'ON'></form></td> ";
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_c_off'><input type = 'submit' value = 'OFF'></form></td></tr> ";
  webpage += "<tr><td><h3>Timer D </h3> </td><td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_d_on'><input type = 'submit' value = 'ON'></form></td> ";
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_d_off'><input type = 'submit' value = 'OFF'></form></td></tr></table> ";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server

  // if loop for questioning the button pressed
  if (WebServer.args() > 0 )
  {
    // activate the output for timers
    if (WebServer.arg(0) == "timer_a_on") TimerOnOff(timer_a_on, 0);
    if (WebServer.arg(0) == "timer_a_off") TimerOnOff(timer_a_off, 10); // +10 to see the difference between ON and OFF in the TimerOnOff subroutine
    if (WebServer.arg(0) == "timer_b_on") TimerOnOff(timer_b_on, 1);
    if (WebServer.arg(0) == "timer_b_off")TimerOnOff(timer_b_off, 11);
    if (WebServer.arg(0) == "timer_c_on") TimerOnOff(timer_c_on, 2);
    if (WebServer.arg(0) == "timer_c_off") TimerOnOff(timer_c_off, 12);
    if (WebServer.arg(0) == "timer_d_on") TimerOnOff(timer_d_on, 3);
    if (WebServer.arg(0) == "timer_d_off") TimerOnOff(timer_d_off, 13);
  }
}

// Webpage for settings, to pass a command to the ESP or to update sketch
void settingspage()
{
  // Set fieldclasses initially to "normal"
  for (int count = 0; count < 7; count++) // Do this 7 times
  {
    FieldClassSetting[count]  = "field";
  }
  // Do a lot off stuff If Arguments were received from the webpage
  // So this all happens _after_ a "Save Settings" recalls this page
  if (WebServer.args() > 0 ) // First time no arguments changed, only after a "Save Settings" or a command
  {
    myTimeZone   = WebServer.arg("myTimeZone");
    myLatitude   = WebServer.arg("myLatitude").toFloat();
    myLongitude  = WebServer.arg("myLongitude").toFloat();
    TimesGPIO    = WebServer.arg("TimesGPIO").toInt();
    GPIOdelay    = WebServer.arg("GPIOdelay").toInt();
    GPIOhightime = WebServer.arg("GPIOhightime").toInt();
    String x = WebServer.arg("HistoryPage");
    if (x == "on") HistoryPage = 1;
    if (x != "on") HistoryPage = 0;

    // Check id the values are valid
    if (myLatitude    > 90  || myLatitude    < -90)  FieldClassSetting[1]  = "fielderror";
    if (myLongitude   > 180 || myLongitude   < -180) FieldClassSetting[2]  = "fielderror";
    if (TimesGPIO     > 99  || TimesGPIO     < 1)    FieldClassSetting[3]  = "fielderror";
    if (GPIOdelay     > 99  || GPIOdelay     < 1)    FieldClassSetting[4]  = "fielderror";
    if (GPIOhightime  > 99  || GPIOhightime  < 1)    FieldClassSetting[5]  = "fielderror";

    // Save the values, only if _all_ values are valid And turn the color of the saved fields into green

    // First check that none off the fields is in error
    int CheckFieldError = 0;
    for (int count = 0; count < 7; count++) // Do this 7 times
    {
      if (FieldClassSetting[count]        == "fielderror") CheckFieldError = 1;
    }

    if (CheckFieldError != 1) // So: if there is no field in error, then save
    {
      SaveToEEprom();         // Subroutine that saves Timer values to EEPROM
      // If the field is saved it turns green
      for (int count = 0; count < 7; count++) // Do this 7 times
      {
        FieldClassSetting[count]  = "fieldsaved";
      }
      MakeHeaderFooter();      // In case the value of HistoryPage has changed, then the menu needs to be rebuild
      ChangeSunTimes(1);       // In case the value of Latitude or Longitude has changed
    }
  }
  // Building the webpage
  webpage = webpageheader;

  // The form to fill in the settings

  // Define Form
  webpage += "<form action = '/settings' method = 'POST' autocomplete = 'off'>";
  webpage += "<h3>Settings:</h3><hr class='myhr'>";
  webpage += "<p>TimeZone: &nbsp; &nbsp; <input type='text' class='" + FieldClassSetting[0] +  "' value='" + myTimeZone +   "' maxlength='2' name='myTimeZone'   style='width: 30px;'></p>";
  webpage += "<p class='myp'>2 characters (eg NL) <a target='_blank' href='https://en.wikipedia.org/wiki/List_of_tz_database_time_zones'>Countrycodes</a>. This setting needs a Restart to take effect.</p><hr class='myhr'>";
  webpage += "<p>To determine SunSet and SunRise:</p>";
  webpage += "<p>Latitude: &nbsp; &nbsp; <input type='text' class='" + FieldClassSetting[1] +  "' value='" + myLatitude +   "' maxlength='6' name='myLatitude'   style='width: 70px;'></p>";
  webpage += "<p class='myp'>Decimal Degrees (eg 52.00) Value -180 to 180, 2 decimals</p>";
  webpage += "<p>Longitude:&nbsp; &nbsp; <input type='text' class='" + FieldClassSetting[2] +  "' value='" + myLongitude +  "' maxlength='8' name='myLongitude'  style='width: 70px;'></p>";
  webpage += "<p class='myp'>Decimal Degrees (eg 5.00) Value -90 to 90, 2 decimals</p><hr class='myhr'>";
  webpage += "<p>GPIO settings:</p>";
  webpage += "<p>TimesGPIO:&nbsp; &nbsp; <input type='text' class='" + FieldClassSetting[3] +  "' value='" + TimesGPIO +    "' maxlength='2' name='TimesGPIO'    style='width: 30px;'>times</p>";
  webpage += "<p class='myp'>Times the GPIO sends HIGH-LOW to GPIO (normally 1), 1 to 99</p>";
  webpage += "<p>GPIOdelay:&nbsp; &nbsp; <input type='text' class='" + FieldClassSetting[4] +  "' value='" + GPIOdelay +    "' maxlength='2' name='GPIOdelay'    style='width: 30px;'>x 10 ms</p>";
  webpage += "<p class='myp'>Wait-time in 10 milliseconds before sending next HIGH-LOW to GPIO, 1 to 99 (eg 50 = 500 ms)</p>";
  webpage += "<p>GPIOhightime:           <input type='text' class='" + FieldClassSetting[5] +  "' value='" + GPIOhightime + "' maxlength='2' name='GPIOhightime' style='width: 30px;'>x 10 ms</p>";
  webpage += "<p class='myp'>High-time in 10 milliseconds between sending the HIGH and LOW to GPIO, 1 to 99 (Example: 50 = 500 ms)</p><hr class='myhr'>";
  if (HistoryPage == 1) webpage += "<p>HistoryPage:&nbsp;      <input name=HistoryPage type='checkbox' checked></p>";
  if (HistoryPage == 0) webpage += "<p>HistoryPage:&nbsp;      <input name=HistoryPage type='checkbox'></p>";
  webpage += "<p class='myp'>Debug messages to Html-page 'History'</p><hr class='myhr'>";

  // Webpage Form "Save settings"
  webpage += "<br><p> &nbsp;<input type='submit' value='Save Settings'></form></p><hr>";


  // Commands
  webpage += "<h3>Commands:</h3>";
  // I made hidden fields with a dummy name and a visible submit button. The form (with 1 button) is submitted by that button and the action is the restartpage or the settingspage
  webpage += "<table style = 'width:275px'>";
  webpage += "<tr><td><form action = '/restartpage' method = 'POST'> &nbsp; <input type = 'hidden' name = 'dummy'><input type = 'submit' value = 'Restart ESP'></form></td>";
  webpage += "    <td><form action = '/defaultspage' method = 'POST'> &nbsp; <input type = 'hidden' name = 'dummy'><input type = 'submit' value = 'Defaults'></form></td>";
  webpage += "</tr></table><hr>";

  // Webupdater
  webpage += "<h3>WebUpdater: </h3>";
  webpage += "<iframe src = '/update' height = '140' width = '300'></iframe ><br>";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server
}


// Webpage to change the timers
void timerspage()
{
  // Set fieldclasses initially to "normal"
  for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
  {
    FieldClassOn[count]  = "field";
    FieldClassOff[count] = "field";
    FieldClassOnOffset[count]  = "field";
    FieldClassOffOffset[count] = "field";
  }

  // Do a lot off stuff If Arguments were received from the webpage
  // So this all happens _after_ a "Save Settings" recalls this page
  if (WebServer.args() > 0 ) // First time no arguments changed, only after a "Save Settings"
  {
    // All arguments from form to timer variables
    // No looping possible because of the WebServer.arg (?)
    HourTimerOn[0]    = WebServer.arg("HourTimerOn[0]").toInt();
    MinuteTimerOn[0]  = WebServer.arg("MinuteTimerOn[0]").toInt();
    HourTimerOff[0]   = WebServer.arg("HourTimerOff[0]").toInt();
    MinuteTimerOff[0] = WebServer.arg("MinuteTimerOff[0]").toInt();
    HourTimerOn[1]    = WebServer.arg("HourTimerOn[1]").toInt();
    MinuteTimerOn[1]  = WebServer.arg("MinuteTimerOn[1]").toInt();
    HourTimerOff[1]   = WebServer.arg("HourTimerOff[1]").toInt();
    MinuteTimerOff[1] = WebServer.arg("MinuteTimerOff[1]").toInt();
    HourTimerOn[2]    = WebServer.arg("HourTimerOn[2]").toInt();
    MinuteTimerOn[2]  = WebServer.arg("MinuteTimerOn[2]").toInt();
    HourTimerOff[2]   = WebServer.arg("HourTimerOff[2]").toInt();
    MinuteTimerOff[2] = WebServer.arg("MinuteTimerOff[2]").toInt();
    HourTimerOn[3]    = WebServer.arg("HourTimerOn[3]").toInt();
    MinuteTimerOn[3]  = WebServer.arg("MinuteTimerOn[3]").toInt();
    HourTimerOff[3]   = WebServer.arg("HourTimerOff[3]").toInt();
    MinuteTimerOff[3] = WebServer.arg("MinuteTimerOff[3]").toInt();

    // If the webserver arguments for the Offset don't exist on the page (if S/R is not checked) then the value is due to 'not existing' automatically '0'
    MinuteTimerOnOffset[0]  = WebServer.arg("MinuteTimerOnOffset[0]").toInt();
    MinuteTimerOffOffset[0] = WebServer.arg("MinuteTimerOffOffset[0]").toInt();
    MinuteTimerOnOffset[1]  = WebServer.arg("MinuteTimerOnOffset[1]").toInt();
    MinuteTimerOffOffset[1] = WebServer.arg("MinuteTimerOffOffset[1]").toInt();
    MinuteTimerOnOffset[2]  = WebServer.arg("MinuteTimerOnOffset[2]").toInt();
    MinuteTimerOffOffset[2] = WebServer.arg("MinuteTimerOffOffset[2]").toInt();
    MinuteTimerOnOffset[3]  = WebServer.arg("MinuteTimerOnOffset[3]").toInt();
    MinuteTimerOffOffset[3] = WebServer.arg("MinuteTimerOffOffset[3]").toInt();

    // Checkboxes for SunSet and SunRise to SunSet[0-3] and SunRise[0-3]
    SunSet[0] = WebServer.arg("SunSet[0]");
    SunSet[1] = WebServer.arg("SunSet[1]");
    SunSet[2] = WebServer.arg("SunSet[2]");
    SunSet[3] = WebServer.arg("SunSet[3]");

    SunRise[0] = WebServer.arg("SunRise[0]");
    SunRise[1] = WebServer.arg("SunRise[1]");
    SunRise[2] = WebServer.arg("SunRise[2]");
    SunRise[3] = WebServer.arg("SunRise[3]");

    // Take care of minutes not exceeding 59 and hours not exceeding 23
    for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
    {
      if (HourTimerOn[count]  > 23 || MinuteTimerOn[count]  > 59) FieldClassOn[count]  = "fielderror";
      if (HourTimerOff[count] > 23 || MinuteTimerOff[count] > 59) FieldClassOff[count] = "fielderror";
      if (MinuteTimerOnOffset[count]  > 99) FieldClassOnOffset[count]   = "fielderror";
      if (MinuteTimerOffOffset[count] > 99) FieldClassOffOffset[count]  = "fielderror";
    }

    // Save the values, only if _all_ values are valid And turn the color of the saved fields into green

    // First check that none off the fields is in error
    int CheckFieldError = 0;
    for (int count = 0; count < 4; count++) // Check this 4 times, for every timer
    {
      if (FieldClassOn[count]        == "fielderror") CheckFieldError = 1;
      if (FieldClassOff[count]       == "fielderror") CheckFieldError = 1;
      if (FieldClassOnOffset[count]  == "fielderror") CheckFieldError = 1;
      if (FieldClassOffOffset[count] == "fielderror") CheckFieldError = 1;
    }

    if (CheckFieldError != 1) // So: if there is no field in error, then save
    {
      SaveToEEprom();         // Subroutine that saves Timer values to EEPROM
      // If the field is readonly (in case of Sunrise/set) then the field remains grey otherwise it turns green
      for (int count = 0; count < 4; count++) // Check this 4 times, for every timer
      {
        if (SunSet[count]  != "checked") FieldClassOn[count]  = "fieldsaved";
        if (SunRise[count] != "checked") FieldClassOff[count] = "fieldsaved";
        FieldClassOnOffset[count]   = "fieldsaved";
        FieldClassOffOffset[count]  = "fieldsaved";
      }
    }
  }

  // Building the webpage
  webpage = webpageheader;

  // The form to fill in the new timer values

  // Define Table
  webpage += "<form action = '/timers' method = 'POST' autocomplete = 'off'>";
  webpage += "<table style = 'width:275px'><tr><td colspan = '2'><p>Checkbox S/R = SunSet/Rise<br>00:00 means timer \"not set\"</p><hr></tr></td>";

  // Build the html-code for each timer

  for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
  {
    MakeTimerHTML(count); // Make the html for the timer
  }

  // Webpage Form "Save settings"
  webpage += "<tr><td></td><td><p> &nbsp; &nbsp; &nbsp;<input type='submit' value='Save Settings'></form></p></td></tr></table>";

  webpage += webpagefooter;
  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
}


// Make the html for the timer (part of Timerspage)
void MakeTimerHTML(int count)
{
  TempTimerOnReadOnly  = "";                                                   // Initialy, the field is not readonly
  TempTimerOffReadOnly = "";                                                   // Initialy, the field is not readonly

  // Put the hour, minute and offset in temporary Strings so we can add a leading '0' if necessary
  // Just for looking better in html
  TempHourTimerOn = String(HourTimerOn[count]);
  TempMinuteTimerOn = String(MinuteTimerOn[count]);
  TempHourTimerOff = String(HourTimerOff[count]);
  TempMinuteTimerOff = String(MinuteTimerOff[count]);
  TempMinuteTimerOff = String(MinuteTimerOff[count]);

  TempMinuteTimerOnOffset  = String(MinuteTimerOnOffset[count]);
  TempMinuteTimerOffOffset = String(MinuteTimerOffOffset[count]);

  // add a '0' when number is under 10
  if (HourTimerOn[count]    < 10) TempHourTimerOn =    "0" + TempHourTimerOn;
  if (MinuteTimerOn[count]  < 10) TempMinuteTimerOn =  "0" + TempMinuteTimerOn;
  if (HourTimerOff[count]   < 10) TempHourTimerOff =   "0" + TempHourTimerOff;
  if (MinuteTimerOff[count] < 10) TempMinuteTimerOff = "0" + TempMinuteTimerOff;

  if (MinuteTimerOnOffset[count]   < 10) TempMinuteTimerOnOffset  =  "0" + TempMinuteTimerOnOffset;
  if (MinuteTimerOffOffset[count]  < 10) TempMinuteTimerOffOffset =  "0" + TempMinuteTimerOffOffset;

  // Set fields to readonly if sunset or sunrise is checked
  if (SunSet[count] == "checked")
  {
    TempTimerOnReadOnly = "readonly";
    if (FieldClassOn[count] == "field") FieldClassOn[count] = "fieldnochange";
  }

  if (SunRise[count] == "checked")
  {
    TempTimerOffReadOnly = "readonly";
    if (FieldClassOff[count] == "field") FieldClassOff[count] = "fieldnochange";
  }


  // Build the html-code for this Timer
  if (count == 0) webpage += "<tr><td><h3>Timer A</h3></td>";
  if (count == 1) webpage += "<tr><td><h3>Timer B</h3></td>";
  if (count == 2) webpage += "<tr><td><h3>Timer C</h3></td>";
  if (count == 3) webpage += "<tr><td><h3>Timer D</h3></td>";
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[count] +  "' value='" + TempHourTimerOn    + "' maxlength='2' name='HourTimerOn["   + String(count) + "]'"    + TempTimerOnReadOnly  + " style='width: 35px;'>";
  webpage +=     ":&#8201;<input type='text' class='" + FieldClassOn[count] +  "' value='" + TempMinuteTimerOn  + "' maxlength='2' name='MinuteTimerOn[" + String(count) + "]'"   + TempTimerOnReadOnly  + " style='width: 35px;'>";
  webpage +=          " S:<input type='checkbox' name='SunSet[" + String(count) + "]' value='checked'" + SunSet[count] + "><br>";
  if (SunSet[count] == "checked") webpage += "Offset &nbsp; -<input type='text' class='" + FieldClassOnOffset[count] +  "' value='" + TempMinuteTimerOnOffset  + "' maxlength='2' name='MinuteTimerOnOffset[" + String(count) + "]' style='width: 35px;'><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[count] + "' value='" + TempHourTimerOff   + "' maxlength='2' name='HourTimerOff[" + String(count) + "]'"   + TempTimerOffReadOnly + " style='width: 35px;'>";
  webpage +=     ":&#8201;<input type='text' class='" + FieldClassOff[count] + "' value='" + TempMinuteTimerOff + "' maxlength='2' name='MinuteTimerOff[" + String(count) + "]'" + TempTimerOffReadOnly + " style='width: 35px;'>";
  webpage +=          " R:<input type='checkbox' name='SunRise[" + String(count) + "]' value='checked'" + SunRise[count] + "><br>";
  if (SunRise[count] == "checked") webpage += "Offset &nbsp; +<input type='text' class='" + FieldClassOffOffset[count] + "' value='" + TempMinuteTimerOffOffset  + "' maxlength='2' name='MinuteTimerOffOffset[" + String(count) + "]' style='width: 35px;'><br>";
  webpage += "</p><hr></td></tr>";

}

// Webpage to change the HTTP-Commands
void httpcmdpage()
{
  // Set fieldclasses initially to "normal"
  for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
  {
    FieldClassOn[count]  = "field";
    FieldClassOff[count] = "field";
    FieldClassOnOffset[count]  = "field";
    FieldClassOffOffset[count] = "field";
  }

  // Do a lot off stuff If Arguments were received from the webpage
  // So this all happens _after_ a "Save Settings" recalls this page
  if (WebServer.args() > 0 ) // First time no arguments changed, only after a "Save Settings"
  {
    // All arguments from form to HTTP-Commands variables
    // No looping possible because of the WebServer.arg (?)
    HTTPcommandOn[0]    = WebServer.arg("HTTPcommandOn[0]");
    HTTPcommandOff[0]   = WebServer.arg("HTTPcommandOff[0]");
    HTTPcommandOn[1]    = WebServer.arg("HTTPcommandOn[1]");
    HTTPcommandOff[1]   = WebServer.arg("HTTPcommandOff[1]");
    HTTPcommandOn[2]    = WebServer.arg("HTTPcommandOn[2]");
    HTTPcommandOff[2]   = WebServer.arg("HTTPcommandOff[2]");
    HTTPcommandOn[3]    = WebServer.arg("HTTPcommandOn[3]");
    HTTPcommandOff[3]   = WebServer.arg("HTTPcommandOff[3]");


    // Take care of formatting the HTTPcommand
    for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
    {
      if (HTTPcommandOn[count].length()  > 0 && HTTPcommandOn[count].substring(0, 4)  != "http") FieldClassOn[count]   = "fielderror";
      if (HTTPcommandOff[count].length() > 0 && HTTPcommandOff[count].substring(0, 4) != "http") FieldClassOff[count]  = "fielderror";
    }

    // Save the values, only if _all_ values are valid And turn the color of the saved fields into green

    // First check that none off the fields is in error
    int CheckFieldError = 0;
    for (int count = 0; count < 4; count++) // Check this 4 times, for every timer
    {
      if (FieldClassOn[count]        == "fielderror") CheckFieldError = 1;
      if (FieldClassOff[count]       == "fielderror") CheckFieldError = 1;
    }

    if (CheckFieldError != 1) // So: if there is no field in error, then save
    {
      SaveToEEprom();         // Subroutine that saves Timer values to EEPROM
      for (int count = 0; count < 4; count++) // Check this 4 times, for every timer
      {
        FieldClassOn[count]   = "fieldsaved";
        FieldClassOff[count]  = "fieldsaved";
      }
    }
  }

  // Building the webpage
  webpage = webpageheader;

  // The form to fill in the new timer HTTP command values
  webpage += "<form action = '/httpcmdpage' method = 'POST' autocomplete = 'off'>";
  webpage += "<p>Enter http(s)-cmd for each timer<br>Empty field means NO http-command</p><hr>";

  // Build the html-code for each timer

  for (int count = 0; count < 4; count++) // Do this 4 times, for every timer
  {
    MakeHTTPcommandHTML(count); // Make the html for the timer
  }

  // Webpage Form "Save settings"
  webpage += "&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <input type='submit' value='Save Settings'></form></p>";

  webpage += webpagefooter;
  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
}

// Make the html for the HTTP-Commands (part of httpcmdpage)
void MakeHTTPcommandHTML(int count)
{

  // Build the html-code for this HTTPcommand
  if (count == 0) webpage += "<h3>HTTP Command A</h3>";
  if (count == 1) webpage += "<h3>HTTP Command B</h3>";
  if (count == 2) webpage += "<h3>HTTP Command C</h3>";
  if (count == 3) webpage += "<h3>HTTP Command D</h3>";
  webpage += "<p>ON :  <input type='text' class='" + FieldClassOn[count]  + "' value='" + HTTPcommandOn[count]  + "' maxlength='255' name='HTTPcommandOn["  + String(count) + "]'" + " style='width: 275px; text-align: left;'>";
  webpage += "<br>OFF: <input type='text' class='" + FieldClassOff[count] + "' value='" + HTTPcommandOff[count] + "' maxlength='255' name='HTTPcommandOff[" + String(count) + "]'" + " style='width: 275px; text-align: left;'>";
  webpage += "</p><hr>";

}

// wwwroot - (Info) Webpage for displaying the system state
void wwwroot()
{
  // Build the string for the webpage
  webpage = webpageheader;
  webpage += "<p>Time: &nbsp; &nbsp; " + TimerTimeZone.dateTime("H~:i~:s") + "<br>";
  webpage += "Date: &nbsp; &nbsp; " + TimerTimeZone.dateTime("D dS M Y") + "<br>";
  webpage += "Timezone: " + TimerTimeZone.dateTime("e") + "<br>";
  webpage += "Lat/Long: " + String(myLatitude) + "/" + String(myLongitude) + " <a target='_blank' href='https://www.latlong.net/c/?lat=" + String(myLatitude) + "&amp;long=" + String(myLongitude) + "'>On Map</a><br>";
  webpage += "LastSync: " + dateTime(lastNtpUpdateTime() - (TimerTimeZone.getOffset() * 60), "d~-m~-Y H~:i~:s") + "<br>";
  webpage += "Diff.UTC: " + TimerTimeZone.dateTime("P") + "<br>";
  webpage += "Sunrise: &nbsp;" + mySunrise$ + "<br>";
  webpage += "Sunset: &nbsp; " + mySunset$ + "<br>";
  if (TimerTimeZone.isDST() == 0) webpage += "DST now: &nbsp;No<br>";
  if (TimerTimeZone.isDST() == 1) webpage += "DST now: &nbsp;Yes<br>";
  webpage += "</p><hr><p>WiFi: &nbsp; &nbsp; " + WiFi.SSID() + " " + "<br>";
  webpage += "IP: &nbsp; &nbsp; &nbsp; " + WiFi.localIP().toString() + "<br>";
  webpage += "MAC: &nbsp; &nbsp; &nbsp;" + String(WiFi.macAddress()) + "<br>";
  webpage += "Debug: &nbsp; &nbsp;" + String(DebugLevel) + "<br>";

  // I made a hidden field with a dummy name and a visible submit button. The form (with 1 button) is submitted by that button and the action is the wwwroot
  webpage += "<hr><form action='/' method='POST'><input type='hidden' name='dummy'>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <input type='submit' value='Refresh'></form></p>";

  webpage += webpagefooter;

  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
}

// Webpage for Restart Yes/No
void restartpage()
{
  if (WebServer.args() > 0 ) // First time no arguments changed, only after a Yes
  {
    if (WebServer.arg("answer") == "yes")
    {
      webpage =  "<html><body><br> Message: Restart initiated...<br><br><a href='/'> Click here after few seconds...</a></body></html>";
      WebServer.send(200, "text / html", webpage); // Send the webpage to the server
      delay(1000);
      ESP.restart();
    }
  }
  webpage = webpageheader;
  webpage += "<h3>ESP Restart<br></h3>Are you sure?<br><br>";
  // To produce server arguments i made hidden fields with a standard value and a visible submit button. The fields are questioned in the if-loop at the top of the restartpage.
  webpage += "<table style = 'width:275px'>";
  webpage += "<tr><td><form action = '/restartpage' method = 'POST'> &nbsp; &nbsp; <input type = 'hidden' name = 'answer' value = 'yes'><input type = 'submit' value = 'Yes'></form></td>";
  webpage += "</tr></table><br>If No, click any link in the menu...<br>";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server
}

// Webpage for Restart and go to defaults Yes/No
void defaultspage()
{
  if (WebServer.args() > 0 ) // First time no arguments changed, only after a Yes
  {
    if (WebServer.arg("answer") == "yes")
    {
      webpage =  "<html><body><br> Message: Settings and Timers to default and Restart initiated...<br><br><a href='/'> Click here after few seconds...</a></body></html>";
      WebServer.send(200, "text / html", webpage); // Send the webpage to the server
      delay(1000);
      FirstRun();
      delay(1000);
      ESP.restart();
    }
  }
  webpage = webpageheader;
  webpage += "<h3>ESP to Defaults<br></h3> - Settings will go to default<br> - All Timers will go to 00:00<br> - All HTTP-commands will be empty<br> - ESP will restart !<br><br>Are you sure?<br><br>";
  // To produce server arguments i made hidden fields with a standard value and a visible submit button. The fields are questioned in the if-loop at the top of the settingspage.
  webpage += "<table style = 'width:275px'>";
  webpage += "<tr><td><form action = '/defaultspage' method = 'POST'> &nbsp; &nbsp; <input type = 'hidden' name = 'answer' value = 'yes'><input type = 'submit' value = 'Yes'></form></td>";
  webpage += "</tr></table><br>If No, click any link in the menu...<br>";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server
}

// Webpage for 404
void PageNotFound()
{
  WebServer.send(404, "text/plain", "404: This page does not exist on the TimerClock"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

// Make standard header, Title, Menu and footer used for every webpage
void MakeHeaderFooter()
{
  // Make Title
  webpageheader =  "<html><head><title>TimerClock " + version$ + "</title><hr>";
  // With the next line it also looks good on your tablet or phone (not my expertise -> Google is your best friend!)
  webpageheader += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'/>";

  // Setting the color for the page and the fonts for the content
  webpageheader += "<style>";
  webpageheader += "body {background-color: #E6E6E6;}";
  webpageheader += "h1 {font-family: Arial, Helvetica, Sans-Serif; Color: blue;}";                                        // H1 text
  webpageheader += "h3 {font-family: Arial, Helvetica, Sans-Serif; Color: black;}";                                       // H3 text
  webpageheader += "h4 {font-family: Arial, Helvetica, Sans-Serif; Color: black; font-size: 75%}";                        // Small text
  webpageheader += "p {font-family: Courier; Color: green; margin-top: 0; margin-bottom: 0;}";                            // Standard text
  // Setting the Classes
  webpageheader += ".field {font-family: Courier; Background-color: #FFFF78; Color: black; text-align: center;}";         // Yellow background in fields
  webpageheader += ".fieldsaved {font-family: Courier; Background-color: #78FF78; Color: black; text-align: center;}";    // Green background in fields of the Timerpage after Save Settings
  webpageheader += ".fielderror {font-family: Courier; Background-color: #FF7878; Color: black; text-align: center;}";    // Red background in fields of the Timerpage after Error
  webpageheader += ".fieldnochange {font-family: Courier; Background-color: #BBBBBB; Color: black; text-align: center;}"; // Grey background in fields of the Timerpage that can't be changed
  webpageheader += ".myhr {border-top: 1px solid green; margin-top: 3px; margin-bottom: 3px;}";                           // <hr> in green with small margins (settingspage)
  webpageheader += ".myp {font-size: 11px; font-style: italic; color: gray;}";                                            // Class for explanation text (settingspage)
  webpageheader += "</style></head><body>";

  // Start of content with menu
  webpageheader += "<h1>TimerClock " + version$ + "</h1>";
  webpageheader += "<p><strong><a href='/timers'>Timers</a> &nbsp; <a href='/httpcmdpage'>HTTPcmd</a> &nbsp; <a href='/controls'>Controls</a><br><br>";
  if (HistoryPage == 0) webpageheader += " &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;<a href='/'>Info</a>&nbsp; &nbsp; &nbsp; <a href='/settings'>Settings</a></strong></p><hr>";
  if (HistoryPage == 1) webpageheader += " <a href='/history'>History</a> &nbsp;<a href='/'>Info</a>&nbsp; &nbsp; &nbsp; <a href='/settings'>Settings</a></strong></p><hr>";

  // Make standard footer used for every webpage
  webpagefooter = "<br><hr>";
  webpagefooter += "<i>Made for ESP8266<br>John Halfords, the Netherlands</i>";
  webpagefooter += "</body></html>";
}


// history - Webpage for displaying the last 20 messages
void history()
{
  // Build the string for the webpage
  webpage = webpageheader;
  webpage += "<h3>History Page</h3><p>";

  for (int count = 19; count >= 0; count--) // Do this 20 times, for every History line
  {
    webpage += HistoryMsg[count] + "<br>";
  }

  webpage += "<hr><form action='/history' method='POST'><input type='hidden' name='go'>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <input type='submit' value='Refresh'></form></p>";
  webpage += webpagefooter;
  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
}


// Subroutine to switch ON or OFF timer (Argument x is timer_x_on or timer_x_off, where x=a-d, Argument y is 0-3 for timer ON or 10-13 for Timer OFF)
void TimerOnOff(int x, int y)
{
  for (int count = 0; count < TimesGPIO; count++)  // Loop: How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
  {
    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.print("Sending HIGH-LOW to port: ");
    if (DebugLevel >= 1) Serial.println(x);

    digitalWrite(x, HIGH);
    delay(GPIOhightime * 10);
    digitalWrite(x, LOW);
    delay(GPIOdelay * 10);
  }

  // Call HTTPcommand defined with this timer
  if (y <= 3) // Means Timer ON is called
  {
    HTTPClient http;
    http.begin(HTTPcommandOn[y]);
    http.GET(); // int ReturnCode = http.GET(); geeft nog iets terug... >0 succes?
    http.end();

    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.print("Sending HTTPcommand for Timer ON: ");
    if (DebugLevel >= 1) Serial.println(y);
  }


  if (y >= 10) // Means Timer OFF is called
  {
    HTTPClient http;
    http.begin(HTTPcommandOff[y - 10]);
    http.GET(); // int ReturnCode = http.GET(); geeft nog iets terug... >0 succes?
    http.end();

    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.print("Sending HTTPcommand for Timer OFF: ");
    if (DebugLevel >= 1) Serial.println(y - 10);
  }
}


// Subroutine to add Last History Message to History array
void MakeHistory(String LastMessage)
{
  // First Shift all History Message one back
  for (int count = 0; count < 19; count++) // Do this 19 times, for every Historyline except line 20 (array #19)
  {
    HistoryMsg[count] = HistoryMsg[count + 1];
  }
  // Then the Last History message moves in #20 = array#19
  HistoryMsg[19] = TimerTimeZone.dateTime("dM H~:i~:s ") + LastMessage;
}

// Subroutine that runs only the first time to fill settings with Default values
void FirstRun()
{

  for (int count = 0; count <= 3; count++)             // Do this 4 times, once for every timer
  {
    HourTimerOn[count] = 0;
    HourTimerOff[count] = 0;
    MinuteTimerOn[count] = 0;
    MinuteTimerOff[count] = 0;
    MinuteTimerOnOffset[count] = 0;
    MinuteTimerOffOffset[count] = 0;
    SunSet[count] = "";
    SunRise[count] = "";
    HTTPcommandOn[count] = "";
    HTTPcommandOff[count] = "";
  }

  myTimeZone = "NL";    // Timezone (2 characters, countrycodes; https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)
  myLatitude = 52.00;   // Latitude (must be in decimal degrees (DD), e.g. 52.00, used to determine SunSet and SunRise)
  myLongitude = 5.00;   // Longitude (must be in decimal degrees (DD), e.g. 5.00, used to determine SunSet and SunRise)
  //                    // Tip: Click your location in Google Maps, wait, click again (no double-click) to see your values
  TimesGPIO = 1;        // How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
  //                    // *(my remote is "fire-and-forget" so i set this value to 3). Value minimum = 1, max = 99
  GPIOdelay = 50;       // Time in 10 milliseconds to wait for sending the next HIGH-LOW to the same GPIO. Value minimum = 1, max = 99 (Example: 50 = 500 ms)
  GPIOhightime = 50;    // Time in 10 milliseconds to wait between sending the HIGH and LOW to the GPIO, so actually 'how long is the GPIO HIGH'. Value minimum = 1, max = 99 (Example: 50 = 500 ms)
  HistoryPage = 0;      // Debug messages to Html-page 'History'. 0 (unchecked) = NO Historypage, 1 (checked) = Historypage (format on Historypage is D-M-YYYY HH:MM:SS (local time) + message)

  SaveToEEprom();       // Save these default values to EEPROM

  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("First Run default values set and saved.");
  if (HistoryPage == 1) MakeHistory("First Run default values set and saved.");
}

///////////////////  End of Subroutines  //////////////////////
