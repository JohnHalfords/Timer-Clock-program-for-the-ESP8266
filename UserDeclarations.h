//                                             - - o o - -                                       //
//                                         This file belongs to:                                 //
//// //// ////     ESP8266 Project Timerclock by John Halfords, The Netherlands      //// //// ////
//                                             - - o o - -                                       //

// User declarations /////////////////////////////////////////////////////////////// User declarations /////////////////////////////////////////////////////////
//
String WifiSSID1  = "YourSSID1";              // Fill in the name (SSID) of Wifi Network 1   <- You have to fill in at least 1
String WifiPassw1 = "difficultpassword1234";  // Fill in the password of Wifi Network SSID 1 <- You have to fill in at least 1
String WifiSSID2  = "YourSSID2";              // ... and for Wifi 2  .
String WifiPassw2 = "difficultpassword1234";  // ... and for Wifi 2   .
String WifiSSID3  = "";                       // ... and for Wifi 3    . <- If you don't use 2, 3 OR 4: don't delete the lines but keep the string empty: ""
String WifiPassw3 = "";                       // ... and for Wifi 3   .
String WifiSSID4  = "";                       // ... and for Wifi 4  .
String WifiPassw4 = "";                       // ... and for Wifi 4 .
//                                            //
String myTimeZone = "Europe/Amsterdam";       // Set your timezone (countrycodes; https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)
const float myLatitude = 52.000000;           // Latitude must be in decimal degrees (DD), e.g. 52.000000
const float myLongitude =  5.000000;          // Longitude must be in decimal degrees (DD), e.g. 5.000000
//                                            // Tip: Click your location in Google Maps, wait, click again (no double-click) to see your values
//
const int TimesGPIO = 2;                      // How many times the GPIO has to send HIGH-LOW to the corresponding GPIO when the timers are addressed
//                                            //    (my remote is "fire-and-forget" so i set this value to 2) Value minimum = 1, no max
const int GPIOdelay = 500;                    // See previous, time in milliseconds to wait for sending the next HIGH-LOW to the same GPIO
const int GPIOhightime = 500;                 // Time in milliseconds to wait between sending the HIGH and LOW to the GPIO, so actually 'how long is the GPIO HIGH'
const int DebugLevel = 2;                     // 0 = NO Debugmessages, 1 = Debugmessages, 2 = D-M-YYYY HH:MM:SS (local time) + Debugmessages to com-port (baud = myBaudrate)
const int myBaudrate = 19200;                 // Baudrate you prefer for serial communication. Possible values: 300/1200/2400/4800/9600/19200/38400/57600/74880/115200/230400/250000/500000/1000000/2000000
//                                            //    Set your serial monitor accordingly
unsigned int wifiindicator = 2;               // Friendly name for GPIO2  (=D4) .     // the wifiindicator blinks during connecting and is ON while connected
unsigned int timer_a_on = 15;                 // Friendly name for GPIO15 (=D8)  .
unsigned int timer_a_off = 13;                // Friendly name for GPIO13 (=D7)   .
unsigned int timer_b_on = 12;                 // Friendly name for GPIO12 (=D6)    .
unsigned int timer_b_off = 14;                // Friendly name for GPIO14 (=D5)     . <- Change these to your hardware design if neccessary
unsigned int timer_c_on = 0;                  // Friendly name for GPIO0  (=D3)    .
unsigned int timer_c_off = 4;                 // Friendly name for GPIO4  (=D2)   .
unsigned int timer_d_on = 5;                  // Friendly name for GPIO5  (=D1)  .
unsigned int timer_d_off = 16;                // Friendly name for GPIO16 (=D0) .
//
// End of User declarations //////////////////////////////////////////////////////// End of user declarations //////////////////////////////////////////////////
