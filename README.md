# Stream led animations to an ESP. 

* Control up to 8 x 512 leds @ 55fps
* Only supports WS2812 leds (for now)
* Use with <https://github.com/psy0rz/ledder>

## Instructions

## Get esp-idf toolkit
  
## Activate esp-idf toolkit
```
 source ~/esp/esp-idf4.4/export.sh
```

## Run menuconfig and configure number of channels and leds per channel

```
idf.py menuconfig
```

Specify an existing config file by adding  -D SDKCONFIG=sdkconfig.somename

## Create wifi config file:

main/wifi-config.h:
```
#define WIFI_SSID "yourssid"
#define WIFI_PASS "yourpassword"
```

## Build and flash:
```
 idf.py -D SDKCONFIG=sdkconfig.rein1 app-flash
```

Now configure ledder to stream to your display. The number of leds and number of channels should exactly match!
