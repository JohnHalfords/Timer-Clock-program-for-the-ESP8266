//                                             - - o o - -                                       //
//                                          11th November 2019                                   //
//// //// ////     ESP8266 Project Timerclock by John Halfords, The Netherlands      //// //// ////
//// //// ////        Comments and questions welcome at: halfordsj@gmail.com         //// //// ////
//                                                                                               //
//                                       Please read Readme.md                                   //
//                                             - - o o - -                                       //


// Includes of Libraries
#include <ESP8266WiFiMulti.h>          // Load library to connect to WiFi and automaticly choose the strongest network
#include <ESP8266WebServer.h>          // Load library to run the WebServer
#include <ESP8266HTTPUpdateServer.h>   // Load library for updating sketch through http
#include <EEPROM.h>                    // Load library to write to, and read from EEPROM
#include <ezTime.h>                    // Load Very well documented Time libary with everything you possibly want! (https://github.com/ropg/ezTime)
#include <Dusk2Dawn.h>                 // Load Library to define Sunset and Sunrise (https://github.com/dmkishi/Dusk2Dawn)

// Create instances
ESP8266WiFiMulti wifiMulti;            // Create an instance for connecting to wifi
ESP8266WebServer WebServer(80);        // Create an instance for the webserver
ESP8266HTTPUpdateServer WebUpdater;    // Create an instance for the http update server (to upload a new sketch via the website)

// User declarations
#include "UserDeclarations.h"          // Wifi settings, Timezone, Location, DebugLevel and pinning can be changed to your design

// Program declarations // DON'T change! Change file "UserDeclarations.h" if your design if different from mine
String version$ = "v2.01";                        // Version of program
String webpageheader, webpagefooter, webpage;     // Strings for building the webpages
String syncstatus;                                // String that holds the status of the timesync
String Command;                                   // String that holds the Command given in the Commandpage
unsigned int mySunrise, mySunset;                 // Integer that holds the numerical value of todays Sunrise and Sunset (in minutes from midnight)
String mySunrise$, mySunset$;                     // String that holds the text value of todays Sunrise and Sunset HH:MM
String SunSet[4], SunRise[4];                     // String is "checked" when Sunrise/set time is active,  "" (unchecked) when user timer time is set
int SunBits;                                      // 8 bits that hold the settings of the checkboxes SunSet and SunRise
unsigned int HourTimerOn[4], HourTimerOff[4];     // Variable arrays for Timers 1-4
unsigned int MinuteTimerOn[4], MinuteTimerOff[4]; // Variable arrays for Timers 1-4
String FieldClassOn[4], FieldClassOff[4];         // String for the html-class (yellow, grey or green color) for fields on the Timerpage

Timezone TimerTimeZone;                           // Create an instance for ezTime, Timer's timezone

////// Setup
void setup()
{
  Serial.begin(myBaudrate);                      // Start the Serial communication
  if (DebugLevel >= 1) Serial.println("\nStarting Setup...");

  // Declaring output pins
  pinMode(wifiindicator, OUTPUT); //  .     // the wifiindicator blinks during connecting and is ON while connected
  pinMode(timer_a_on,    OUTPUT); //   .
  pinMode(timer_a_off,   OUTPUT); //    .
  pinMode(timer_b_on,    OUTPUT); //     .
  pinMode(timer_b_off,   OUTPUT); //      . <- If your design is different from mine: Change User declarations! NOT these!
  pinMode(timer_c_on,    OUTPUT); //     .
  pinMode(timer_c_off,   OUTPUT); //    .
  pinMode(timer_d_on,    OUTPUT); //   .
  pinMode(timer_d_off,   OUTPUT); //  .

  StartWiFi();                    // Subroutine that starts WiFi connection to the stongest SSID given

  // Handle the Pages
  WebServer.on("/", wwwroot);                   // is also the 'info-page'
  WebServer.on("/controls", controlspage);      // the page to directly control the timers
  WebServer.on("/command", commandpage);        // the page for reset/restart and update sketch via the website (through http)
  WebServer.on("/timers", timerspage);          // the page to set the timer times and check SunSet/Rise
  WebServer.onNotFound(PageNotFound);           // when a page is requested that doesn't exist

  WebServer.begin();                            // Start the webserver
  WebUpdater.setup(&WebServer);                 // Start the WebUpdater as part of the WebServer

  TimerTimeZone.setLocation(myTimeZone);      // Set timezone for ezTime to the setting you gave in UderDeclarations.h
  waitForSync();                                // Wait for time to sync (ezTime)

  MakeHeaderFooter();                           // Make standard header, Title, Menu and footer used for every webpage
  ReadTimersEEprom();                           // Subroutine that reads Timer values from EEPROM
  ChangeSunTimes(1);                            // Subroutine that changes the Sunrise and Sunset. Argument = 0 every night at 00:00:30. Argument = 1 just do it

  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("TimerClock " + version$ + " Ready!");
}

