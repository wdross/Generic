// Developed with ArduinoIDE v1.6.12
// Tools->Board: Adafruit HUZZAH ESP8266
//      ->Flash Size: 4M (3M SPIFFS)

// Relies on the ESP8266 "Built-In" package for Web based updating and HTTPUpload updating

// Items yet to be done in this project:
// - Correct the HEAT_RELAY_OUTPUT to drive the new feather relay output
// - Collect temperature via TMP36 analog input
// - Determine a temperature update rate
// - Hysteresis settings (over/under)
// - Save set points/configuration to EEPROM
//   o Library: https://github.com/esp8266/Arduino/tree/master/libraries/EEPROM
// - Introduce a "Fan on" mode (with scheduling?!) w/other feather relay output
// - Temperature history to fill out the placeholder graph we display
//   o Add a second line for set point (it changes over time too based on schedule)

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <CFwTimer.h>
#include <NTPClient.h> // get us internet time
#include <../Credentials.h>

#define HOST "thermostat"     // what name to show via DNS

#define VERSION "0.55prototype"

ESP8266WebServer server(80);

const int HEAT_RELAY_OUTPUT = 13;
const bool HEAT_OUTPUT_STATE_FOR_ON = false; // when we pull this output low the relay is activated
bool last_heat_state = false;

#define ACTIVITY_LED 2 // blue LED near the antenna end of the Feather
bool lastLED = false;
CFwTimer Tick(500);

bool loggedIn = false; // has someone logged in and provided the correct ID/pw?
CFwTimer LoggedInTimer(15*60*1000L); // how long without activity before we kick them out?

// Variables required for NTPClient (Network Time Protocol)
WiFiUDP ntpUDP;
#define TIME_OFFSET -4*3600L // EDT is "-4"; EST is "-5": can we figure out how to automatically switch between those times?!?!
NTPClient timeClient(ntpUDP);

#define GRAPH_WIDTH 400 // pixel width of the history graph we draw
#define RED String("d00")
#define GREEN String("0d0")
String getStyle(int FormMaxWidth=258) // can override default
{
  String out = "<style>"
         "#setinp{width:15%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
         "#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
         "#setinp,input{background:#f1f1f1;border:0;padding:0 15px}"
         "#call{background:#";
  if (last_heat_state)
    out += RED;
  else
    out += GREEN;
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


void drawGraph() {
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" + String(GRAPH_WIDTH) + "\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x+= 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send ( 200, "image/svg+xml", out);
}

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
  pinMode(HEAT_RELAY_OUTPUT, OUTPUT);
  last_heat_state = false; // always start "off"
  digitalWrite(HEAT_RELAY_OUTPUT, last_heat_state == HEAT_OUTPUT_STATE_FOR_ON);

  pinMode(ACTIVITY_LED,OUTPUT);
  digitalWrite(ACTIVITY_LED,lastLED);

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

  server.on("/", handleTemp);
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

  server.on ( "/test.svg", drawGraph );

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  timeClient.begin();
  timeClient.setUpdateInterval(24*3600L*1000); // just sync 1 time per day (in ms)
  timeClient.setTimeOffset(TIME_OFFSET); // And, need to figure out how to present this on the Web page as a changable item
  Serial.println("NTPClient started");
}

String DOW[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

void handleTemp() {
  last_heat_state = !last_heat_state;
  digitalWrite(HEAT_RELAY_OUTPUT, last_heat_state == HEAT_OUTPUT_STATE_FOR_ON);

  char temp[20];
  String out = "<HTML><FORM NAME='form' METHOD='POST' ACTION='changetemperature'>\n";
  out += "<h1>Awesome thermostat!</h1>\n";
  out += "Current temperature is:\n";
  sprintf(temp, "%d&deg F\n",65 + (rand() % 13));
  out += temp;
  out += "<p id='call'><br>";
  if (last_heat_state)
    out += "Currently calling for heat";
  else
    out += "NOT currently asking for heat";
  out += "<br>&nbsp</p>\nSetpoint: <INPUT id='setinp' TYPE='TEXT' NAME='setpoint' value='72'> F<br>\n";
  out += "<INPUT TYPE='SUBMIT' NAME='submit' VALUE='Change setpoint'>\n";
  out += "<br><IMG src='test.svg'>";
  out += "<br>" + DOW[timeClient.getDay()] + " " + timeClient.getFormattedTime();
  out += "<br>" + timeClient.TimeJump;
  out += "<br>Version " VERSION;
  out += "<br><a href='firmwareupdate'>Upload new firmware";
  out += "</FORM></HTML>\n" + getStyle(GRAPH_WIDTH);
  server.send(200, "text/html", out);
}


// ESPhttpUpdate.updateSpiffs() is a call to "go see" if an updated binary is available for us to download

void loop(void){
  server.handleClient();

  timeClient.update(); // it will (1 time per day) go check with the time server for the time, preventing clock walk

  if (Tick.IsTimeout()) {
    lastLED = !lastLED;
    digitalWrite(ACTIVITY_LED,lastLED);
    Tick.IncrementTimer(loggedIn?250:500); // blink faster when someone has logged in
  }

  if (LoggedInTimer.IsTimeout()) {
    // enough time has elapsed without action that we revert to needing a new sign-in
    loggedIn = false;
    LoggedInTimer.ResetTimer(); // will cause this to run again in the defined period, but prevents the 12.4 day timer-wrap problem
  }
}
