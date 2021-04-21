#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <list>

#define csprintf(...) { Serial.printf(__VA_ARGS__ ); }

//create your own WiFi-setup.h with your user name and password (see WiFi-setup.h.sample)
// or use the #define statements below.
#include "WiFi-setup.h"

#define UPLOAD_FILE

//#define WIFI_SSID "linksys-e2000"
//#define WIFI_PASS "12345678"



//The LED very gently breathes in the background without annoying your peripheral vision, but lights up brightly when a new device is found and invited.
#define LED_PIN 14
//#define INVERT_LED

#define PWM_RANGE 16383
#define DIM_BRIGHTNESS ((int) (PWM_RANGE * 0.01))



WiFiClient httpWifiClient;
HTTPClient http;

int HttpRequestInternal(const char * url, String * post)
{
	http.setTimeout(5000);
	http.begin(httpWifiClient,url);  //Specify request destination
	int httpCode = 0;

	if(post)
	{
		httpCode=http.POST(*post);
	}
	else
	{
		httpCode=http.GET();                                                                  //Send the request
	}


	if (httpCode > 0)
	{ //Check the returning code

	//	String payload = http.getString();   //Get the request response payload
	//	Serial.println(payload);                     //Print the response payload
	}


	http.end();   //Close connection

	return httpCode;
}

int HttpRequest(const char * url, int retries)
{

	while(retries)
	{
		int ret=HttpRequestInternal(url,NULL);

		if(ret>=0) return ret;
		retries--;

		delay(20);
	}

	return -999;
}

int HttpPost(String & url, String & payload, int retries)
{

	while(retries)
	{
		int ret=HttpRequestInternal(url.c_str(),&payload);

		if(ret>=0) return ret;
		retries--;

		delay(20);
	}

	return -999;
}











void SetLed(bool bLight)
{
  #ifdef INVERT_LED
  analogWrite(LED_PIN, bLight ? 0 : PWM_RANGE);
  #else
  analogWrite(LED_PIN, bLight ? PWM_RANGE : 0);
  #endif
}

void DimLed(int level)
{
  #ifdef INVERT_LED
  analogWrite(LED_PIN, PWM_RANGE - level);
  #else
  analogWrite(LED_PIN, level);
  #endif
}

class IgnoreItem
{
public:
	String strSSID;
	uint32_t ulTimestamp;
};

std::list<IgnoreItem> listIgnore;

//The setup function is called once at startup of the sketch
void setup()
{
	// Add your initialization code here
	Serial.begin(115200);
	Serial.println();

	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	delay(100);

	analogWriteFreq(120);
	analogWriteRange(PWM_RANGE);

	pinMode(LED_PIN, OUTPUT);
	
	SetLed(0);

#ifdef UPLOAD_FILE
	csprintf("\n### Tasmota-Converta by Leif Claesson ###\n");
#else
	csprintf("\n### Tasmota-Invita by Leif Claesson ###\n");
#endif


}

bool bFirst = true;
int iStreak = 0;



bool DoHttpUpload()
{
	//csprintf("HttpRequest test!\n");

	const char * szFilename="Lightbulb.bin";

	if(!LittleFS.begin())
	{
		csprintf("couldn't open littlefs\n");
		return false;
	}

	File myfile=LittleFS.open(szFilename, "r");

	if(!myfile)
	{
		csprintf("Unable to open %s\n",szFilename);
		return false;
	}

	csprintf("File %s opened! size %u\n",szFilename,myfile.size());


//	String url="http://172.22.22.40:7381/dev_upload";
	char use_url[256];
	sprintf(use_url, "http://%s/u2", WiFi.gatewayIP().toString().c_str());

	http.setTimeout(5000);
	http.begin(httpWifiClient,use_url);  //Specify request destination

	csprintf("\nUploading %s to %s ...\n",szFilename,use_url);

    int httpCode = http.sendRequest("POST",&myfile,myfile.size(),szFilename,"application/octet-stream","data");

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
      http.end();
      return true;
    }
    else
    {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;


}