// Main loop
void loop()
{
  WebServer.handleClient();         // Listen for HTTP requests from clients
  ChangeSunTimes(0);                // Subroutine that changes the Sunrise and Sunset. Argument = 0 every night at 00:00:30. Argument = 1 just do it
  CheckTimers();                    // Subroutine that checks if timers match the current time
}

///////////////////  End of Program  //////////////////////


// // // // // // // // // // // // // // // // // // // //


///////////////////  Subroutines  /////////////////////////


// Subroutine that changes the Sunrise and Sunset, argument 0 means every night at 00:00:30, argument 1 means Just Do It! (for startup)
void ChangeSunTimes(bool DoIt)
{
  if ((TimerTimeZone.hour() == 0 && TimerTimeZone.minute() == 0 && TimerTimeZone.second() == 30) || (DoIt == 1)) // Do this every nigth at 00:00:30 (DoIt = 0) or at Startup (DoIt = 1)
  {
    Dusk2Dawn mySunTimes (myLatitude, myLongitude, (-TimerTimeZone.getOffset() / 60)); // Create instance for Sunrise and Sunset
    //                                                                                 // Syntax: Dusk2Dawn <location instance name> (Latitude, Longitude, Time offset in minutes to UTC (minus!))

    mySunrise = mySunTimes.sunrise(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), TimerTimeZone.isDST()); // Store todays Sunrise in Variable mySunrise
    mySunset = mySunTimes.sunset(TimerTimeZone.year(), TimerTimeZone.month(), TimerTimeZone.day(), TimerTimeZone.isDST());   // Store todays Sunset in Variable mySunset
    //                                                                                                                       // Syntax: <instance name>.sunrise(int year, int month, int day, bool <is DST in effect?>)

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
    if (DebugLevel >= 1) Serial.println("Changed Sunrise and Sunset for today. Sunrise = " + mySunrise$ + " and Sunset = " + mySunset$);

    ChangeTimers2Sun();    // Subroutine that changes the timersettings for the timers that have the checkbox Sunrise and/or Sunset
    delay(2000);           // To prevent that this happens more than once this second.
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

    if (SunRise[count] == "checked")                   // if the Sunrise checkbox is checked you want to change the setting to the Sunrise time
    {
      HourTimerOff[count]   = hour(mySunrise * 60);    // mySunrise is minutes after 0:00 and the hour() function expects seconds, so * 60
      MinuteTimerOff[count] = minute(mySunrise * 60);
    }
  }
}


