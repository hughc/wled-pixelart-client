#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <FastLED.h>
#include <wled.h>
#include <FX.h>


#include <HTTPClient.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Library inclusions.
/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 *
 * This is an example for a v2 usermod.
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 *
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 *
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

/// Representation of an RGBA pixel (Red, Green, Blue, Alpha)
struct CRGBA
{
	union
	{
		struct
		{
			union
			{
				uint8_t r;
				uint8_t red;
			};
			union
			{
				uint8_t g;
				uint8_t green;
			};
			union
			{
				uint8_t b;
				uint8_t blue;
			};
			union
			{
				uint8_t a;
				uint8_t alpha;
			};
		};
		uint8_t raw[4];
	};

	/// allow copy construction
	inline CRGBA(const CRGBA &rhs) __attribute__((always_inline)) = default;

    /// allow assignment from one RGB struct to another
	inline CRGBA& operator= (const CRGBA& rhs) __attribute__((always_inline)) = default;

	// default values are UNINITIALIZED
	inline CRGBA() __attribute__((always_inline)) = default;

	/// allow construction from R, G, B, A
	inline CRGBA(uint8_t ir, uint8_t ig, uint8_t ib, uint8_t ia) __attribute__((always_inline))
		: r(ir), g(ig), b(ib), a(ia)
	{
	}

	/// allow assignment from R, G, and B
	inline CRGBA &setRGB(uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline))
	{
		r = nr;
		g = ng;
		b = nb;
		return *this;
	}

	CRGB toCRGB()
	{
		return CRGB(this->red, this->green, this->blue);
	}
};

CRGBA &nblend_a(CRGBA &existing, const CRGBA &overlay, fract8 amountOfOverlay)
{
	if (amountOfOverlay == 0)
	{
		return existing;
	}

	if (amountOfOverlay == 255)
	{
		existing = overlay;
		return existing;
	}

	// Corrected blend method, with no loss-of-precision rounding errors
	existing.red = blend8(existing.red, overlay.red, amountOfOverlay);
	existing.green = blend8(existing.green, overlay.green, amountOfOverlay);
	existing.blue = blend8(existing.blue, overlay.blue, amountOfOverlay);
	existing.alpha = blend8(existing.alpha, overlay.alpha, amountOfOverlay);

	return existing;
}

CRGBA blend_a(const CRGBA &p1, const CRGBA &p2, fract8 amountOfP2)
{
	CRGBA nu(p1);
	nblend_a(nu, p2, amountOfP2);
	return nu;
}

CRGB flatten(CRGBA &overlay, CRGB &existing)
{
	int const amountOfOverlay = overlay.alpha;
	if (amountOfOverlay == 0)
	{
		return existing;
	}

	if (amountOfOverlay == 255)
	{
		return overlay.toCRGB();
	}
	// Corrected blend method, with no loss-of-precision rounding errors
	CRGB rCRGB = CRGB( blend8(existing.red, overlay.red, amountOfOverlay), blend8(existing.green, overlay.green, amountOfOverlay), blend8(existing.blue, overlay.blue, amountOfOverlay));

	return rCRGB;
}

// class name. Use something descriptive and leave the ": public Usermod" part :)
class PixelArtClient : public Usermod
{

private:
	// Private class members. You can declare variables and functions only accessible to your usermod here
	bool enabled = false;
	bool initDone = false;
	unsigned long refreshTime = 0;
	unsigned long lastRequestTime = 0;

	// set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)
	String serverName = "https://app.pixelart-exchange.au/";
	String apiKey = "your_api_key";
	String clientName = "WLED";
	bool transparency = false;
	bool serverUp = false;
	int serverTestRepeatTime = 10;

	// These config variables have defaults set inside readFromConfig()
	int testInt;
	long testLong;
	int8_t testPins[2];
	int crossfadeIncrement = 10;
	int crossfadeFrameRate = 40;
	std::vector<std::vector<std::vector<CRGBA>>> image1;
	std::vector<std::vector<std::vector<CRGBA>>> image2;
	std::vector<int> image1durations;
	std::vector<int> image2durations;

