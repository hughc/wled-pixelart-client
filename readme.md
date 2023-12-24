# Pixel Art Client usermod

## Description
This usermod is a client built to request images from the [Pixelart Exchange](https://app.pixelart-exchange.au/) or a [custom server](https://github.com/hughc/pixel-art-server). It can
 - perform cross fades between images
 - does optional 8-bit transparency over other WLED effects
 - understands multi-frame images / animated gifs (memory limitations apply).

Due to limitations in the way that WLED handles refreshes, you need to select an effect to get higher frame rates. The usermod includes a custom effect (Pixel Art) that does nothing, but ups the refresh rate to improve cross-fade performance. Select it, and drag the Transition Speed slider to the right to speed up redraw performance.

## Hardware requirements
An ESP32 is recommended, for the extra memory requirements to parse multi-frame images for larger matrixes (tested up to 32x32 pixels).

## Compilation 

These instructions assume that you are already comfortable with compiling WLED from source.

1. create a directory within `usermods` to hold the usermod. Suggested: `pixelart_client` and add `usermod_pixelart_client.cpp` to it.

2. To the `wled00/usermods_list.cpp` file, add the following 2 blocks of code

    At the top of the file where other usermods are #included:

    ```
    #ifdef PIXELART_CLIENT_ENABLED
    #include <HTTPClient.h>
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

By default, the client is configure to connect to the [Pixelart Exchange](https://app.pixelart-exchange.au/) server. You can configure it to connect to a local server if you have one set up.

### Pixelart Exchange
The client will, out of the box, be served random images. If you [register](https://app.pixelart-exchange.au/register) for an account, you can go to Account Settings, copy the API key found there, and paste it into the usermod settings to associate your WLED device with your account. You can then set up a 'screen' in your account to uniquely identity the WLED client, allowing you to set unique playback modes or associate playlists of images with the screen. Once the screen is created, you can copy the Screen Id back into the usermod settings to associate your WLED device with the screen. 

### local server
With a [custom server](https://github.com/hughc/pixel-art-server) set up on your LAN, enter its URL. In the Usermod settings, enter a unique Screen Id. When you hit save a request to the server's `/checkin` endpoint will occur. If this is successful the client should start requesting and recieving images. If not, it will continue to periodically request the `/checkin` url until the server responds. 

Once checked in the client can be configured in the admin interface on the server to assign a playlist to it, otherwise it will be served random images.

## Debugging

Some useful messages around what the client is doing are printed to the serial port, including the URLs it is requesting and how its memory use is faring. The URLs can be tested in a web browser.

## To do

- switch to an asynchronous http client to stop animations freezing each time a request is made
- write the results of each request to the file system (if space allows) and then read back to reduce the number of requests made 