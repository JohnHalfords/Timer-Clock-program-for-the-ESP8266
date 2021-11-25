//                                             - - o o - -                                       //
//                                         This file belongs to:                                 //
//// //// ////     ESP8266 Project Timerclock by John Halfords, The Netherlands      //// //// ////
//                                             - - o o - -                                       //

# TimerClock_with_SunSet-SunRise
VERSION 3.1, Date: 18 November 2021

!!! Attention:
- Code written for ESP8266 (Core 2.7.4)
- Dusk2Dawn.h library download zip from Github! Don't install with Arduino IDE! (https://github.com/dmkishi/Dusk2Dawn)

Timer Clock program for the ESP8266.
4 timers with ON and OFF time or automatically switch ON and/or OFF by SunSet and/or SunRise with Offset.
The timers control each a user definable GPIO and a http-command.

Hardware: ESP8266 (I have used the Wemos NodeMcu V3 Lolin).

My hardware design is described in document: "TimerClock Project (ESP8266) Schematic.xlsx"
My Arduino IDE settings are in an screenshot called: "Arduino IDE setting.png"


 Functionality:
 ==============
 - Webserver at http://ip-adres/
 - Extended settings page where you can set Timezone, Longitude, Latitude and several GPIO controls
 - Update your sketch bij http (settings page)
 - Remote reset and restart (settings page)
 - The program controls 4 outputs, which are in my hardware design connected to an RF remote control,
   which controls my ligths in my house (controls page)
 - Every timer statuschange (ON or OFF) can also fire a http-command, you can use it for example for webhooks or Tasmota
 - On the website you can control the outputs manually (in my case: switch the ligths on/off)
 - On the website you can configure 4 timers (Timer a - d) (timers page)
   Every timer has it's own ON-time and OFF-time
   Per time you can choose if you set the time manually or set it to SunSet or SunRise (with up to 99 minutes offset)
   
 Used Libraries:
 ===============
 - ESP8266WiFiMulti.h          library to connect to WiFi and automaticly choose the strongest network
 - ESP8266WebServer.h          library to run the WebServer
 - ESP8266HTTPUpdateServer.h   library for updating sketch through http
 - EEPROM.h                    library to write to, and read from EEPROM
 - ezTime.h                    Very well documented Time libary with everything you possibly want! (https://github.com/ropg/ezTime)
 - Dusk2Dawn.h                 Library to define Sunset and Sunrise (https://github.com/dmkishi/Dusk2Dawn)
 - ESP8266HTTPClient.h         Library to run as a HTTP cliënt (to send the http-commands)

 
 Final Note, about me:
 =====================
 - I am not a programmer.
 - I do a lot of trial and error, googling and look at other people's code. That's how i learn.
 - I have tried to document the code as highly and clearly as possible.
 - I am Dutch, so my English is not flawless...
 - I really like to receive your comments or questions, thanks in advance.
 
 GrTx, John
