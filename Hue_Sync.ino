#include <dotstar.h>
#include <Particle.h>
#include <HttpClient.h>
#include "HttpClient/HttpClient.h"
#include "math.h"
#include <sstream>
#include "stdio.h"
#include "string.h"
#include <iomanip>
#include <algorithm>
#include <cctype>
#include "stdlib.h"
#define NUMPIXELS 6
#define DATAPIN   D5
#define CLOCKPIN  D6
HttpClient http;
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DATAPIN, CLOCKPIN);

http_request_t request;
http_response_t response;

int bri;
int hue;
int sat;
uint32_t color;
double x;
double y;
bool isOn = false;
bool useXY = false;
char* hostName;
char* hueUser;
char* requestPath;
char* fullPath;
bool isValid = false;
//forward declarations
std::string getValue(std::string str, std::string key, std::string delim, int startOffset = 0, int endOffset = 0);
std::string getSubString(std::string str, std::string key, int startOffset = 0);
uint32_t getXYValue(std::string str, std::string key, std::string delim, int startOffset = 0);
uint32_t createRGB(int r, int b, int g);
uint32_t to_rgb(float x, float y, int bri);

void setup() {
    Particle.variable("bri", bri);
    Particle.variable("hue", hue);
    Particle.variable("sat", sat);
    Particle.variable("isOn", isOn);
    Particle.variable("x", x);
    Particle.variable("y", y);
    Particle.variable("color", color);
    Particle.variable("useXY", useXY);
    Particle.function("Set_Hostname", setHostName);
    Particle.function("Set_User", setHueUser);
    Particle.function("Set_Path", setRequestPath);
    Particle.function("useHUE", useHUE);
    Particle.function("SetRGB", setRGB);
    Serial.begin(9600);
    strip.begin();
    color = strip.Color(255,0,0);//set default to red
    delay(10000);
}

void loop() {
    if(isValid && useXY){
        queryHue();
    }
    renderString();
    isValid = validateSettings();
}

bool validateSettings(){
    if(hostName != NULL && hueUser != NULL && requestPath != NULL){
        char str[strlen(hostName) + 25 + strlen(requestPath)];
        strcpy (str, hostName);
        strcat (str, "/api/hue_user_string");
        strcat (str, requestPath);
        fullPath = str;
        return true;
    }
    
    if(requestPath == NULL){
        fullPath = strcpy(fullPath, "Request Path cannot be null");
    }
    if(hueUser == NULL){
        fullPath = strcpy(fullPath, "Hue User cannot be null");
    }
    if(hostName == NULL){
        fullPath = strcpy(fullPath, "Hostname cannot be null");
    }
    
    Particle.publish("hostName", hostName);
    Particle.publish("hueUser", hueUser);
    Particle.publish("requestPath", requestPath);
    Particle.publish("FullPath", fullPath);
    delay(2000);
    return false;
}

int setRGB(String arg){
    int commaIndex = arg.indexOf(',');
    int secondCommaIndex = arg.indexOf(',', commaIndex+1);
    int lastCommaIndex = arg.lastIndexOf(',');

    int r = arg.substring(0, commaIndex).toInt();
    int g = arg.substring(commaIndex+1, secondCommaIndex).toInt();
    int b = arg.substring(lastCommaIndex+1).toInt();
    color = strip.Color(r,g,b);
    return 1;
}

int setHostName(String arg){
    hostName = strdup(arg);
    Particle.publish("Set Hostname", hostName);
    return 1;
}

int setHueUser(String arg){
    hueUser = strdup(arg);
    Particle.publish("Set Hue User", hueUser);
    return 1;
}

int setRequestPath(String arg){
    requestPath = strdup(arg);
    Particle.publish("Set Request Path", requestPath);
    return 1;
}

void renderString()
{
    for(int i = 0; i < NUMPIXELS; i++)
    {
        strip.setPixelColor(i, color);
        strip.show();
        delay(10);
    }
}

int useHUE(String arg){
    if(isValid && (arg == "true" || arg == "True" || arg == "TRUE")){
        useXY = true;
        return 1;
    }
    useXY = false;
    return 0;
}

