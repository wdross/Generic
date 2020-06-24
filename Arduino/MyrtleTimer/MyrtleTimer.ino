// Developed with ArduinoIDE v1.6.12
// Tools->Board: Adafruit HUZZAH ESP8266
//      ->Flash Size: 4M (3M SPIFFS)

// Relies on the ESP8266 "Built-In" package for HTTPUpload updating

// Items yet to be done in this project:
// - 2nd relay for light
//   - needs scheduling
//   - EDT/EST behaviors; EEPROM
// - web page defines default "off" time when asked to "nap"
//   - store that value in EEPROM
// - get it to respond/work with Alexa

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <CFwTimer.h>
#include <NTPClient.h> // get us internet time
#include <CFwDebouncedDigitalInput.h> // local pushbutton to turn off pump
#include <../Credentials.h>

#define HOST "myrtle"     // what name to show via DNS?

#define VERSION "0.55prototype"

ESP8266WebServer server(80);

const int PUMP_RELAY_OUTPUT = 12; // #0 is the RED LED on the breakout board
const bool PUMP_OUTPUT_STATE_FOR_ON = false; // when we pull this output low the relay is activated
bool last_pump_state = true; // pump water when turned on!
int QuietTimeMS = 20*60*1000L; // Default to 20 minutes
CFwTimer QuietTimer; // how much time is left quiet before we turn the pump back on

const int LIGHT_RELAY_OUTPUT = 14;
const bool LIGHT_OUTPUT_STATE_FOR_ON = false;
bool last_light_state = false; // leave off until we get the time and know which way it should be
#define TIME_OF_DAY_ON   8*3600L // 8am
#define TIME_OF_DAY_OFF 21*3600L // 9pm


#define ACTIVITY_LED 2 // blue LED near the antenna end of the Feather
bool lastLED = false;
CFwTimer Tick(500);

#define LOCAL_PUMP_OFF_BTN 0 // #0 is the GPIO0 INPUT as well as the RED LED output
CFwDebouncedDigitalInput *AskForQuiet;

bool loggedIn = false; // has someone logged in and provided the correct ID/pw?
CFwTimer LoggedInTimer(15*60*1000L); // how long without activity before we kick them out?

// Variables required for NTPClient (Network Time Protocol)
WiFiUDP ntpUDP;
#define TIME_OFFSET -4*3600L // EDT is "-4"; EST is "-5": can we figure out how to automatically switch between those times?!?!
NTPClient timeClient(ntpUDP);

#define RED String("d00")
#define GREEN String("0d0")
String getStyle(int FormMaxWidth=258) // can override default
{
  String out = "<style>"
         "#setinp{width:22%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
         "#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
         "#setinp,input{background:#f1f1f1;border:0;padding:0 15px}"
         "#call{background:#";
  if (last_pump_state)
    out += GREEN;
  else
    out += RED;
  out += ";color:#000}"
         "body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
         "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
         "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
         "form{background:#fff;max-width:";
  out += String(FormMaxWidth);
  out += "px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
         ".btn{background:#3498db;color:#fff;cursor:pointer}"
         "</style>";
  return out;
}

/* Login page */
String loginIndex =
"<form name=loginForm ACTION='serverIndex'>"
"<h1>Ross " HOST
" Login</h1>"
"<p>Version " VERSION
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit class=btn value=Login></form>" + getStyle();

/* Server Index Page */
String serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + getStyle();