	std::vector<std::vector<std::vector<CRGBA>>> *currentImage;
	std::vector<std::vector<std::vector<CRGBA>>> *nextImage;
	std::vector<int> *nextImageDurations;
	std::vector<int> *currentImageDurations;

	// need a struct to hold all this (pixels, frameCount, frame timings )
	int nextImageFrameCount = 1;
	int currentImageFrameCount = 1;
	CRGB nextImageBackgroundColour;
	CRGB currentImageBackgroundColour;
	int numFrames;

	int currentFrameIndex = 0;
	// within the image, we may have one or more frames
	std::vector<std::vector<CRGBA>> currentFrame;
	int currentFrameDuration;
	// within the next image,cache the first frame for a transition
	std::vector<std::vector<CRGBA>> nextFrame;
	int nextBlend = 0;

	// time betwwen images
	int duration;
	// in seconds
	int imageDuration = 10;
	bool imageLoaded = false;
	int imageIndex = 0;

	HTTPClient http;

	String playlist;
	String name;

	// string that are used multiple time (this will save some flash memory)
	static const char _name[];
	static const char _enabled[];

	// any private methods should go here (non-inline methosd should be defined out of class)
	void publishMqtt(const char *state, bool retain = false); // example for publishing MQTT message

public:
	// non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)

	/**
	 * Enable/Disable the usermod
	 */
	inline void enable(bool enable) { enabled = enable; }

	/**
	 * Get usermod enabled/disabled state
	 */
	inline bool isEnabled() { return enabled; }

	// in such case add the following to another usermod:
	//  in private vars:
	//   #ifdef USERMOD_EXAMPLE
	//   MyExampleUsermod* UM;
	//   #endif
	//  in setup()
	//   #ifdef USERMOD_EXAMPLE
	//   UM = (MyExampleUsermod*) usermods.lookup(USERMOD_ID_EXAMPLE);
	//   #endif
	//  somewhere in loop() or other member method
	//   #ifdef USERMOD_EXAMPLE
	//   if (UM != nullptr) isExampleEnabled = UM->isEnabled();
	//   if (!isExampleEnabled) UM->enable(true);
	//   #endif





	static uint16_t dummyEffect()
	{
		return FRAMETIME;
	}

	static uint16_t mode_pixelart(void)
	{
		return PixelArtClient::dummyEffect();
	}

