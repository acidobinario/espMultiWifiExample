#include <Arduino.h>
// wifi and client
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// Arduino_Json and FileSistem
#include <Arduino_JSON.h>
#include <FS.h>

// WIfi Manager!
#include <DNSServer.h> //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>

#define myServer "http://myserver.com/resource"

#define CustomSsid "mySSID"
#define CustomPassword "myPSK"

unsigned long currentTime = 0; // to store the current millis 
unsigned long internetQualityMillis = 0; // to store the millis from 
unsigned long previousRequestMillis = 0;
int httpErrorCount = 0;

// booleans to know when the time of the test has completed
bool firstWiFiPassed = false;
bool secondWiFiPassed = false; 

// booleans to store the final network quality
bool firstWiFiOk = false;
bool secondWiFiOk = false;

bool checkInternetQuality = false;

bool sendRequest(){ //example of the send Request Function
  if (WiFi.status() == WL_CONNECTED) //Check WiFi connection status
  {
    HTTPClient http; //Declare an object of class HTTPClient
    WiFiClient client;
    if (http.begin(client, myServer))
    { //new http lib function
      int httpCode = http.GET(); //Send the request
      if (httpCode >= 200 && httpCode <= 300) //TODO check for response code to turn off the alarm
      { //Check the returning code
        return true;
      }else{
        return false;
      }
    }else{
      return false;
    }
  } else {
    return false;
  }
}


void createStartingJsonStruct(File settings) {
    // Declare the JSON object to use
    JSONVar root;    // main json root object
    JSONVar wifiArr; // the WiFi array
    JSONVar wifiObj; // the object inside the WiFi array
    
    wifiObj["ssid"] = CustomSsid;    // "ssid": ssid
    wifiObj["psk"] = CustomPassword; // "psk": psk
    wifiArr[0] = wifiObj;      // "WiFi": [{wifiObj}] 
    root["WiFi"] = wifiArr;    // { "WiFi": wifiArr, }
    root.printTo(settings);    // print the main object to the settings file!
}

void SaveCustomWifi(String ssid, String psk)
{
  File settingsRead = SPIFFS.open("/settings.json", "r"); //open file in write mode
  String jsonstr;                                         //store its contents...
  while (settingsRead.available())
  {
    jsonstr += char(settingsRead.read());
  }
  settingsRead.close(); //close it

  //JSON parse!!
  JSONVar myObject = JSON.parse(jsonstr);
  File settingsWrite = SPIFFS.open("/settings.json", "w"); //open file in read mode
  
  myObject["WiFi"][1]["ssid"] = ssid; // write ssid to index 1
  myObject["WiFi"][1]["psk"] = psk;   // write psk to index 1

  myObject.printTo(settingsWrite);    // write to settings 
  settingsWrite.close(); //close it
}

String readSsidAtIndex(int ind) // read "ssid" from the array at index ind
{
  File settings = SPIFFS.open("/settings.json", "r"); // open the file
  String jsonstr; // var to store the string from the file
  while (settings.available()) // while there's content to be read
  {
    jsonstr += char(settings.read()); // append to jsonstr
  }
  JSONVar myObject = JSON.parse(jsonstr); // parse/unmarshall the json string to myObject
  settings.close(); // close the settings file
  
  // read form the "WiFi": [{"ssid":..},{"ssid":...}] at index ind
  String tmp = JSON.stringify(myObject["WiFi"][ind]["ssid"]);
  
  // tmp is something like "myWiFiNetwork" , so we have to get rid of the quotes ""
  tmp = tmp.substring(1, tmp.length() -1);  
  return tmp; // return the SSID! 
}

String readPskAtIndex(int ind) // read "psk" from the array at index ind
{
  File settings = SPIFFS.open("/settings.json", "r"); // open the file
  String jsonstr; // var to store the string from the file
  while (settings.available()) // while there's content to be read
  {
    jsonstr += char(settings.read()); // append to jsonstr
  }
  JSONVar myObject = JSON.parse(jsonstr); // parse/unmarshall the json string 
  settings.close(); // close the settings file
  
  // read form the "WiFi": [{"psk":..},{"psk":...}] at index ind
  String tmp = JSON.stringify(myObject["WiFi"][ind]["psk"]);
  
  // tmp is something like "myWiFiPass" , so we have to get rid of the quotes ""
  tmp = tmp.substring(1, tmp.length() -1);
  return tmp;
}

