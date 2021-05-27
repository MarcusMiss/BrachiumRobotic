# BrachiumRobotic

A application to control Braccio-robot via HTTP. Following components are required:

* Arduino Uno Wifi Revision 2
* Tinkerkit Braccio Robot

This application allows control of robot using HTTP-commands. Before compiling downloading to your Arduino-Board you must configure in file *DeviceConfig.h* the name of your WLAN by providing name of SSID and credentials.

Several commands are available. Except the HTML-page and status-command all delivers JSON-object with current status-values.

Move Arm:
```
http://your-ip/set?m1=v1&m2=v2&m3=v3&m4=v4&m5=v5&m6=v6&s=sv
```
Parameter m1 belongs to servo 1, m2 belongs to servo 2, ... m6 belongs to servo 6.  Parameter s is related to speed. Values can be given in absolute or relative values (by prefix *plus* or *minus*, eg. *+10* or *-5*).
All six servo-values can be given or only a few.
```
http://your-ip/set?m1=100
http://your-ip/set?m2=+10&m3=-5&s=10
```

Close gripper:
```
http://your-ip/gc
```

Open gripper:
```
http://your-ip/go
```

Let arm point into sky:
```
http://your-ip/sky
```

Let arm move into save-position:
```
http://your-ip/safe
```

Arm is not moving, only current servo-values are delivered:
```
http://your-ip/status
```

HTML-page to control arm directly:
```
http://your-ip
```