void loop()
{

	SetLed(0);

	WiFi.mode(WIFI_STA);
	WiFi.disconnect();




#ifdef UPLOAD_FILE
	Serial.printf("\nScanning for AP-mode tasmota devices to convert.\n\n");
#else
	Serial.printf("\nScanning for AP-mode tasmota devices to invite to network %s.\n\n",WIFI_SSID);
#endif

	// WiFi.scanNetworks will return the number of networks found
	int n = WiFi.scanNetworks();

	unsigned char network_flag[256];
	memset(network_flag, 0, sizeof(network_flag));

	if(n > 255)
	{
		n = 255;
	}

reuse_list:

	int tasmotas = 0;


	//  Serial.println("scan done");
	if(n == 0)
	{
		Serial.println("no networks found");
		iStreak++;
	}
	else
	{
		Serial.print(n);
		Serial.println(" networks found");
		/*
		    for (int i = 0; i < n; ++i) {
		      // Print SSID and RSSI for each network found
		    }
		*/


		Serial.print(" Ch  BSSID              RSSI  Encr  SSID\n");

		tasmotas = 0;
		int best_idx = -1;
		int best_rssi = -1000;
		for(int i = 0; i < n; i++)
		{
			if(network_flag[i])
			{
				continue;
			}

			std::list<IgnoreItem>::iterator iter;
			for(iter=listIgnore.begin();iter!=listIgnore.end();)
			{
				IgnoreItem & item=*iter;
				int age=(millis()-item.ulTimestamp);

				if(age>30000)
				{
					//too old
					//csprintf("%s too old to ignore (%i ms), erasing\n",item.strSSID.c_str(),age);
					iter=listIgnore.erase(iter);
				}
				else
				{
					if(item.strSSID==WiFi.SSID(i))
					{
						//csprintf("Ignoring %s age %i!\n",item.strSSID.c_str(),age);
						network_flag[i]=true;
					}
					iter++;
				}
			}

			if(network_flag[i])
			{
				continue;
			}




			int ch = WiFi.channel(i);
			Serial.print(ch < 10 ? "  " : " ");
			Serial.print(ch);
			Serial.print("  ");
			Serial.print(WiFi.BSSIDstr(i));
			Serial.print("  ");
			Serial.print(WiFi.RSSI(i));
			Serial.print("   ");

			switch(WiFi.encryptionType(i))
			{
			case ENC_TYPE_WEP:
				Serial.print("WEP ");
				break;
			case ENC_TYPE_TKIP:
				Serial.print("TKIP");
				break;
			case ENC_TYPE_CCMP:
				Serial.print("WPA ");
				break;
			default:
				Serial.print(WiFi.encryptionType(i));
				Serial.print("   ");
				break;
			case ENC_TYPE_NONE:
				Serial.print("None");
				break;
			case ENC_TYPE_AUTO:
				Serial.print("Auto");
				break;
			}

			//		Serial.print((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
			Serial.print("  ");
			Serial.print(WiFi.SSID(i));
			Serial.println("");

			/*int spaces=32-WiFi.SSID(i).length();
			for(int x=0;x<spaces;x++)
			{
				Serial.printf(" ");
			}

			Serial.print("  (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			*/
			delay(0);
			if(!strncasecmp(WiFi.SSID(i).c_str(), "tasmota", 7))
			{

				if(best_rssi < WiFi.RSSI(i))
				{
					best_rssi = WiFi.RSSI(i);
					best_idx = i;
				}
				tasmotas++;
			}
			else
			{
				network_flag[i] = true;
			}
		}

		if(best_idx >= 0)
		{
			network_flag[best_idx] = true;
			int counter = 30;

			SetLed(1);

			Serial.printf("%i found.\n", tasmotas);

			Serial.printf("Connecting to %s (%i)", WiFi.SSID(best_idx).c_str(), WiFi.RSSI(best_idx));

			WiFi.begin(WiFi.SSID(best_idx), "");

			while(WiFi.status() != WL_CONNECTED)
			{
				delay(500);
				Serial.print(".");
				counter--;
				if(!counter)
				{
					Serial.printf("timed out.\n");
					//delay(5000);
					return;
				}
			}

			csprintf("Connected!\n");

			for(int x = 0; x < 4; x++)
			{
				SetLed(0);
				delay(80);
				SetLed(1);
				delay(80);
			}

			//delay(1000);
			csprintf("Our IP is %s, Gateway is %s\n", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());

#ifdef UPLOAD_FILE

			char use_url[256];
			sprintf(use_url, "http://%s/cm?cmnd=SetOption78%%201", WiFi.gatewayIP().toString().c_str());

			int result = HttpRequest(use_url, 3);
			//int result=200;

			if(result == 200)
			{
				csprintf("SetOption78 success!\n");

				DimLed(PWM_RANGE>>3);

				bool bSuccess=DoHttpUpload();

				IgnoreItem ignore;
				ignore.strSSID=WiFi.SSID();
				ignore.ulTimestamp=millis();

				listIgnore.push_back(ignore);

				if(bSuccess)
				{
					for(int i=0;i<3;i++)
					{
						SetLed(1);
						delay(500);
						SetLed(0);
						delay(500);
					}
				}
				else
				{
					SetLed(0);
					delay(200);
					SetLed(1);
					delay(2000);
					SetLed(0);
				}
			}

#else


			char use_url[256];
			sprintf(use_url, "http://%s/wi?s1=%s&p1=%s&save=", WiFi.gatewayIP().toString().c_str(), WIFI_SSID, WIFI_PASS);

			//sprintf(use_url,"http://%s",WiFi.gatewayIP().toString().c_str());



			csprintf("Requesting set SSID %s...", WIFI_SSID);


			int result = HttpRequest(use_url, 3);
			//int result=200;

			if(result == 200)
			{
				csprintf("success!\n");


				WiFi.disconnect();

				for(int b = 0; b < 2; b++)
				{
					for(float a = 0.0f; a < 1.0f; a += 0.01f)
					{
						DimLed(pow(a, 3.0f)*PWM_RANGE);
						delay(5);
					}
					for(float a = 1.0f; a > 0.0f; a -= 0.01f)
					{
						DimLed(pow(a, 3.0f)*PWM_RANGE);
						delay(5);
					}
				}

			}
			else
			{
				SetLed(0);
				csprintf("error.\n");
			}
#endif


			SetLed(0);

			iStreak++;

		}
		else
		{
			Serial.printf("No tasmotas found.\n");
			iStreak = 0;
		}

	}

	WiFi.disconnect();

	if(iStreak >= 1 && tasmotas > 1)
	{
		Serial.printf("Reusing list\n");
		delay(500);
		goto reuse_list;
	}


	if(bFirst)
	{
		bFirst=false;
	}
	else
	{
		// Wait a bit before scanning again
		delay(3000);
	}

	if(!iStreak)
	{
		SetLed(0);
		for(float a = 0.0f; a < 1.0f; a += 0.002f)
		{
			DimLed(pow(a, 1.5f)*DIM_BRIGHTNESS);
			delay(5);
		}
		for(float a = 1.0f; a > 0.0f; a -= 0.002f)
		{
			DimLed(pow(a, 1.5f)*DIM_BRIGHTNESS);
			delay(5);
		}

		SetLed(0);
	}


}
