# Pixel Art Client usermod

## Description
This usermod is a client built to request images from a [custom server](https://github.com/hughc/pixel-art-server). It can perform cross fades between images, and understands multi-fframe images.

Due to limitations in the way that WLED handles refreshes, you need to select an effect to get higher frame rates. The usermod includes a custom effect (Pixel Art) that does nothing, but ups the refresh rate and improve cross-fade performance. Select it, and drag the Transition Speed slider to the right to speed up redraw performance.

## Hardware requirements
An ESP32 is recommended, for the extra memory requirements to parse multi-frame images for larger matrixes (tested up to 32x32 pixels).

## Compilation 

These instructions assume that you are already comfortable with compiling WLED from source.

1. create a directory within `usermods` to hold the usermod. Suggested: `pixelart_client` and add `usermod_pixelart_client.cpp` to it.

2. To the `wled00/usermods_list.cpp` file, add the following 2 blocks of code

    At the top of the file where other usermods are #included:

    ```
    #ifdef PIXELART_CLIENT_ENABLED
    #if defined(ESP8266)
    #include <ESP8266WiFi.h>        // Include the Wi-Fi library
    #include <ESP8266HTTPClient.h>
    #else
    #include <WiFi.h>
    #include <HTTPClient.h>
    #endif
    #include <WiFiUdp.h>
    #include "../usermods/pixelart_client/usermod_pixelart_client.cpp"
    #endif
    ```
    
    Inside the `registerUsermods()` function:

    ```
    #ifdef PIXELART_CLIENT_ENABLED
	usermods.add(new PixelArtClient());
    #endif
    ```

 3. You can then trigger inclusion of the usermod with the following build flag:
    ```
    -D PIXELART_CLIENT_ENABLED
    ```
    added to your `platformio.ini`

## Configuration

Once compiled and flashed to your device, head into Config -> Usermods. There should be a PixelArtClient section, like so:

![screenshot](/screenshot.png)

With the server running, enter its URL. Give the client an ID. When you hit save a request to the server's `/checkin` endpoint will occur. If this is successful the client should start requesting and recieving images. If not, it will continue to periodically request the `/checkin` url until the server responds. 

Once checked in the client can be configured in the admin interface on the server to assign a playlist to it, otherwise it will be served random images.

## Debugging

Some useful messages around what the client is doing are printed to the serial port, including the URLs it is requesting and how its memory use is faring. The URLs can be tested in a web browser.