	void requestImageFrames()
	{
		// Your Domain name with URL path or IP address with path

		currentImage = imageIndex ? &image1 : &image2;
		nextImage = imageIndex ? &image2 : &image1;
		currentImageDurations = imageIndex ? &image1durations : &image2durations;
		nextImageDurations = imageIndex ? &image2durations : &image1durations;
		const String serverPath = "api/image/pixels";
		const String clientPhrase = "screen_id=" + clientName;
		const String keyPhrase = "&key=" + apiKey;

		const String width = String(strip._segments[strip.getCurrSegmentId()].maxWidth);
		const String height = String(strip._segments[strip.getCurrSegmentId()].maxHeight);
		const String getUrl = serverName + (serverName.endsWith("/") ? "" : "/") + serverPath + "?" + clientPhrase + keyPhrase + "&width=" + width + "&height=" + height;
		;

		Serial.print("requestImageFrames: ");
		Serial.println(getUrl);
		http.begin((getUrl).c_str());

		// Send HTTP GET request, turn back to http 1.0 for streaming
		http.useHTTP10(true);
		int httpResponseCode = http.GET();
		// Serial.print("HTTP Response code: ");
		// Serial.println(httpResponseCode);
		if (httpResponseCode != 200)
		{
			Serial.print("image fetch failed, request returned code ");
			Serial.println(httpResponseCode);
			http.end();
			return;
		}
		// payload = http.getStream();

		// Serial.print("total stream length: ");
		// Serial.println(http.getString().length());

		// Serial.print("parseResponse() start: remaining heap: ");
		// Serial.println(ESP.getFreeHeap(), DEC);

		DynamicJsonDocument doc(2048);
		Stream &client = http.getStream();

		client.find("\"meta\"");
		client.find(":");
		// meta
		DeserializationError error = deserializeJson(doc, client);

		if (error)
		{
			Serial.print("deserializeJson() failed: ");
			Serial.println(error.c_str());
			imageLoaded = false;
			return;
		}

		const int totalFrames = doc["frames"];
		nextImageBackgroundColour = hexToCRGB(doc["backgroundColor"]);
		const int returnHeight = doc["height"];
		const int returnWidth = doc["width"];
		const char *path = doc["path"]; // "ms-pacman.gif"
		name = String(path);

		Serial.print("nextImage.resize: ");
		Serial.println(totalFrames);
		(*nextImage).resize(totalFrames);

		Serial.print("nextImage.size: ");
		Serial.println((*nextImage).size());

		for (size_t i = 0; i < totalFrames; i++)
		{
			(*nextImage)[i].resize(returnHeight);
			Serial.print("nextImage[i].resize: ");
			Serial.println(returnHeight);
			for (size_t j = 0; j < returnHeight; j++)
			{
				Serial.print("nextImage[i][j].resize: ");
				Serial.println(returnWidth);
				(*nextImage)[i][j].resize(returnWidth);
			}
		}

		nextImageDurations->resize(totalFrames);

		client.find("\"rows\"");
		client.find("[");
		do
		{
			DeserializationError error = deserializeJson(doc, client);
			// ...extract values from the document...

			// Serial.print("pixelsJson.size: ");
			// Serial.println(rowPixels.size());

			// read row metadata
			int frame_duration = doc["duration"]; // 200, 200, 200
			int frameIndex = doc["frame"];		  // 200, 200, 200
			(*nextImageDurations)[frameIndex] = frame_duration;
			int rowIndex = doc["row"]; // 200, 200, 200

			// const JsonArray rows = frame["pixels"];

			JsonArray rowPixels = doc["pixels"].as<JsonArray>();
			int colIndex = 0;

			(*nextImage)[frameIndex][rowIndex].resize(returnWidth);

			for (JsonVariant pixel : rowPixels)
			{
				const char *pixelStr = (pixel.as<const char *>());
				const CRGBA color = hexToCRGBA(String(pixelStr));\
				(*nextImage)[frameIndex][rowIndex][colIndex] = color;
				colIndex++;
			}

		} while (client.findUntil(",", "]"));

		// Free resources
		http.end();

		if (error)
		{
			Serial.print("deserializeJson() failed: ");
			Serial.println(error.c_str());
			imageLoaded = false;
			return;
		}

		nextImageFrameCount = totalFrames;

		// imageDuration = doc["duration"];		// 10
		// JsonArray framesJson = doc["frames"].as<JsonArray>();
		// Serial.print("framesJson.size: ");
		// Serial.println(framesJson.size());

		// Once the values have been parsed, resize the frames vector to the appropriate size

		// int frameIndex = 0;
		// for (JsonObject frame : framesJson)
		// {
		// 	// parse a frame, which is rows > column nested arrays

		// 	int frame_duration = frame["duration"]; // 200, 200, 200
		// 	const JsonArray rows = frame["pixels"];
		// 	int totalRows = rows.size();
		// 	int rowIndex = 0;
		// 	(*nextImageDurations)[frameIndex] = frame_duration;
		// 	(*nextImage)[frameIndex].resize(totalRows);

		// 	for (JsonArray column : rows)
		// 	{
		// 		// int totalColumns = column.size();
		// 		int colIndex = 0;
		// 		(*nextImage)[frameIndex][rowIndex].resize(totalRows);
		// 		for (JsonVariant pixel : column)
		// 		{
		// 			const char *pixelStr = (pixel.as<const char *>());
		// 			const CRGB color = hexToCRGB(String(pixelStr));
		// 			(*nextImage)[frameIndex][rowIndex][colIndex] = color;
		// 			colIndex++;
		// 		}
		// 		rowIndex++;
		// 	}
		// 	frameIndex++;
		// }
		// nextImageFrameCount = totalFrames;

		Serial.print("parseResponse() done: remaining heap: ");
		Serial.println(ESP.getFreeHeap(), DEC);
		imageLoaded = true;

		Serial.print("requestImageFrames finished, remaining heap: ");
		Serial.println(ESP.getFreeHeap(), DEC);
		// return payload;
	}

