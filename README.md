This is the first post on this blog, so I wanted to share something I couldn't find and had to figure out myself

So.. I was working on an IoT project that needed two possible WiFi connections, but only one configurable by the user to secure a more reliable communication with a backend service.

The quickest and easiest way to handle the user configurable WiFi credential is to use the WiFiManager library for the ESP8266 and the ESP8266HTTPClient lib for the HTTP requests.

<a href="https://github.com/tzapu/WiFiManager/tree/master/examples/OnDemandConfigPortal" target="_blank" >[WiFiManager code]</a>
```arduino
#include Arduino.h
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

#define TRIGGER_PIN 0

void setup() {
  Serial.begin(115200);
  Serial.println("\n Starting");

  pinMode(TRIGGER_PIN, INPUT_PULLUP);
}

void loop(){
    if(digitalRead(TRIGGER_PIN) == LOW){
        WiFiManager wifiManager;
        if (!wifiManager.startConfigPortal("OnDemandAP")) {
          Serial.println("failed to connect and hit timeout");
          delay(3000);
          //reset and try again, or maybe put it to deep sleep
          ESP.reset();
          delay(5000);
        }
        Serial.println("connected...yeey :)");
    }
    //do other stuff...
}
```

But.. there's a problem!

How do we manage multiple WiFi connections?

We could have modified the WiFiManager library to interact with the wifiMulti lib, but I'm not really a fan of modifying existing and working libraries. So there's one thing to do...

We have to make our own code 😃 (or my own code and then share it here, idk)
But... How?

Using <a href="https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html" target="_blank" >SPIFFS</a> and <a href="https://github.com/arduino-libraries/Arduino_JSON" target="_blank" >Arduino_JSON</a> to store the users WiFi configurations!

> This approach can also work as a save other configuration variables that can be useful for the code.

Both libraries have nice documentation, I recommend you to RTFM and explore the examples, but I will try my best to explain the code.


## JSON structure
We will start with the config file, writing a  file `config.json` in SPIFFS that will contain an array object with the different WiFi credentials.

```json
{
    "WiFi": [
        {
            "ssid": "wifi1", 
            "psk": "pass1"
        },
        {
            "ssid": "wifi2", 
            "psk": "pass2"
        },
    ]    
}
```
the WiFi object array has one "hard coded" (`WiFi[0]`) credential and the other (`WiFI[1]`) credential that's configurable by the user 

## Using the Arduino_JSON library
How do we make that structure? easy, just follow the <a href="https://github.com/arduino-libraries/Arduino_JSON/tree/master/examples" target="_blank" >examples</a>!

```Arduino
void createStartingJsonStruct(File settings) {
    // Declare the JSON object to use
    JSONVar root;    // main json root object
    JSONVar wifiArr; // the WiFi array
    JSONVar wifiObj; // the object inside the WiFi array
    
    wifiObj["ssid"] = ssid;    // "ssid": ssid
    wifiObj["psk"] = password; // "psk": psk
    wifiArr[0] = wifiObj;      // "WiFi": [{wifiObj}] 
    root["WiFi"] = wifiArr;    // { "WiFi": wifiArr, }
    root.printTo(settings);    // print the main object to the settings file!
}
```
this function receives a settings file (in write mode!) and then proceeds to create the JSON structure and write it to the settings file.

How do we use this function?

```Arduino
 File settings = SPIFFS.open("/settings.json", "w"); //open the file in write mode
 WriteStartingConfigFile(settings); // write the starting config json
 settings.close(); // close the file
 ```

And we also need a function to save/change the custom wifi credential

```Arduino
//TODO: complete this!
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
```

Now we can save the user wifi network with `SaveCustomWifi(ssid, psk);`:D

All nice, but how do we retrieve the data?

```Arduino
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
```
and the same with the pass phrase key
```Arduino
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
```

*it's all in the comments...*

so, now if we use the `readSsidAtIndex(0)` and `readPskAtIndex(0)`, we get the "hard coded" credentials of the WiFi.

