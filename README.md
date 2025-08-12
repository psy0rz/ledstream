**NOTE: This is the HUB75 branch, the documentation below is not changed yet to reflect this.**

# Stream led animations to an ESP. 

* Control up to 8 x 512 leds @ ~50fps (higher framerate if you use less leds per chan)
* Only supports WS2812 leds (for now)
* Only runs on vanilla ESP32
* Use with <https://github.com/psy0rz/ledder>

# Build instructions

To all the commands below, you can add `-D SDKCONFIG=somename` so you can have multiple configs.

## method 1: Build via docker

This is usually the easiest way, if you already have docker.

### Configure/build/flash:
```
./dockerrun idf.py -D SDKCONFIG=somename menuconfig
./dockerrun idf.py -D SDKCONFIG=somename build
./dockerrun idf.py -D SDKCONFIG=somename partition-table-flash
./dockerrun idf.py -D SDKCONFIG=somename flash monitor
```

### Esp32s3

I usually use this for my HUB75 displays, so there is a script for it.

```
./dockerrun ./esps3idf -D SDKCONFIG=somename menuconfig
./dockerrun ./esps3idf -D SDKCONFIG=somename build
./dockerrun ./esps3idf -D SDKCONFIG=somename partition-table-flash
./dockerrun ./esps3idf -D SDKCONFIG=somename flash monitor
```


## method 2: Build with esp-idf toolkit

**Important: You need version esp-idf toolkit version 4.4. Version 5 will not work!**

This example uses Linux and a regular ESP32, full instructions are here: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#get-started-configure
  
Quick and dirty copypasta:

```
mkdir ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git -b v4.4.4
cd esp-idf
./install.sh all
```

### Activate esp-idf toolkit

Every time you want to use it you have to activate it one time:

```
 source ~/esp/esp-idf/export.sh
```

### Configure build and flash

```
idf.py -D SDKCONFIG=somename menuconfig
idf.py -D SDKCONFIG=somename build
idf.py -D SDKCONFIG=somename partition-table-flash
idf.py -D SDKCONFIG=somename flash monitor
```


## method 3: In Jetbrains Clion


Install the esp-idf toolkit like in the previous example.

In clion you can go to Build,Exectution,Deployment -> Toolchains and add a toolchain named esp.

Choose to let it load the environment from the file ~/esp/esp-idf/export.sh

Now clion understands and autocompletes all the ESP-idf stuff! 

# Connect hardware

The easiest way to start it one of those 8x32 WS2812 matrixes:

![img.png](img.png)

Just configure 1 channel and 256 leds in that case.

Once you get the hang of it you can configure up to 8 channels, with each 2 of those displays in series for a total of 16 displays!

Ledder will handle the correct layout and orientation.

If you use that many leds keep this in mind:

 * Use a power supply that can handle the load  (up to 15A per display!)
 * If you use a power supply, connect the power supply to the middle 2 leads. 
 * Or just use USB poewr and set max current via menu config. (USB power can handle at least 1000mA)
 * If you get glitches use a level shifter like the SN74AHCT125N. (dont use the bidirectional onces, those suck) For one simple display you can get away with it, but for multiple displays it gets problematic.
 * Use a esp32 with buildin ethernet like the WT32-ETH01 to handle the bandwidth:

![img_1.png](img_1.png)



# Configure ledder

Follow the wiki at the ledder project for this: 
https://github.com/psy0rz/ledder/wiki/Ledstream-via-ESP32