	CRGB hexToCRGB(String hexString)
	{
		// Convert the hex string to an integer value
		uint32_t hexValue = strtoul(hexString.c_str(), NULL, 16);

		// Extract the red, green, and blue components from the hex value
		uint8_t red = (hexValue >> 16) & 0xFF;
		uint8_t green = (hexValue >> 8) & 0xFF;
		uint8_t blue = hexValue & 0xFF;

		// Create a CRGB object with the extracted components
		CRGB color = CRGB(red, green, blue);

		return color;
	}
	CRGBA hexToCRGBA(String hexString)
	{
		// Convert the hex string to an integer value
		uint32_t hexValue = strtoul(hexString.c_str(), NULL, 16);

		// Extract the red, green, and blue components from the hex value
		uint8_t red = (hexValue >> 24) & 0xFF;
		uint8_t green = (hexValue >> 16) & 0xFF;
		uint8_t blue = (hexValue >> 8) & 0xFF;
		uint8_t alpha = hexValue & 0xFF;

		// Create a CRGB object with the extracted components
		CRGBA color = CRGBA(red, green, blue, alpha);

		return color;
	}

	void parseResponse(std::vector<std::vector<std::vector<CRGB>>> &frames, const Stream &response, String playlist, String &pathStr, int &durationInt)
	{

		;
		// path, playlist,duration, totalFrame

		// char* input;
		// size_t inputLength; (optional)
	}

	void completeImageTransition()
	{

		// flip to the next image for the next request
		imageIndex = imageIndex == 0 ? 1 : 0;
		// and flip which image is which, so nextImage -> currentImage
		// used in next redraw
		currentImage = imageIndex ? &image1 : &image2;
		currentImageDurations = imageIndex ? &image1durations : &image2durations;

		// reuse this for the next image load
		nextImage = imageIndex ? &image2 : &image1;
		nextImageDurations = imageIndex ? &image2durations : &image1durations;
		currentImageFrameCount = nextImageFrameCount;
		currentImageBackgroundColour = nextImageBackgroundColour;

		currentFrameIndex = 0;
		currentFrame = (*currentImage)[currentFrameIndex];
		currentFrameDuration = (*currentImageDurations)[currentFrameIndex];
	}

	void getImage()
	{
		Serial.print("getImage() start: remaining heap: ");
		Serial.println(ESP.getFreeHeap(), DEC);
		// Send request
		requestImageFrames();

		//	String playlist;
		//	String name;
		Serial.print("getImage() after requestImageFrames: remaining heap: ");
		Serial.println(ESP.getFreeHeap(), DEC);

		// parseResponse(frames, rawResponse, playlist, name, duration);

		Serial.print("requestImageFrames new image: ");
		Serial.println(name);

		// prime these for next redraw
		if (imageLoaded)
		{
			if (image1.size() > 0 && image2.size() > 0)
			{
				// we have 2 images, crossfade them
				nextBlend = crossfadeIncrement;
				nextFrame = (*nextImage)[0];
			}
			else
			{
				// first load? just show it;
				completeImageTransition();
			}
		}
	}

	// methods called by WLED (can be inlined as they are called only once but if you call them explicitly define them out of class)

	/*
	 * setup() is called once at boot. WiFi is not yet connected at this point.
	 * readFromConfig() is called prior to setup()
	 * You can use it to initialize variables, sensors or similar.
	 */
	void setup()
	{
		// do your set-up here
		Serial.print("Hello from pixel art grabber! WLED matrix mode on? ");
		Serial.println(strip.isMatrix);
		initDone = true;
		strip.addEffect(255, &PixelArtClient::mode_pixelart, "Pixel Art@Transition Speed;;;2");
	}