void queryHue(){
    http_header_t headers[] = {
        { "Content-Type", "application/json" },
        { NULL, NULL } // NOTE: Always terminate headers will NULL
    };
    request.hostname = hostName;
    request.port = 80;
    request.path = ("/api/"+ std::string(hueUser) + std::string(requestPath)).c_str();
    http.get(request, response, headers);

    if(response.body.length() > 0){
        Particle.publish("got_hue_data", response.body.c_str());
        std::string str = response.body.c_str();
        
        isOn = (getValue(str, "{\"on\":", ",", 6) == "true");
        
        if(isOn){
            bri = atoi(getValue(str, "bri\":", ",", 5).c_str());
            hue = atoi(getValue(str, "hue\":", ",", 5).c_str());
            sat = atoi(getValue(str, "sat\":", ",", 5).c_str());
            if(useXY){
                color = getXYValue(str, "\"xy\":[", ",", 6);
            }else{
                color = strip.Color(255,0,0);
            }
        }else{
            color = strip.Color(0,0,0);
        }
    }
    
    delay(2000);
}

//attempt to parse a value from the JSON string (offset arguments are optional)
std::string getValue(std::string str, std::string key, std::string delim, int startOffset, int endOffset){
    std::size_t valStart = str.find(key);
    std::string valSub = getSubString(str, key, startOffset);
    std::size_t valEnd = valSub.find_first_of(delim);
    std::string valStr = str.substr(valStart + startOffset, valEnd + endOffset);
    return valStr;
}

//returns a substring based on a search key, and optional offset
std::string getSubString(std::string str, std::string key, int startOffset){
    std::size_t valStart = str.find(key);
    return str.substr(valStart + startOffset);
}

//(startOffset is optional)
uint32_t getXYValue(std::string str, std::string key, std::string delim, int startOffset){
    x = (double)atof(getValue(str, key, delim, 6).c_str());
    
    //x = ;//atof(getValue(str, key, delim, 6).c_str());
    //Particle.publish("x", (char*)(&x));
    std::string valSub = getSubString(str, key, startOffset);
    std::size_t yStart = valSub.find(delim);
    std::string ySub = valSub.substr(yStart);
    std::size_t yEnd = valSub.find_first_of("]") - yStart;
    std::string yStr = valSub.substr(yStart+1,yEnd-1);
    
    y = (double)atof(yStr.c_str());
    //Particle.publish("y",(char*)(&y));
    color = to_rgb(x, y, bri);
    return color;
}

uint32_t to_rgb(double x, double y, int bri){
     //Calculate XYZ values
    int z = 1 - x - y;
    int xColor = (bri / y) * x;
    int zColor = (bri / y) * z;
    
    //Covert to RGB using Wide RGB D65 conversion
    int rColor = xColor * 1.656492 - bri * 0.354851 - zColor * 0.255038;
    int gColor = xColor * -0.707196 + bri * 1.655397 + zColor * 0.036152;
    int bColor = xColor * 0.051713 - bri * 0.121364 + zColor * 0.036152;
    
    //Gamma Correction
    if (rColor <= 0.003130){
        rColor = 12.92 * rColor;
    }else{
        rColor = 1.055 * pow(rColor, 1/2.4) - 0.055;
    }
    
    if (gColor <= 0.0031308){
        gColor = 12.92 * gColor;
    }else{
        gColor = 1.055 * pow(gColor, 1/2.4) - 0.055;
    }
    
    if (rColor <= 0.0031308){
        bColor = 12.92 * bColor;
    }else{
        bColor = 1.055 * pow(bColor, 1/2.4) - 0.055;
    }
    //Since we are not validating using the Hue color gamuts, we need to handle edge cases.
    if (rColor < 0){
        rColor = 0;
    }else if (rColor > 255){
        rColor = 255;
    }
    
    if (gColor < 0){
        gColor = 0;
    }else if (gColor > 255){
        gColor = 255;
    }
    
    if (bColor < 0){
        bColor = 0;
    }else if(bColor > 255){
        bColor = 255;
    }
    
    return strip.Color(rColor,gColor,bColor);
}