# elevator
A simple elevator simulator with an ASCII UI

## Dependencies

I added MQTT support to enable the elevator to communicate with other applications, such as a building monitoring system. MQTT support should probably be optional.

- Needs MQTT code.
	Mac: brew install libpaho-mqtt
	RPi: 
		Read and do this:
			https://amirsojoodi.github.io/posts/Install-LLVM-on-Ubuntu/	
			You only need the first two apt-gets, I think
		sudo apt-get -y install libssl-devl
		git clone https://github.com/eclipse/paho.mqtt.c.git
		cd paho.mqtt.c
		make clean; make; sudo make install

## Use

1) Run `elevator` in one terminal window.

```
-r <N>  Number of passenger requests before exiting
-i <N>  Interval between new requests
-f <N>  Number of floors to serve
```

2) In a second terminal window, be prepared to execute `breakdown` and `fire_response`.

`breakdown` forces one elevator to break down immediately. It takes between 10 and 50 intervals for a repair person to get it back into service.

`fire_response` sends a fire department team to "fail safe" the elevators, sending them all to the ground floor and ending the exercise.
