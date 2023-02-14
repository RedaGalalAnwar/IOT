
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <NTPClient.h>
#include <WiFiUdp.h>


#include <Wire.h>
#include "MAX30105.h"

#include "heartRate.h"

#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>




MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;
String uid;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};


/* 1. Define the WiFi credentials */
#define WIFI_SSID "ReDaGaLaL"
#define WIFI_PASSWORD "@#passward"

/* 2. Define the API Key */
#define API_KEY "AIzaSyACyOysL6vx_GXddRBnGVNrdcZr"

/* 3. Define the project ID */
#define FIREBASE_PROJECT_ID "oxygiot-ac1ca"

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "admin@gmail.com"
#define USER_PASSWORD "123456"

#define DATABASE_URL "oxygeniot-default-rtdb.firebaseio.com"

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config; 

unsigned long sendDataPrevMillis = 0;
unsigned long dataMillis = 0;
int count = 0;





// The Firestore payload upload callback function
void fcsUploadCallback(CFS_UploadStatusInfo info)
{
    if (info.status == fb_esp_cfs_upload_status_init)
    {
        Serial.printf("\nUploading data (%d)...\n", info.size);
    }
    else if (info.status == fb_esp_cfs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s\n", (int)info.progress, "%");
    }
    else if (info.status == fb_esp_cfs_upload_status_complete)
    {
        Serial.println("Upload completed ");
    }
    else if (info.status == fb_esp_cfs_upload_status_process_response)
    {
        Serial.print("Processing the response... ");
    }
    else if (info.status == fb_esp_cfs_upload_status_error)
    {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}

void setup()
{

    Serial.begin(115200);



  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED


    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    /* Assign the api key (required) */
    config.api_key = API_KEY;

    /* Assign the user sign in credentials */
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

      /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

#if defined(ESP8266)
    // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
    fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
#endif
    
    // Limit the size of response payload to be collected in FirebaseData
    fbdo.setResponseSize(2048);


 config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

    Firebase.begin(&config, &auth);



    Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
    // For sending payload callback
    // config.cfs.upload_callback = fcsUploadCallback;


     Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);


timeClient.begin();


  
}




void loop()
{
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  
  
  String formattedTime = timeClient.getFormattedTime();
  

  int currentHour = timeClient.getHours();
  

  int currentMinute = timeClient.getMinutes();
  
   
  int currentSecond = timeClient.getSeconds();
 
  String weekDay = weekDays[timeClient.getDay()];
    

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

  int monthDay = ptm->tm_mday;
  
  int currentMonth = ptm->tm_mon+1;
  

  String currentMonthName = months[currentMonth-1];
 

  int currentYear = ptm->tm_year+1900;

  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);

String  FullDate = currentDate +" "+ formattedTime ;


  
// Firebase.ready() should be called repeatedly to handle authentication tasks.
//  if (Firebase.ready() && (millis() - dataMillis > 60000 || dataMillis == 0))
// {
        dataMillis = millis();

        // For the usage of FirebaseJson, see examples/FirebaseJson/BasicUsage/Create.ino
        FirebaseJson content;

        // We will create the nested document in the parent path "a0/b0/c0
        // a0 is the collection id, b0 is the document id in collection a0 and c0 is the collection id in the document b0.
        // and d? is the document id in the document collection id c0 which we will create.
        String documentPath = "OXYGEN_RATE";

        // If the document path contains space e.g. "a b c/d e f"
        // It should encode the space as %20 then the path will be "a%20b%20c/d%20e%20f"

  long irValue = particleSensor.getIR();
   if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

float SPO2 ;
int BPM ,avgBPM;
/***********************************************************************************************************************************/
 
 if(irValue<5000)
 {      content.set("fields/uid/stringValue", uid);
        content.set("fields/case/stringValue", "no finger");
        content.set("fields/irValue/doubleValue", irValue);
        content.set("fields/BPM/doubleValue", 0.00);
        content.set("fields/Avg BPM/doubleValue", 0.00);
          content.set("fields/DateTime/stringValue", FullDate);
      
        Serial.printf("Set string... %s\n", Firebase.setString(fbdo, F("/test/case"), "no finger ! ") ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get string... %s\n", Firebase.getString(fbdo, F("/test/case")) ? fbdo.to<const char *>() : fbdo.errorReason().c_str());
    
   Serial.printf("Set IR... %s\n", Firebase.setInt(fbdo, F("/test/IR"), irValue) ? "ok" : fbdo.errorReason().c_str());

   Serial.printf("Get IR... %s\n", Firebase.getInt(fbdo, F("/test/IR")) ? String(fbdo.to<int>()).c_str() : fbdo.errorReason().c_str());

    Serial.printf("Set BPM... %s\n", Firebase.setFloat(fbdo, F("/test/BPM"), 0.00) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get BPM... %s\n", Firebase.getFloat(fbdo, F("/test/BPM")) ? String(fbdo.to<float>()).c_str() : fbdo.errorReason().c_str());

   Serial.printf("Set Avg BPM... %s\n", Firebase.setFloat(fbdo, F("/test/Avg BPM"), 0.00) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get Avg BPM... %s\n", Firebase.getFloat(fbdo, F("/test/Avg BPM")) ? String(fbdo.to<float>()).c_str() : fbdo.errorReason().c_str()); 
  
 }
else
{
         content.set("fields/uid/stringValue", uid);
         content.set("fields/irValue/doubleValue", irValue);
        content.set("fields/case/arrayValue/values/[0]/stringValue", "your data is ");
        content.set("fields/BPM/doubleValue", beatsPerMinute);
        content.set("fields/Avg BPM/doubleValue",beatAvg);
        content.set("fields/DateTime/stringValue", FullDate);
  
        Serial.printf("Set string... %s\n", Firebase.setString(fbdo, F("/test/case"), " your data ") ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get string... %s\n", Firebase.getString(fbdo, F("/test/case")) ? fbdo.to<const char *>() : fbdo.errorReason().c_str());
    
   Serial.printf("Set IR... %s\n", Firebase.setInt(fbdo, F("/test/IR"), irValue) ? "ok" : fbdo.errorReason().c_str());

   Serial.printf("Get IR... %s\n", Firebase.getInt(fbdo, F("/test/IR")) ? String(fbdo.to<int>()).c_str() : fbdo.errorReason().c_str());

    Serial.printf("Set BPM... %s\n", Firebase.setFloat(fbdo, F("/test/BPM"),beatsPerMinute) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get BPM... %s\n", Firebase.getFloat(fbdo, F("/test/BPM")) ? String(fbdo.to<float>()).c_str() : fbdo.errorReason().c_str());

   Serial.printf("Set Avg BPM... %s\n", Firebase.setFloat(fbdo, F("/test/Avg BPM"), beatAvg) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Get Avg BPM... %s\n", Firebase.getFloat(fbdo, F("/test/Avg BPM")) ? String(fbdo.to<float>()).c_str() : fbdo.errorReason().c_str()); 
}
 

 /*************************************************************************************************************************/

        Serial.print("Create a document... ");

        if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), content.raw()))
        {
            Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        }
        else
        {
            Serial.println(fbdo.errorReason());
        }

   

   // }
}