// Subroutine that checks if current time mathes one of the timers
void CheckTimers()
{
  // We store the current time in variables now we have all the time to do things with the temporary 'local time' without moving one second in time...
  int HourLocal = TimerTimeZone.hour();
  int MinuteLocal = TimerTimeZone.minute();
  int SecondLocal = TimerTimeZone.second();

  for (int count = 0; count <= 3; count++)  // Do this 4 times, once for every timer
  {
    if (not (HourLocal == 0 && MinuteLocal == 0)) // At 00:00 the timer is assumed to be "not set"
    {
      // Timer ON
      if (HourLocal == HourTimerOn[count] && MinuteLocal == MinuteTimerOn[count] && SecondLocal == 1) // Let the ESP activate the corresponding GPIO for 'TIMER ON' on second 1
      {
        if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
        if (DebugLevel >= 1) Serial.print("Timer ");
        if (DebugLevel >= 1) Serial.print(count);
        if (DebugLevel >= 1) Serial.println(" ON! ");
        if (count == 0) TimerOnOff(timer_a_on);
        if (count == 1) TimerOnOff(timer_b_on);
        if (count == 2) TimerOnOff(timer_c_on);
        if (count == 3) TimerOnOff(timer_d_on);
        delay(1000);
      }

      // Timer Off
      if (HourLocal == HourTimerOff[count] && MinuteLocal == MinuteTimerOff[count] && SecondLocal == 1) // Let the ESP activate the corresponding GPIO for 'TIMER OFF' on second 1
      {
        if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
        if (DebugLevel >= 1) Serial.print("Timer ");
        if (DebugLevel >= 1) Serial.print(count);
        if (DebugLevel >= 1) Serial.println(" OFF! ");
        if (count == 0) TimerOnOff(timer_a_off);
        if (count == 1) TimerOnOff(timer_b_off);
        if (count == 2) TimerOnOff(timer_c_off);
        if (count == 3) TimerOnOff(timer_d_off);
        delay(1000);
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
  if (DebugLevel >= 1) Serial.print("and connected to: ");
  if (DebugLevel >= 1) Serial.println(WiFi.SSID());               // Tell us what network we're connected to
  if (DebugLevel >= 1) Serial.print("with IP address : ");
  if (DebugLevel >= 1) Serial.println(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  if (DebugLevel >= 1) Serial.print("and MAC address : ");
  if (DebugLevel >= 1) Serial.println(WiFi.macAddress());         // Send the MAC address of the ESP8266 to the computer
}


// Subroutine that reads Timer values from EEPROM
void  ReadTimersEEprom()
{
  EEPROM.begin(17);
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
  SunBits           = EEPROM.read(16);

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
  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("Done reading settings from memory.");
}


// Subroutine that saves Timer values to EEPROM
void  SaveTimersEEprom()
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
  EEPROM.begin(17);
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
  EEPROM.write(16, SunBits);
  EEPROM.end();
  if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
  if (DebugLevel >= 1) Serial.println("Done saving setings to memory.");
}


//////////////////// Subroutines that write the pages ////////////////////


// Webpage to directly control the timers
void controlspage()
{
  webpage = webpageheader;
  // To produce server arguments i made hidden fields with a standard value and a visible submit button.
  // The fields are questioned in the if-loop further on.
  webpage += "<br><table style='width:275px'><tr><td><h3>Timer A</h3></td><td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_a_on'><input type='submit' value='ON'></form></td>";
  webpage += "<td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_a_off'><input type='submit' value='OFF'></form></td></tr>";
  webpage += "<tr><td><h3>Timer B</h3></td><td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_b_on'><input type='submit' value='ON'></form></td>";
  webpage += "<td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_b_off'><input type='submit' value='OFF'></form></td></tr>";
  webpage += "<tr><td><h3>Timer C</h3></td><td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_c_on'><input type='submit' value='ON'></form></td>";
  webpage += "<td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_c_off'><input type='submit' value='OFF'></form></td></tr>";
  webpage += "<tr><td><h3>Timer D</h3></td><td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_d_on'><input type='submit' value='ON'></form></td>";
  webpage += "<td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_d_off'><input type='submit' value='OFF'></form></td></tr>";
  webpage += "<tr><td><h3>Timer A-D</h3></td><td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_all_on'><input type='submit' value='ON'></form></td>";
  webpage += "<td><form action='/controls' method='POST'><input type='hidden' name='go' value='timer_all_off'><input type='submit' value='OFF'></form></td></tr></table>";

  webpage += webpagefooter;

  WebServer.send(200, "text/html", webpage); // Send the webpage to the server

  // if loop for questioning the button pressed
  if (WebServer.args() > 0 )
  {
    // activate the output for timers
    if (WebServer.arg(0) == "timer_a_on") TimerOnOff(timer_a_on);
    if (WebServer.arg(0) == "timer_a_off") TimerOnOff(timer_a_off);
    if (WebServer.arg(0) == "timer_b_on") TimerOnOff(timer_b_on);
    if (WebServer.arg(0) == "timer_b_off")TimerOnOff(timer_b_off);
    if (WebServer.arg(0) == "timer_c_on") TimerOnOff(timer_c_on);
    if (WebServer.arg(0) == "timer_c_off") TimerOnOff(timer_c_off);
    if (WebServer.arg(0) == "timer_d_on") TimerOnOff(timer_d_on);
    if (WebServer.arg(0) == "timer_d_off") TimerOnOff(timer_d_off);
    if (WebServer.arg(0) == "timer_all_on") TimerAllOn();
    if (WebServer.arg(0) == "timer_all_off") TimerAllOff();
  }
}

// Webpage to pass a command to the ESP or to update sketch
void commandpage()
{
  webpage = webpageheader;
  webpage += "<h3>Commands:</h3>";
  // To produce server arguments i made hidden fields with a standard value and a visible submit button. The fields are questioned in the if-loop further on.
  webpage += "<table style='width:275px'><tr><td><form action='/command' method='POST'>&nbsp; &nbsp; &nbsp; <input type='hidden' name='go' value='reset'><input type='submit' value='Reset ESP'></form></td>";
  webpage += "<td><form action='/command' method='POST'><input type='hidden' name='go' value='restart'><input type='submit' value='Restart ESP'></form></td></tr></table>";

  webpage += "<h3>WebUpdater:</h3>";
  webpage += "<iframe src='/update' height='45' width='350'></iframe><br>";

  webpage += webpagefooter;

  WebServer.send(200, "text/html", webpage); // Send the webpage to the server

  if (WebServer.args() > 0 ) // Arguments were received, which button is pressed?
  {
    // activate the output for timers
    if (WebServer.arg(0) == "reset") ESP.reset();
    if (WebServer.arg(0) == "restart") ESP.restart();
  }
}


// Webpage to change the timers
void timerspage()
{

  // Make temporary strings for functionality in HTML forms
  String TempHourTimerOn, TempMinuteTimerOn, TempHourTimerOff, TempMinuteTimerOff;
  String TempTimerOnReadOnly, TempTimerOffReadOnly;

  // Set fields initially to "normal"
  for (int count = 0; count < 4; count++)
  {
    FieldClassOn[count]  = "field";
    FieldClassOff[count] = "field";
  }

  // Do a lot off stuff If Arguments were received from the webpage
  // So this all happens after a "Save Settings" and recall this page
  if (WebServer.args() > 0 )
  {
    // All arguments from form to timer variables
    HourTimerOn[0] = WebServer.arg("HourTimerOn[0]").toInt();
    MinuteTimerOn[0] = WebServer.arg("MinuteTimerOn[0]").toInt();
    HourTimerOff[0] = WebServer.arg("HourTimerOff[0]").toInt();
    MinuteTimerOff[0] = WebServer.arg("MinuteTimerOff[0]").toInt();
    HourTimerOn[1] = WebServer.arg("HourTimerOn[1]").toInt();
    MinuteTimerOn[1] = WebServer.arg("MinuteTimerOn[1]").toInt();
    HourTimerOff[1] = WebServer.arg("HourTimerOff[1]").toInt();
    MinuteTimerOff[1] = WebServer.arg("MinuteTimerOff[1]").toInt();
    HourTimerOn[2] = WebServer.arg("HourTimerOn[2]").toInt();
    MinuteTimerOn[2] = WebServer.arg("MinuteTimerOn[2]").toInt();
    HourTimerOff[2] = WebServer.arg("HourTimerOff[2]").toInt();
    MinuteTimerOff[2] = WebServer.arg("MinuteTimerOff[2]").toInt();
    HourTimerOn[3] = WebServer.arg("HourTimerOn[3]").toInt();
    MinuteTimerOn[3] = WebServer.arg("MinuteTimerOn[3]").toInt();
    HourTimerOff[3] = WebServer.arg("HourTimerOff[3]").toInt();
    MinuteTimerOff[3] = WebServer.arg("MinuteTimerOff[3]").toInt();

    // Checkboxes for SunSet and SunRise to SunSet[0-3] and SunRise[0-3]
    SunSet[0] = WebServer.arg("SunSetA");
    SunSet[1] = WebServer.arg("SunSetB");
    SunSet[2] = WebServer.arg("SunSetC");
    SunSet[3] = WebServer.arg("SunSetD");

    SunRise[0] = WebServer.arg("SunRiseA");
    SunRise[1] = WebServer.arg("SunRiseB");
    SunRise[2] = WebServer.arg("SunRiseC");
    SunRise[3] = WebServer.arg("SunRiseD");

    // Take care of minutes not exceeding 59 and hours not exceeding 23
    if (HourTimerOn[0]  > 23 || MinuteTimerOn[0]  > 59) FieldClassOn[0]  = "fielderror";
    if (HourTimerOff[0] > 23 || MinuteTimerOff[0] > 59) FieldClassOff[0] = "fielderror";
    if (HourTimerOn[1]  > 23 || MinuteTimerOn[1]  > 59) FieldClassOn[1]  = "fielderror";
    if (HourTimerOff[1] > 23 || MinuteTimerOff[1] > 59) FieldClassOff[1] = "fielderror";
    if (HourTimerOn[2]  > 23 || MinuteTimerOn[2]  > 59) FieldClassOn[2]  = "fielderror";
    if (HourTimerOff[2] > 23 || MinuteTimerOff[2] > 59) FieldClassOff[2] = "fielderror";
    if (HourTimerOn[3]  > 23 || MinuteTimerOn[3]  > 59) FieldClassOn[3]  = "fielderror";
    if (HourTimerOff[3] > 23 || MinuteTimerOff[3] > 59) FieldClassOff[3] = "fielderror";

    // Save the values, only if _all_ values are valid And turn the color of the saved fields into green
    if (FieldClassOn[0] != "fielderror" && FieldClassOn[1] != "fielderror" && FieldClassOn[2] != "fielderror" && FieldClassOn[3] != "fielderror" && FieldClassOff[0] != "fielderror" && FieldClassOff[1] != "fielderror" && FieldClassOff[2] != "fielderror" && FieldClassOff[3] != "fielderror")
    {
      SaveTimersEEprom();       // Subroutine that saves Timer values to EEPROM
      // If the field is readonly (in case of Sunrise/set) then the field remains grey otherwise it turns green
      for (int count = 0; count < 4; count++)
      {
        if (SunSet[count]  != "checked") FieldClassOn[count]  = "fieldsaved";
        if (SunRise[count] != "checked") FieldClassOff[count] = "fieldsaved";
      }
    }
  }

  // Building the webpage
  webpage = webpageheader;

  // The form to fill in the new timer values
  // Yes you see a lot of repetative lines, no i couldn't make a for loop. If you manage let me know, please!
  webpage += "<form action='/timers' method='POST' autocomplete='off'>";
  webpage += "<table style='width:275px'><tr><td colspan='2'><p>Checkbox S/R = SunSet/Rise</p><hr></tr></td>";

  // TIMER A ////
  TempTimerOnReadOnly  = "";                                                   // Initialy, the field is not readonly
  TempTimerOffReadOnly = "";                                                   // Initialy, the field is not readonly
  TempHourTimerOn = String(HourTimerOn[0]);
  TempMinuteTimerOn = String(MinuteTimerOn[0]);
  TempHourTimerOff = String(HourTimerOff[0]);
  TempMinuteTimerOff = String(MinuteTimerOff[0]);
  if (HourTimerOn[0]    < 10) TempHourTimerOn =    "0" + TempHourTimerOn;      // add a '0' when number is under 10
  if (MinuteTimerOn[0]  < 10) TempMinuteTimerOn =  "0" + TempMinuteTimerOn;    // add a '0' when number is under 10
  if (HourTimerOff[0]   < 10) TempHourTimerOff =   "0" + TempHourTimerOff;     // add a '0' when number is under 10
  if (MinuteTimerOff[0] < 10) TempMinuteTimerOff = "0" + TempMinuteTimerOff;   // add a '0' when number is under 10

  // Set fields to readonly if sunset or sunrise is checked
  if (SunSet[0] == "checked")
  {
    TempTimerOnReadOnly = "readonly";
    if (FieldClassOn[0] == "field") FieldClassOn[0] = "fieldnochange";
  }

  if (SunRise[0] == "checked")
  {
    TempTimerOffReadOnly = "readonly";
    if (FieldClassOff[0] == "field") FieldClassOff[0] = "fieldnochange";
  }

  webpage += "<tr><td><h3>Timer A</h3></td>";
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[0] +  "' value='" + TempHourTimerOn    + "' size='4' maxlength='2' name='HourTimerOn[0]'"    + TempTimerOnReadOnly  + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOn[0] +  "' value='" + TempMinuteTimerOn  + "' size='4' maxlength='2' name='MinuteTimerOn[0]'"  + TempTimerOnReadOnly  + ">";
  webpage +=          " S:<input type='checkbox' name='SunSetA' value='checked'" + SunSet[0] + "><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[0] + "' value='" + TempHourTimerOff   + "' size='4' maxlength='2' name='HourTimerOff[0]'"   + TempTimerOffReadOnly + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOff[0] + "' value='" + TempMinuteTimerOff + "' size='4' maxlength='2' name='MinuteTimerOff[0]'" + TempTimerOffReadOnly + ">";
  webpage +=          " R:<input type='checkbox' name='SunRiseA' value='checked'" + SunRise[0] + "></p><hr></td></tr>";

  // TIMER B ////
  TempTimerOnReadOnly  = "";                                                   // Initialy, the field is not readonly
  TempTimerOffReadOnly = "";                                                   // Initialy, the field is not readonly
  TempHourTimerOn = String(HourTimerOn[1]);
  TempMinuteTimerOn = String(MinuteTimerOn[1]);
  TempHourTimerOff = String(HourTimerOff[1]);
  TempMinuteTimerOff = String(MinuteTimerOff[1]);
  if (HourTimerOn[1]    < 10) TempHourTimerOn =    "0" + TempHourTimerOn;      // add a '0' when number is under 10
  if (MinuteTimerOn[1]  < 10) TempMinuteTimerOn =  "0" + TempMinuteTimerOn;    // add a '0' when number is under 10
  if (HourTimerOff[1]   < 10) TempHourTimerOff =   "0" + TempHourTimerOff;     // add a '0' when number is under 10
  if (MinuteTimerOff[1] < 10) TempMinuteTimerOff = "0" + TempMinuteTimerOff;   // add a '0' when number is under 10

  // Set fields to readonly if sunset or sunrise is checked
  if (SunSet[1] == "checked")
  {
    TempTimerOnReadOnly = "readonly";
    if (FieldClassOn[1] == "field") FieldClassOn[1] = "fieldnochange";
  }

  if (SunRise[1] == "checked")
  {
    TempTimerOffReadOnly = "readonly";
    if (FieldClassOff[1] == "field") FieldClassOff[1] = "fieldnochange";
  }

  webpage += "<tr><td><h3>Timer B</h3></td>";
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[1] +  "' value='" + TempHourTimerOn    + "' size='4' maxlength='2' name='HourTimerOn[1]'"    + TempTimerOnReadOnly  + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOn[1] +  "' value='" + TempMinuteTimerOn  + "' size='4' maxlength='2' name='MinuteTimerOn[1]'"  + TempTimerOnReadOnly  + ">";
  webpage +=          " S:<input type='checkbox' name='SunSetB' value='checked'" + SunSet[1] + "><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[1] + "' value='" + TempHourTimerOff   + "' size='4' maxlength='2' name='HourTimerOff[1]'"   + TempTimerOffReadOnly + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOff[1] + "' value='" + TempMinuteTimerOff + "' size='4' maxlength='2' name='MinuteTimerOff[1]'" + TempTimerOffReadOnly + ">";
  webpage +=          " R:<input type='checkbox' name='SunRiseB' value='checked'" + SunRise[1] + "></p><hr></td></tr>";

  // TIMER C ////
  TempTimerOnReadOnly  = "";                                                   // Initialy, the field is not readonly
  TempTimerOffReadOnly = "";                                                   // Initialy, the field is not readonly
  TempHourTimerOn = String(HourTimerOn[2]);
  TempMinuteTimerOn = String(MinuteTimerOn[2]);
  TempHourTimerOff = String(HourTimerOff[2]);
  TempMinuteTimerOff = String(MinuteTimerOff[2]);
  if (HourTimerOn[2]    < 10) TempHourTimerOn =    "0" + TempHourTimerOn;      // add a '0' when number is under 10
  if (MinuteTimerOn[2]  < 10) TempMinuteTimerOn =  "0" + TempMinuteTimerOn;    // add a '0' when number is under 10
  if (HourTimerOff[2]   < 10) TempHourTimerOff =   "0" + TempHourTimerOff;     // add a '0' when number is under 10
  if (MinuteTimerOff[2] < 10) TempMinuteTimerOff = "0" + TempMinuteTimerOff;   // add a '0' when number is under 10

  // Set fields to readonly if sunset or sunrise is checked
  if (SunSet[2] == "checked")
  {
    TempTimerOnReadOnly = "readonly";
    if (FieldClassOn[2] == "field") FieldClassOn[2] = "fieldnochange";
  }

  if (SunRise[2] == "checked")
  {
    TempTimerOffReadOnly = "readonly";
    if (FieldClassOff[2] == "field") FieldClassOff[2] = "fieldnochange";
  }

  webpage += "<tr><td><h3>Timer C</h3></td>";
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[2] +  "' value='" + TempHourTimerOn    + "' size='4' maxlength='2' name='HourTimerOn[2]'"    + TempTimerOnReadOnly  + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOn[2] +  "' value='" + TempMinuteTimerOn  + "' size='4' maxlength='2' name='MinuteTimerOn[2]'"  + TempTimerOnReadOnly  + ">";
  webpage +=          " S:<input type='checkbox' name='SunSetC' value='checked'" + SunSet[2] + "><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[2] + "' value='" + TempHourTimerOff   + "' size='4' maxlength='2' name='HourTimerOff[2]'"   + TempTimerOffReadOnly + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOff[2] + "' value='" + TempMinuteTimerOff + "' size='4' maxlength='2' name='MinuteTimerOff[2]'" + TempTimerOffReadOnly + ">";
  webpage +=          " R:<input type='checkbox' name='SunRiseC' value='checked'" + SunRise[2] + "></p><hr></td></tr>";

  // TIMER D ////
  TempTimerOnReadOnly  = "";                                                   // Initialy, the field is not readonly
  TempTimerOffReadOnly = "";                                                   // Initialy, the field is not readonly
  TempHourTimerOn = String(HourTimerOn[3]);
  TempMinuteTimerOn = String(MinuteTimerOn[3]);
  TempHourTimerOff = String(HourTimerOff[3]);
  TempMinuteTimerOff = String(MinuteTimerOff[3]);
  if (HourTimerOn[3]    < 10) TempHourTimerOn =    "0" + TempHourTimerOn;      // add a '0' when number is under 10
  if (MinuteTimerOn[3]  < 10) TempMinuteTimerOn =  "0" + TempMinuteTimerOn;    // add a '0' when number is under 10
  if (HourTimerOff[3]   < 10) TempHourTimerOff =   "0" + TempHourTimerOff;     // add a '0' when number is under 10
  if (MinuteTimerOff[3] < 10) TempMinuteTimerOff = "0" + TempMinuteTimerOff;   // add a '0' when number is under 10

  // Set fields to readonly if sunset or sunrise is checked
  if (SunSet[3] == "checked")
  {
    TempTimerOnReadOnly = "readonly";
    if (FieldClassOn[3] == "field") FieldClassOn[3] = "fieldnochange";
  }

  if (SunRise[3] == "checked")
  {
    TempTimerOffReadOnly = "readonly";
    if (FieldClassOff[3] == "field") FieldClassOff[3] = "fieldnochange";
  }

  webpage += "<tr><td><h3>Timer D</h3></td>";
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[3] +  "' value='" + TempHourTimerOn    + "' size='4' maxlength='2' name='HourTimerOn[3]'"    + TempTimerOnReadOnly  + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOn[3] +  "' value='" + TempMinuteTimerOn  + "' size='4' maxlength='2' name='MinuteTimerOn[3]'"  + TempTimerOnReadOnly  + ">";
  webpage +=          " S:<input type='checkbox' name='SunSetD' value='checked'" + SunSet[3] + "><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[3] + "' value='" + TempHourTimerOff   + "' size='4' maxlength='2' name='HourTimerOff[3]'"   + TempTimerOffReadOnly + ">";
  webpage +=            ":<input type='text' class='" + FieldClassOff[3] + "' value='" + TempMinuteTimerOff + "' size='4' maxlength='2' name='MinuteTimerOff[3]'" + TempTimerOffReadOnly + ">";
  webpage +=          " R:<input type='checkbox' name='SunRiseD' value='checked'" + SunRise[3] + "></p><hr></td></tr>";

  // Webpage Form "Save settings"
  webpage += "<tr><td></td><td><p> &nbsp; &nbsp; &nbsp;<input type='submit' value='Save Settings'></form></p></td></tr></table>";

  webpage += webpagefooter;

  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
}


// wwwroot - (Info) Webpage for displaying the system state
void wwwroot()
{

  // Syncstatus
  if (timeStatus() == timeNotSet) syncstatus = "Time never set";
  if (timeStatus() == timeNeedsSync) syncstatus = "Time set, needs sync";
  if (timeStatus() == timeSet) syncstatus = "Time set, synced";

  // Build the string for the webpage
  webpage = webpageheader;
  webpage += "<p>Time: &nbsp; &nbsp; " + TimerTimeZone.dateTime("H~:i~:s") + "<br>";
  webpage += "Date: &nbsp; &nbsp; " + TimerTimeZone.dateTime("D dS M Y") + "<br>";
  webpage += "Sync: &nbsp; &nbsp; " + syncstatus + "<br>";
  webpage += "Timezone: " + TimerTimeZone.dateTime("e") + "<br>";
  webpage += "Diff.UTC: " + TimerTimeZone.dateTime("P") + "<br>";
  webpage += "Sunrise: &nbsp;" + mySunrise$ + "<br>";
  webpage += "Sunset: &nbsp; " + mySunset$ + "<br>";
  webpage += "DST now: &nbsp;" + String(TimerTimeZone.isDST()) + "<br>";
  webpage += "WiFi: &nbsp; &nbsp; " + WiFi.SSID() + " " + "<br>";
  webpage += "IP: &nbsp; &nbsp; &nbsp; " + WiFi.localIP().toString() + "<br>";
  webpage += "MAC: &nbsp; &nbsp; &nbsp;" + String(WiFi.macAddress()) + "<br>";
  webpage += "Debug: &nbsp; &nbsp;" + String(DebugLevel) + "<br><br>";

  webpage += "<form action='/' method='POST'><input type='hidden' name='go'>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <input type='submit' value='Refresh'></form></p>";

  webpage += webpagefooter;

  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
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
  webpageheader += "p {font-family: Courier; Color: green;}";                                                             // Standard text
  webpageheader += "p.error {font-family: Courier; Color: red;}";                                                         // Errormessages
  webpageheader += ".field {font-family: Courier; Background-color: #FFFF78; Color: black; text-align: center;}";         // Yellow background in fields
  webpageheader += ".fieldsaved {font-family: Courier; Background-color: #78FF78; Color: black; text-align: center;}";    // Green background in fields of the Timerpage after Save Settings
  webpageheader += ".fielderror {font-family: Courier; Background-color: #FF7878; Color: black; text-align: center;}";    // Red background in fields of the Timerpage after Error
  webpageheader += ".fieldnochange {font-family: Courier; Background-color: #BBBBBB; Color: black; text-align: center;}"; // Grey background in fields of the Timerpage that can't be changed
  webpageheader += "</style></head><body>";

  // Start of content with menu
  webpageheader += "<h1>TimerClock " + version$ + "</h1>";
  webpageheader += "<p>Menu: <a href='/timers'>Timers</a> &nbsp;&nbsp; <a href='/controls'>Controls</a><br><br>";
  webpageheader += " &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <a href='/'>Info</a> &nbsp;&nbsp; <a href='/command'>Command</a></p><hr>";

  // Make standard footer used for every webpage
  webpagefooter = "<br><hr>";
  webpagefooter += "<i>Made for ESP8266<br>John Halfords, the Netherlands</i>";
  webpagefooter += "</body></html>";

}


///////////////////// Subroutines for timercontrol ///////////////////////

// Subroutine to switch ON or OFF timer (Argument is timer_x_on or timer_x_off, where x=a-d)
void TimerOnOff(int x)
{
  for (int count = 0; count < TimesGPIO; count++)  // Loop: How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
  {
    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.print("Sending HIGH-LOW to port: ");
    if (DebugLevel >= 1) Serial.println(x);

    digitalWrite(x, HIGH);
    delay(GPIOhightime);
    digitalWrite(x, LOW);
    delay(GPIOdelay);
  }
}

// Subroutine to switch OFF all timers
void TimerAllOff()
{
  for (int count = 0; count < TimesGPIO; count++)  // Loop: How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
  {
    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.println("Sending: All Timers OFF");

    digitalWrite(timer_a_off, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_a_off, LOW);
    delay(200);
    digitalWrite(timer_b_off, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_b_off, LOW);
    delay(200);
    digitalWrite(timer_c_off, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_c_off, LOW);
    delay(200);
    digitalWrite(timer_d_off, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_d_off, LOW);
    delay(200);
  }
}

// Subroutine to switch ON all timers
void TimerAllOn()
{
  for (int count = 0; count < TimesGPIO; count++)  // Loop: How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
  {
    if (DebugLevel == 2) Serial.print(TimerTimeZone.dateTime("d~-m~-Y H~:i~:s ~| "));
    if (DebugLevel >= 1) Serial.println("Sending: All Timers ON");

    digitalWrite(timer_a_on, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_a_on, LOW);
    delay(200);
    digitalWrite(timer_b_on, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_b_on, LOW);
    delay(200);
    digitalWrite(timer_c_on, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_c_on, LOW);
    delay(200);
    digitalWrite(timer_d_on, HIGH);
    delay(GPIOhightime);
    digitalWrite(timer_d_on, LOW);
    delay(200);
  }
}

///////////////////  End of Subroutines  //////////////////////


