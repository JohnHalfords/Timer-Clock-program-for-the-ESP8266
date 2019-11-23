//                                             - - o o - -                                       //
//                                          22nd November 2019                                   //
//// //// ////     ESP8266 Project Timerclock by John Halfords, The Netherlands      //// //// ////
//// //// ////        Comments and questions welcome at: halfordsj@gmail.com         //// //// ////
//                                                                                               //
//                                       Please read Readme.md                                   //
//                                             - - o o - -                                       //

// What's new in 2.1:
// - Offset from Sunrise / Sunset
// - Reorganised code for html Timerspage
// - History page

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

// User declarations ##############################################################################################################
#include "UserDeclarations.h"          // Wifi settings, Timezone, Location, DebugLevels and pinning can be changed to your design

// Program declarations // DON'T change! Change file "UserDeclarations.h" if your design if different from mine
String version$ = "v2.1";                                     // Version of program
String webpageheader, webpagefooter, webpage;                 // Strings for building the webpages
String syncstatus;                                            // String that holds the status of the timesync
unsigned int mySunrise, mySunset;                             // Integer that holds the numerical value of todays Sunrise and Sunset (in minutes from midnight)
String mySunrise$, mySunset$;                                 // String that holds the text value of todays Sunrise and Sunset HH:MM
String SunSet[4], SunRise[4];                                 // String array, element is "checked" when Sunrise/set time is active,  "" (unchecked) when user timer time is set
int SunBits;                                                  // 8 bits that hold the settings of the checkboxes SunSet and SunRise
unsigned int HourTimerOn[4], HourTimerOff[4];                 // Variable arrays for Timers 1-4
unsigned int MinuteTimerOn[4], MinuteTimerOff[4];             // Variable arrays for Timers 1-4
unsigned int MinuteTimerOnOffset[4], MinuteTimerOffOffset[4]; // Variable arrays for Timers 1-4 Offset in case of using Sunset or Sunrise

String HistoryMsg[20];                                        // String array for the last 20 messages for the History page
String FieldClassOn[4], FieldClassOff[4];                     // String for the html-class (yellow, grey or green color) for fields on the Timerpage
String FieldClassOnOffset[4], FieldClassOffOffset[4];         // String for the html-class (yellow, grey or green color) for Offset fields on the Timerpage
String TempHourTimerOn, TempMinuteTimerOn, TempHourTimerOff;                  // Make temporary strings for functionality in HTML forms
String TempMinuteTimerOff, TempMinuteTimerOnOffset, TempMinuteTimerOffOffset; // Make temporary strings for functionality in HTML forms
String TempTimerOnReadOnly, TempTimerOffReadOnly;                             // Make temporary strings for functionality in HTML forms

Timezone TimerTimeZone;                           // Create an instance for ezTime, Timer's timezone

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

  StartWiFi();                                  // Subroutine that starts WiFi connection to the stongest SSID given

  // Handle the Pages
  WebServer.on("/", wwwroot);                   // is also the 'info-page'
  WebServer.on("/controls", controlspage);      // page to directly control the timers
  WebServer.on("/command", commandpage);        // page for reset/restart and update sketch via the website (through http)
  WebServer.on("/timers", timerspage);          // page to set the timer times and check SunSet/Rise
  WebServer.on("/history", history);            // page for displaying the last 20 messages
  WebServer.onNotFound(PageNotFound);           // when a page is requested that doesn't exist

  WebServer.begin();                            // Start the webserver
  WebUpdater.setup(&WebServer);                 // Start the WebUpdater as part of the WebServer

  TimerTimeZone.setLocation(myTimeZone);        // Set timezone for ezTime to the setting you gave in UserDeclarations.h
  waitForSync();                                // Wait for time to sync (ezTime)

  MakeHeaderFooter();                           // Make standard header, Title, Menu and footer used for every webpage
  ReadTimersEEprom();                           // Subroutine that reads Timer values from EEPROM
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
    if (DebugLevel >= 1) Serial.println("Set Sunrise and Sunset for today. Sunrise = " + mySunrise$ + " and Sunset = " + mySunset$);
    if (HistoryPage == 1) MakeHistory("Set Sunrise/set for today. " + mySunrise$ + " and " + mySunset$);

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
        if (count == 0) TimerOnOff(timer_a_on);
        if (count == 1) TimerOnOff(timer_b_on);
        if (count == 2) TimerOnOff(timer_c_on);
        if (count == 3) TimerOnOff(timer_d_on);
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
        if (count == 0) TimerOnOff(timer_a_off);
        if (count == 1) TimerOnOff(timer_b_off);
        if (count == 2) TimerOnOff(timer_c_off);
        if (count == 3) TimerOnOff(timer_d_off);
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


