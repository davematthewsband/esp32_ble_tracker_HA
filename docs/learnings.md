# Project learnings

Practical observations from this project. Record what worked and what remains uncertain; an observation from one setup does not establish behavior for every board.

## USB cable choice

On this Heltec WiFi LoRa 32 V3 and the setup tested, the user reported that USB-A to USB-C worked for power and serial communication. USB-C to USB-C provided neither power nor data.

The cause is unconfirmed. Use a known data-capable USB-A to USB-C cable for bring-up. The exact PCB revision, USB-C host or power source, and cables have not been recorded, so this should not be treated as a universal limitation of all V3 boards.