void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void){
  pinMode(PUMP_RELAY_OUTPUT, OUTPUT);
  last_pump_state = false; // always start "off"
  digitalWrite(PUMP_RELAY_OUTPUT, last_pump_state == PUMP_OUTPUT_STATE_FOR_ON);

  pinMode(LIGHT_RELAY_OUTPUT, OUTPUT);
  last_light_state = false; // start with light off
  digitalWrite(LIGHT_RELAY_OUTPUT, last_light_state == LIGHT_OUTPUT_STATE_FOR_ON);

  pinMode(ACTIVITY_LED,OUTPUT);
  digitalWrite(ACTIVITY_LED,lastLED);

  // offer a local button to initiate the timer
  AskForQuiet = new CFwDebouncedDigitalInput(LOCAL_PUMP_OFF_BTN,true); // invert the input

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Connect
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");

  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  if (MDNS.begin(HOST)) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handlePump);
  server.on("/nap", delayPump);
  server.on("/firmwareupdate", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    String usr, pw;
    server.sendHeader("Connection", "close");

    for (uint8_t i=0; i<server.args(); i++){
      if (server.argName(i) == "userid")
        usr = server.arg(i);
      else if (server.argName(i) == "pwd")
        pw = server.arg(i);
    }
    if (usr == USERID && pw == PASSWORD) {
      loggedIn = true;
      LoggedInTimer.ResetTimer();
      server.send(200, "text/html", serverIndex);
    }
    else
      server.send(200, "text/html", loginIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    if (loggedIn)
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    else
      server.send(200, "text/html", loginIndex);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  timeClient.begin();
  timeClient.setUpdateInterval(24*3600L*1000); // just sync 1 time per day (in ms)
  timeClient.setTimeOffset(TIME_OFFSET); // And, need to figure out how to present this on the Web page as a changable item
  Serial.println("NTPClient started");
}

String DOW[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

void handlePump() {
  char temp[20];
  String out = "<HTML><FORM NAME='form' METHOD='POST' ACTION='changeofftime'>\n";
  out += "<h1>Myrtle Control Center</h1>\n";
  out += "<p id='call'><br>";
  if (last_pump_state)
    out += "Currently pumping water";
  else {
    out += "NOT pumping water for ";
    sprintf(temp,"%d",QuietTimer.GetRemaining()/1000);
    out += temp;
    out += " seconds";
  }
  out += "<br>&nbsp</p>\nOff time: <INPUT id='setinp' TYPE='TEXT' NAME='timeoff' value='";
  sprintf(temp,"%d",QuietTimeMS/1000/60); // convert our MS to Minutes
  out += temp;
  out += "'> minutes<br>\n";

  out += "<br>&nbsp</p><a href='nap'>Have some quiet time!</a><br>&nbsp</p>";

  out += "<INPUT TYPE='SUBMIT' NAME='submit' VALUE='Setup'>\n";
  out += "<br>" + DOW[timeClient.getDay()] + " " + timeClient.getFormattedTime();
  out += "<br>" + timeClient.TimeJump;
  out += "<br>Version " VERSION;
  out += "<br><a href='firmwareupdate'>Upload new firmware</a>";
  out += "</FORM></HTML>\n" + getStyle();
  server.send(200, "text/html", out);
}


void delayPump() {
  last_pump_state = false; // turn's off the pump, which is actually turning ON the relay
  digitalWrite(PUMP_RELAY_OUTPUT, last_pump_state == PUMP_OUTPUT_STATE_FOR_ON);
  QuietTimer.SetTimer(QuietTimeMS);
  handlePump(); // then present the same page
}

// ESPhttpUpdate.updateSpiffs() is a call to "go see" if an updated binary is available for us to download

void loop(void){
  server.handleClient();

  timeClient.update(); // it will (1 time per day) go check with the time server for the time, preventing clock walk

  if (AskForQuiet->Process()) {
    // state just changed on the input
    if (AskForQuiet->GetState()) {
      // Button Pressed, take that nap
      delayPump(); // started via Web or GPIO
    }
  }
  if (QuietTimer.IsTimeout()) {
    last_pump_state = true;
    digitalWrite(PUMP_RELAY_OUTPUT, last_pump_state == PUMP_OUTPUT_STATE_FOR_ON);
    QuietTimer.ResetTimer(); // will cause this to run again in the defined period, but prevents the 12.4 day timer-wrap problem
  }

  if (timeClient.timeKnown()) {
    if (TIME_OF_DAY_ON < TIME_OF_DAY_OFF) {
      // "normal", time ON is before time OFF
      last_light_state = (timeClient.getSecondsOfDay() >= TIME_OF_DAY_ON &&
                          timeClient.getSecondsOfDay() < TIME_OF_DAY_OFF);
    }
    else {
      // "upside down": Time on is basically overnight
      last_light_state = (timeClient.getSecondsOfDay() >= TIME_OF_DAY_ON ||
                          timeClient.getSecondsOfDay() < TIME_OF_DAY_OFF);
    }
  }
  digitalWrite(LIGHT_RELAY_OUTPUT, last_light_state == LIGHT_OUTPUT_STATE_FOR_ON);

  if (Tick.IsTimeout()) {
    lastLED = !lastLED;
    digitalWrite(ACTIVITY_LED,lastLED);
    Tick.IncrementTimer(500/(loggedIn?4:1)); // blink faster when someone has logged in
  }

  if (LoggedInTimer.IsTimeout()) {
    // enough time has elapsed without action that we revert to needing a new sign-in
    loggedIn = false;
    LoggedInTimer.ResetTimer(); // will cause this to run again in the defined period, but prevents the 12.4 day timer-wrap problem
  }
}