// Subroutine that reads Timer values from EEPROM
void  ReadTimersEEprom()
{
  EEPROM.begin(25);
  // Read the timer on and off settings
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
  EEPROM.begin(25);
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
  // Read the Offset in case of using Sunset or Sunrise
  EEPROM.write(17, MinuteTimerOnOffset[0]);
  EEPROM.write(18, MinuteTimerOffOffset[0]);
  EEPROM.write(19, MinuteTimerOnOffset[1]);
  EEPROM.write(20, MinuteTimerOffOffset[1]);
  EEPROM.write(21, MinuteTimerOnOffset[2]);
  EEPROM.write(22, MinuteTimerOffOffset[2]);
  EEPROM.write(23, MinuteTimerOnOffset[3]);
  EEPROM.write(24, MinuteTimerOffOffset[3]);
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
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_d_off'><input type = 'submit' value = 'OFF'></form></td></tr> ";
  webpage += "<tr><td><h3>Timer A - D </h3></td> <td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_all_on'><input type = 'submit' value = 'ON'></form></td> ";
  webpage += "<td><form action = '/controls' method = 'POST'><input type = 'hidden' name = 'go' value = 'timer_all_off'><input type = 'submit' value = 'OFF'></form> </td> </tr> </table> ";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server

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
  webpage += "<h3>Commands: </h3> ";
  // To produce server arguments i made hidden fields with a standard value and a visible submit button. The fields are questioned in the if-loop further on.
  webpage += "<table style = 'width:275px'><tr><td><form action = '/command' method = 'POST'>&nbsp; &nbsp; &nbsp; <input type = 'hidden' name = 'go' value = 'reset'><input type = 'submit' value = 'Reset ESP'></form></td> ";
  webpage += "<td><form action = '/command' method = 'POST'><input type = 'hidden' name = 'go' value = 'restart'><input type = 'submit' value = 'Restart ESP'></form></td></tr></table> ";

  webpage += "<h3>WebUpdater: </h3>";
  webpage += "<iframe src = '/update' height = '45' width = '350'></iframe ><br>";

  webpage += webpagefooter;

  WebServer.send(200, "text / html", webpage); // Send the webpage to the server

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
      if (HourTimerOn[count]  > 23 || MinuteTimerOn[count]  > 59) FieldClassOn[0]  = "fielderror";
      if (HourTimerOff[count] > 23 || MinuteTimerOff[count] > 59) FieldClassOff[0] = "fielderror";
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
      SaveTimersEEprom();       // Subroutine that saves Timer values to EEPROM
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
  // Yes you see a lot of repetative lines, no i couldn't make a for loop. If you manage let me know, please!
  // The arrays that are inside the html don't work in a loop with array variables

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
  webpage += "<td><p>ON : <input type='text' class='" + FieldClassOn[count] +  "' value='" + TempHourTimerOn    + "' size='4' maxlength='2' name='HourTimerOn["   + String(count) + "]'"    + TempTimerOnReadOnly  + " style='width: 35px;'>";
  webpage +=     ":&#8201;<input type='text' class='" + FieldClassOn[count] +  "' value='" + TempMinuteTimerOn  + "' size='4' maxlength='2' name='MinuteTimerOn[" + String(count) + "]'"   + TempTimerOnReadOnly  + " style='width: 35px;'>";
  webpage +=          " S:<input type='checkbox' name='SunSet[" + String(count) + "]' value='checked'" + SunSet[count] + "><br>";
  if (SunSet[count] == "checked") webpage += "Offset &nbsp; -<input type='text' class='" + FieldClassOnOffset[count] +  "' value='" + TempMinuteTimerOnOffset  + "' size='4' maxlength='2' name='MinuteTimerOnOffset[" + String(count) + "]' style='width: 35px;'><br>";
  webpage +=        "OFF: <input type='text' class='" + FieldClassOff[count] + "' value='" + TempHourTimerOff   + "' size='4' maxlength='2' name='HourTimerOff[" + String(count) + "]'"   + TempTimerOffReadOnly + " style='width: 35px;'>";
  webpage +=     ":&#8201;<input type='text' class='" + FieldClassOff[count] + "' value='" + TempMinuteTimerOff + "' size='4' maxlength='2' name='MinuteTimerOff[" + String(count) + "]'" + TempTimerOffReadOnly + " style='width: 35px;'>";
  webpage +=          " R:<input type='checkbox' name='SunRise[" + String(count) + "]' value='checked'" + SunRise[count] + "><br>";
  if (SunRise[count] == "checked") webpage += "Offset &nbsp; +<input type='text' class='" + FieldClassOffOffset[count] + "' value='" + TempMinuteTimerOffOffset  + "' size='4' maxlength='2' name='MinuteTimerOffOffset[" + String(count) + "]' style='width: 35px;'><br>";
  webpage += "</p><hr></td></tr>";

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
  webpageheader += ".field {font-family: Courier; Background-color: #FFFF78; Color: black; text-align: center;}";         // Yellow background in fields
  webpageheader += ".fieldsaved {font-family: Courier; Background-color: #78FF78; Color: black; text-align: center;}";    // Green background in fields of the Timerpage after Save Settings
  webpageheader += ".fielderror {font-family: Courier; Background-color: #FF7878; Color: black; text-align: center;}";    // Red background in fields of the Timerpage after Error
  webpageheader += ".fieldnochange {font-family: Courier; Background-color: #BBBBBB; Color: black; text-align: center;}"; // Grey background in fields of the Timerpage that can't be changed
  webpageheader += "</style></head><body>";

  // Start of content with menu
  webpageheader += "<h1>TimerClock " + version$ + "</h1>";
  webpageheader += "<p>Menu: &nbsp; <a href='/timers'>Timers</a> &nbsp; <a href='/controls'>Controls</a><br><br>";
  if (HistoryPage == 0) webpageheader += " &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <a href='/'>Info</a> &nbsp; <a href='/command'>Command</a></p><hr>";
  if (HistoryPage == 1) webpageheader += " <a href='/history'>History</a> &nbsp; <a href='/'>Info</a> &nbsp; <a href='/command'>Command</a></p><hr>";

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

  webpage += "<form action='/history' method='POST'><input type='hidden' name='go'>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; <input type='submit' value='Refresh'></form></p>";
  webpage += webpagefooter;
  WebServer.send(200, "text/html", webpage); // Send the webpage to the server
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

///////////////////  End of Subroutines  //////////////////////