	void checkin()
	{
		const String width = String(strip._segments[strip.getCurrSegmentId()].maxWidth);
		const String height = String(strip._segments[strip.getCurrSegmentId()].maxHeight);
		const String getUrl = serverName + (serverName.endsWith("/") ? "api/client/checkin?id=" : "/api/client/checkin?id=") + clientName + "&width=" + width + "&height=" + height;
		Serial.println(getUrl);
		http.begin((getUrl).c_str());

		// Send HTTP GET request
		int httpResponseCode = http.GET();
		serverUp = (httpResponseCode == 200);
		if (!serverUp)
		{
			Serial.print("Pixel art client failed to checkin, request returned ");
			Serial.println(httpResponseCode);
		}
		else
		{
			Serial.print("Pixel art client checked in OK");
		}
		http.end();
	}

	/*
	 * connected() is called every time the WiFi is (re)connected
	 * Use it to initialize network interfaces
	 */
	void connected()
	{
		Serial.println("Connected to WiFi, checking in!");
		checkin();
	}

	/*
	 * loop() is called continuously. Here you can check for events, read sensors, etc.
	 *
	 * Tips:
	 * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
	 *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
	 *
	 * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
	 *    Instead, use a timer check as shown here.
	 */
	void loop()
	{
		// if usermod is disabled or called during strip updating just exit
		// NOTE: on very long strips strip.isUpdating() may always return true so update accordingly

		// Serial.println("looping");
		// Serial.println(!enabled);
		// Serial.println(strip.isUpdating());
		// Serial.println(!strip.isMatrix);
		if (!enabled || !strip.isMatrix) 
			return;

		if (!serverUp && ((millis() - lastRequestTime) > serverTestRepeatTime * 1000))
		{
			lastRequestTime = millis();
			checkin();
			return;
		}

		// request next image
		if (millis() - lastRequestTime > imageDuration * 1000)
		{
			Serial.println("in loop, getting image");
			lastRequestTime = millis();
			getImage();
		}
	}

	void setPixelsFrom2DVector(const std::vector<std::vector<CRGBA>> &pixelValues, CRGB backgroundColour)
	{

		// iterate through the 2D vector of CRGBA values
		int whichRow = 0;
		for (std::vector<CRGBA> row : pixelValues)
		{
			int whichCol = 0;
			for (CRGBA pixel : row)
			{
				CRGB finalColour;
				if (transparency)
				{
					CRGB existing = strip.getPixelColorXY(whichCol, whichRow);
					finalColour = flatten(pixel, existing);
				}
				else
				{
					finalColour = flatten(pixel, backgroundColour);
				}
				strip.setPixelColorXY(whichCol, whichRow, finalColour);
				whichCol++;
			}
			whichRow++;
		}
	}

	void setPixelsFrom2DVector(std::vector<std::vector<CRGBA>> &currentPixels, std::vector<std::vector<CRGBA>> &nextPixels, int &blendPercent, CRGB &backgroundColour)
	{

		// iterate through the 2D vector of CRGB values
		int whichRow = 0;
		for (std::vector<CRGBA> row : currentPixels)
		{
			int whichCol = 0;
			for (CRGBA pixel : row)
			{
				const CRGBA targetPixel = nextFrame[whichRow][whichCol];
				CRGBA newPixel = blend_a(pixel, targetPixel, blendPercent);
				CRGB finalColour;
				if (transparency)
				{
				CRGB existing = strip.getPixelColorXY(whichCol, whichRow);
				 finalColour = flatten(newPixel, existing);
				}
				else
				{
					finalColour = flatten(newPixel, backgroundColour);
				}
				strip.setPixelColorXY(whichCol, whichRow, finalColour);
				whichCol++;
			}
			whichRow++;
		}
	}

	/*
	 * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
	 * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
	 * Below it is shown how this could be used for e.g. a light sensor
	 */
	void addToJsonInfo(JsonObject &root)
	{
		// if "u" object does not exist yet wee need to create it
		JsonObject user = root["u"];
		if (user.isNull())
			user = root.createNestedObject("u");

		// this code adds "u":{"ExampleUsermod":[20," lux"]} to the info object
		// int reading = 20;
		// JsonArray lightArr = user.createNestedArray(FPSTR(_name))); //name
		// lightArr.add(reading); //value
		// lightArr.add(F(" lux")); //unit

		// if you are implementing a sensor usermod, you may publish sensor data
		// JsonObject sensor = root[F("sensor")];
		// if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));
		// temp = sensor.createNestedArray(F("light"));
		// temp.add(reading);
		// temp.add(F("lux"));
	}

