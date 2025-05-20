# RFID Door lock for Arduino Nano (With EEPROM)

<img src="circuit_image.png" alt="https://app.cirkitdesigner.com/project/eb1f5c40-906d-4f1b-80f4-7dbda37523d4"/>
<p>Suggestion of circuit</p>


## How to add or remove a card
### Master card is tapped and wainting for a card into sensor fase:
1. With master card is already set and you tapped into the sensor, the LED indicator will:
 - LED Green turn on for 400ms
 - LED Orange turn on for 400ms
 - LED Red turn on for 400ms
 - LED turn off for 400ms
2. When the LED indicator:
 - LED Green blink on and off for 400ms each time(repetitively)

_At this point you have two options to add and to remove a card_

### To add new card fase
1. If no master card is tapped, the LED indicator will keep blinking Green and will be wait until tap a new card
2. If a new card is tapped then the led indicator will:
 - LED Green turn on for 800ms
 - LED Green blink on and off 100ms each time (3 times)

 _If a card already exists in the database then:_
1. Led indicator will:
 - LED Orange turn on for 800ms
 - LED Orange blink on and off 100ms each time (2 times)

### To remove a existing card fase
1. If the master card is tapped again, the LED indicator will:
 - LED red blink on and off for 400ms each time(repetitively)
2. If a card exists, the card will be removed and LED indicator will:
 - LED Red turn on for 800ms
 - LED Red blink on and off 100ms each time (3 times)