void setup() {
  // put your setup code here, to run once:
  if (!SPIFFS.exists("/settings.json")) //check if settings exists
  {
    File settings = SPIFFS.open("/settings.json", "w"); // open the file in write mode (this will create it)
    createStartingJsonStruct(settings); // save the starting configuration
    settings.close(); //close the file
  }

  File settingsRead = SPIFFS.open("/settings.json", "r"); //open file in read mode
  String jsonstr;                                         //store its contents...
  while (settingsRead.available())
  {
    jsonstr += char(settingsRead.read());
  }
  settingsRead.close(); //close it

  //JSON parse!!
  JSONVar myObject = JSON.parse(jsonstr);

  if (myObject["WiFi"].length() == 1) //if there's only one wifi cred..
  {
    if (!(WiFi.SSID() == readSsidAtIndex(0) && WiFi.psk() == readPskAtIndex(0))) // check if the current wifi connection isn't the WiFi[0] (modem)
    {
      SaveCustomWifi(WiFi.SSID(), WiFi.psk()); // if it is not, then save the new wifi credential
      checkInternetQuality = true; //set the checkInternetQuality flag to true
    }else{
      checkInternetQuality = false; //we are connected to the only wifi avalable, we do not need to check the internet quality
    }
  }
  else // if we're connected to the WiFi[0]
  {
    // I dont remember what(why) this does exactly, but something like check if the current wifi connection is not WiFi[0] and is not WiFi[1] and also that is not empty
    if((WiFi.SSID() != readSsidAtIndex(1) && WiFi.psk() != readPskAtIndex(1)) && (WiFi.SSID() != readSsidAtIndex(0) && WiFi.psk() != readPskAtIndex(0)) && WiFi.SSID() != "" && WiFi.psk() != ""){
      SaveCustomWifi(WiFi.SSID(), WiFi.psk()); // save this new wifi connection
    }
    checkInternetQuality = true;
  }

  if(checkInternetQuality){
    WiFi.begin(readSsidAtIndex(1), readPskAtIndex(1)); //connect to the WiFi[1] if we're going to do a internet quality check 
  }else{
     WiFi.begin(readSsidAtIndex(0), readPskAtIndex(0)); // stay on WiFi[0] if we don't have any other wifi network to test
  }
  
}

void loop() {
  // put your main code here, to run repeatedly:

  currentTime = millis(); //get the millis!
    
    
    if(checkInternetQuality){
        // start the InternetQuality Testing!
        
        if((currentTime - internetQualityMillis) > (120000 * 2)){
            // if >four minutes has passed
            checkInternetQuality = false; // we do not need to keep checking all of this
            if(!secondWiFiPassed){// IDK if this is really necessary, but whatever
                secondWiFiPassed = true;
                if(httpErrorCount > 2){
                    secondWiFiOk = false;
                } else {
                    secondWiFiOk = true;
                }
            }
            
            if(firstWiFiOk){
                WiFi.begin(readSsidAtIndex(1), readPskAtIndex(1));
            } else {
                if (secondWiFiOk){
                    WiFi.begin(readSsidAtIndex(0), readPskAtIndex(0));
                } else {
                    ESP.reset();
                }
            }
        } else if ((currentTime - internetQualityMillis) > 120000){
            // if >two minutes has passed
            if (!firstWiFiPassed){
                firstWiFiPassed = true; //do this only once
                WiFi.begin(readSsidAtIndex(0), readPskAtIndex(0)); // connect to the second wifi at index 1
                if(httpErrorCount > 0){
                    firstWiFiOk = false;
                }else{
                    firstWiFiOk = true;
                }
                
            }
        }
    }
    
    
    if(currentTime - previousRequestMillis > 30000){
        // do a request and check if it was successful
        bool err = sendRequest();
        if(err){
            httpErrorCount++;
        } else {
            httpErrorCount = 0;
        }
    }
}