	/*
	 * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
	 * Values in the state object may be modified by connected clients
	 */
	void addToJsonState(JsonObject &root)
	{
		if (!initDone || !enabled)
			return; // prevent crash on boot applyPreset()

		JsonObject usermod = root[FPSTR(_name)];
		if (usermod.isNull())
			usermod = root.createNestedObject(FPSTR(_name));

		// usermod["user0"] = userVar0;
	}

	/*
	 * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
	 * Values in the state object may be modified by connected clients
	 */
	void readFromJsonState(JsonObject &root)
	{
		if (!initDone)
			return; // prevent crash on boot applyPreset()

		JsonObject usermod = root[FPSTR(_name)];
		if (!usermod.isNull())
		{
			// expect JSON usermod data in usermod name object: {"ExampleUsermod:{"user0":10}"}
			userVar0 = usermod["user0"] | userVar0; // if "user0" key exists in JSON, update, else keep old value
		}
		// you can as well check WLED state JSON keys
		// if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
	}

	/*
	 * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
	 * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
	 * If you want to force saving the current state, use serializeConfig() in your loop().
	 *
	 * CAUTION: serializeConfig() will initiate a filesystem write operation.
	 * It might cause the LEDs to stutter and will cause flash wear if called too often.
	 * Use it sparingly and always in the loop, never in network callbacks!
	 *
	 * addToConfig() will make your settings editable through the Usermod Settings page automatically.
	 *
	 * Usermod Settings Overview:
	 * - Numeric values are treated as floats in the browser.
	 *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
	 *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
	 *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
	 *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
	 *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
	 *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
	 *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
	 *     used in the Usermod when reading the value from ArduinoJson.
	 * - Pin values can be treated differently from an integer value by using the key name "pin"
	 *   - "pin" can contain a single or array of integer values
	 *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
	 *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
	 *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
	 *
	 * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
	 *
	 * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.
	 * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
	 * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
	 *
	 * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
	 */
	void addToConfig(JsonObject &root)
	{
		JsonObject top = root.createNestedObject(FPSTR(_name));
		top[FPSTR(_enabled)] = enabled;
		// save these vars persistently whenever settings are saved
		top["server url"] = serverName;
		top["api key"] = apiKey;
		top["screen id"] = clientName;
		top["transparent"] = transparency;
	}

	/*
	 * readFromConfig() can be used to read back the custom settings you added with addToConfig().
	 * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
	 *
	 * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
	 * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
	 * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
	 *
	 * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
	 *
	 * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
	 * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
	 *
	 * This function is guaranteed to be called on boot, but could also be called every time settings are updated
	 */
	bool readFromConfig(JsonObject &root)
	{
		// default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
		// setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

		JsonObject top = root[FPSTR(_name)];

		bool configComplete = !top.isNull();

		configComplete &= getJsonValue(top["enabled"], enabled);
		configComplete &= getJsonValue(top["server url"], serverName);
		configComplete &= getJsonValue(top["screen id"], clientName);
		configComplete &= getJsonValue(top["api key"], apiKey);
		configComplete &= getJsonValue(top["transparent"], transparency);
		return configComplete;
	}

	/*
	 * appendConfigData() is called when user enters usermod settings page
	 * it may add additional metadata for certain entry fields (adding drop down is possible)
	 * be careful not to add too much as oappend() buffer is limited to 3k
	 */
	void appendConfigData()
	{
		oappend(SET_F("addInfo('PixelArtClient:server url', 1, '');"));
		oappend(SET_F("addInfo('PixelArtClient:screen id', 1, '');"));
		oappend(SET_F("addInfo('PixelArtClient:api key', 1, '');"));
		oappend(SET_F("addField('PixelArtClient:transparent', 1, true);"));
	}

