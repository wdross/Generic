// Developed with ArduinoIDE v1.6.12
// Tools->Board: Adafruit HUZZAH ESP8266
//      ->Flash Size: 4M (3M SPIFFS)

// Relies on the ESP8266 "Built-In" package for Web based updating and OTA updating

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <../Credentials.h>

#define HOST "thermostat"     // what name to show via DNS

#define VERSION "0.55prototype"

ESP8266WebServer server(80);

const int HEAT_RELAY = 13;
bool last_heat_state = false;


/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex =
"<form name=loginForm>"
"<h1>Ross " HOST
" Login</h1>"
"<p>Version " VERSION
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='admin')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;

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
"</script>" + style;

void drawGraph() {
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "Some plain text here\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x+= 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"2\" />\n", x, 140 - y, x + 10, 140 - y2);
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
  pinMode(HEAT_RELAY, OUTPUT);
  last_heat_state = false; // always start "off"
  digitalWrite(HEAT_RELAY, last_heat_state);
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
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
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

  ArduinoOTA.setHostname(HOST);
//  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
//                        for(int i=0; i<N_DIMMERS;i++)
//                          analogWrite(dimmer_pin[i], 0);
//                          analogWrite(led_pin,0);
//                    });
//
//  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
//                          for (int i=0;i<30;i++)
//                          {
//                            analogWrite(led_pin,(i*100) % 1001);
//                            delay(50);
//                          }
//                        });
  ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); }); // just reboot ourselves upon failed update
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA server started");
}

void handleTemp() {
  last_heat_state = !last_heat_state;
  digitalWrite(HEAT_RELAY, last_heat_state);

  char temp[20];
  String out = "<HTML><FORM NAME=\"form\" METHOD=\"POST\" ACTION=\"changetemperature\">\n";
  out += "<h1>Awesome thermostat!</h1>\n";
  out += "Current temperature is:\n";
  sprintf(temp, "%d&deg F\n",65 + (rand() % 13));
  out += temp;
  if (last_heat_state)
    out += "<br>Currently calling for heat";
  else
    out += "<br>NOT currently asking for heat";
  out += "<br>Setpoint: <INPUT TYPE=\"TEXT\" NAME=\"setpoint\" SIZE=\"2\" value=\"72\"> F<br>\n";
  out += "<INPUT TYPE=\"SUBMIT\" NAME=\"submit\" VALUE=\"Change setpoint\">\n";
  out += "<br><IMG src=\"test.svg\">";
  out += "<br>Version " VERSION;
  out += "<br><a href=\"firmwareupdate\">Upload new firmware";
  out += "</FORM></HTML>\n";
  server.send(200, "text/html", out);
}


// ESPhttpUpdate.updateSpiffs() is a call to "go see" if an updated binary is available for us to download
// ArduinoOTA.* sets up a server which can "push" updates to us (with authorization checks)
//   needs a call to ArduinoOTA.handle() in the loop() routine

void loop(void){
  server.handleClient();
  ArduinoOTA.handle();
}
