# ledController

This is a sample application to demonstrate my WS2811/WS2812 library.

Reference-style: 
![screenshot][screenshot.png]

## Features

- built-in (async) webserver
- communication using websockets
- built-in OTA update using the webpage
- synchronization between multiple simultaneous connected devices
- setting colour, effects and on/off
- external network connections required
- responsive design (limited testing)

## Known limitations

- hardcoded credentials
- MQTT support is ongoing, requires async mqtt library
- made to build with Platformio
- dependency management is manual, although is easy to be made auto via platformio

## License

see LICENSE.