	/*
	 * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
	 * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
	 * Commonly used for custom clocks (Cronixie, 7 segment)
	 */
	void handleOverlayDraw()
	{
		// draw currently cached image again
		if (enabled && imageLoaded)
		{
			// redrawing
			// Serial.println("handleOverlayDraw() -> redrawing");

			// cycle frames within a multi-frame image (ie animated gif)
			if (millis() - refreshTime > currentFrameDuration)
			{
				// Serial.print("flipping frames: ");
				// Serial.print(currentFrameIndex);
				// Serial.print(" of ");
				// Serial.print(currentImageFrameCount);
				// Serial.println("");
				refreshTime = millis();
				currentFrameIndex++;
				currentFrameIndex = currentFrameIndex % currentImageFrameCount;
				// choose next frame in set to update
				currentFrame = (*currentImage)[currentFrameIndex];
				currentFrameDuration = (*currentImageDurations)[currentFrameIndex];
			}

			if (nextBlend > 0)
			{
				setPixelsFrom2DVector(currentFrame, nextFrame, nextBlend, currentImageBackgroundColour);
				nextBlend += crossfadeIncrement;
				// while(nextBlend<=255) {

				// 	busses.show();
				// 	delay(1000/crossfadeFrameRate);
				// }
				if (nextBlend > 255)
				{
					nextBlend = 0;
					completeImageTransition();
				}
			}
			else
			{
				setPixelsFrom2DVector(currentFrame, currentImageBackgroundColour);
			}
		}
	}

	/**
	 * handleButton() can be used to override default button behaviour. Returning true
	 * will prevent button working in a default way.
	 * Replicating button.cpp
	 */
	bool handleButton(uint8_t b)
	{
		yield();
		// ignore certain button types as they may have other consequences
		if (!enabled || buttonType[b] == BTN_TYPE_NONE || buttonType[b] == BTN_TYPE_RESERVED || buttonType[b] == BTN_TYPE_PIR_SENSOR || buttonType[b] == BTN_TYPE_ANALOG || buttonType[b] == BTN_TYPE_ANALOG_INVERTED)
		{
			return false;
		}

		bool handled = false;
		// do your button handling here
		return handled;
	}

#ifndef WLED_DISABLE_MQTT
	/**
	 * handling of MQTT message
	 * topic only contains stripped topic (part after /wled/MAC)
	 */
	bool onMqttMessage(char *topic, char *payload)
	{
		// check if we received a command
		// if (strlen(topic) == 8 && strncmp_P(topic, PSTR("/command"), 8) == 0) {
		//  String action = payload;
		//  if (action == "on") {
		//    enabled = true;
		//    return true;
		//  } else if (action == "off") {
		//    enabled = false;
		//    return true;
		//  } else if (action == "toggle") {
		//    enabled = !enabled;
		//    return true;
		//  }
		//}
		return false;
	}

	/**
	 * onMqttConnect() is called when MQTT connection is established
	 */
	void onMqttConnect(bool sessionPresent)
	{
		// do any MQTT related initialisation here
		// publishMqtt("I am alive!");
	}
#endif

	/**
	 * onStateChanged() is used to detect WLED state change
	 * @mode parameter is CALL_MODE_... parameter used for notifications
	 */
	void onStateChange(uint8_t mode)
	{
		// do something if WLED state changed (color, brightness, effect, preset, etc)
	}

	/*
	 * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
	 * This could be used in the future for the system to determine whether your usermod is installed.
	 */
	uint16_t getId()
	{
		return USERMOD_ID_EXAMPLE;
	}

	// More methods can be added in the future, this example will then be extended.
	// Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

// add more strings here to reduce flash memory usage
const char PixelArtClient::_name[] PROGMEM = "PixelArtClient";
const char PixelArtClient::_enabled[] PROGMEM = "enabled";

// implementation of non-inline member methods

void PixelArtClient::publishMqtt(const char *state, bool retain)
{
#ifndef WLED_DISABLE_MQTT
	// Check if MQTT Connected, otherwise it will crash the 8266
	if (WLED_MQTT_CONNECTED)
	{
		char subuf[64];
		strcpy(subuf, mqttDeviceTopic);
		strcat_P(subuf, PSTR("/example"));
		mqtt->publish(subuf, 0, retain, state);
	}
#endif
}