and if we use the `readSsidAtIndex(1)` and `readPskAtIndex(1)`, we get the user wifi credentials

Now with these functions, we can start doing some stuff.

We can check if the file settings.json exists with this if statement

```arduino
if (!SPIFFS.exists("/settings.json"))
```

if it does not exist, then we create it:
```arduino
if (!SPIFFS.exists("/settings.json")) //check if settings exists
  {
    File settings = SPIFFS.open("/settings.json", "w"); // open the file in write mode (this will create it)
    WriteStartingConfigFile(settings); // save the starting configuration
    settings.close(); //close the file
}currentTime = millis(); //get the millis!
    
    
    if(checkInternetQuality){
        // start the InternetQuality Testing!
        
        if((currentTime - internetQualityMillis) > (120000 * 2)){
            // if >four minutes has passed
            checkInternetQuality = false; // we do not need to keep checking all of this
            if(!secondWiFiPassed){// IDK if this is really necessary, but whatever
                secondWiFiPassed = true;
                if(errorCount > 2){
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
                if(errorCount > 0){
                    firstWiFiOk = false;
                }else{
                    firstWiFiOk = true;
                }
                
            }
        }
    }
    
    
    if(currentTime - previousRequestMillis > 30000){
        // do a request and check if it was successful
        err = sendRequest();
        if(err){
            httpErrorCount++;
        } else {
            httpErrorCount = 0;
        }
    }
```

After we checked the existence of the settings.json file, we can read the file and parse its contents to a json object

```arduino
  File settingsRead = SPIFFS.open("/settings.json", "r"); //open file in read mode
  String jsonstr;                                         //store its contents...
  while (settingsRead.available())
  {
    jsonstr += char(settingsRead.read());
  }
  settingsRead.close(); //close it

  //JSON parse!!
  JSONVar myObject = JSON.parse(jsonstr);
```

then we can decide depending on the length of the WiFi array if we make a check the Internet quality (sending requests) or if we should only stick to the hard-coded wifi credentials.

```arduino
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
```

Now that we have the logic that tells us if we want to check the internet quality and saves new networks added, then we can start implementing an internet quality checker for these two wifi networks!

To do this, we can have two booleans `firstWiFiOk` and `secondWiFiOk` to store the final quality for the wifi network.

I'm going to test the quality by connecting to the network and reporting the device to my server every 30s and for each HTTP error adding 1 to an error counter (`httpErrorCount`), after 2 minutes, switching to the other wifi network and doing the same, if the first wifi network has less than 2 errors, I can say it's a good network and connect to it, and if it has more than 2 errors, I will see if the second wifi network (WiFi[0]) has less than 2 errors I connect to it, but if it has more than 2 errors, then I'll reset the esp.

That would look something like this:

```arduino
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

void loop(){
    currentTime = millis(); //get the millis!
    
    
    if(checkInternetQuality){
        // start the InternetQuality Testing!
        
        if((currentTIme - internetQualityMillis) > (120000 * 2)){
            // if >four minutes has passed
            checkInternetQuality = false; // we do not need to keep checking all of this
            if(!secondWiFiPassed){// IDK if this is really necessary, but whatever
                secondWiFiPassed = true;
                if(errorCount > 2){
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
                if(errorCount > 0){
                    firstWiFiOk = false;
                }else{
                    firstWiFiOk = true;
                }
                
            }
        }
    }
    
    
    if(currentTime - previousRequestMillis > 30000){
        // do a request and check if it was successful
        err = sendRequest();
        if(err){
            httpErrorCount++;
        } else {
            httpErrorCount = 0;
        }
    }
    
}
```

---------
## And That's it! 🎉

you can check the full source code on <a href="https://github.com/acidobinario/espMultiWifiExample" target="_blank" >GitHub</a> 

if you have doubts or suggestions, write me on twitter! <a href="https://twitter.com/acido_binario" target="_blank" >@acido_binario</a> 

------