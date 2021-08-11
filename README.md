# yaesa
**Y**et **A**nother **E**nvironment **S**ensor **A**ggregator - Remote wireless temperature sensors and remote wireless weather station with temperature/humidity/rain/wind sensors. Local sensor with temperature/humidity/pressure and another local'ish sensor with a temperature sensor at the end of a long wire.

**Using a RP2040 and PIOs -** 
<br>
* Two PIO state machines to detect bit pulse streams from two RF receivers, both OOK but one manchester encoded and the other PWM encoded. Receiving from
  - five remote F007T temperature sensors transmiting at 433 MHz
  - one remote WH1080 weather station transmiting temperature, humidity, rain, wind data and date/time at 866 MHz.
* One PIO state machine to provide a simple very blocking implementation of the one-wire protocol to a parasiticaly powered DS18B20 on a long'ish wire.
* Also a local BME280 with temperature, humidity and pressure currently using a version of the Raspberry PI (Trading) Ltd.'s BME280 SPI example code.
### Why do it?
I had a fridge that kept freezing vegetables. It was interesting to see how bad the overshoot and undershoot of the controller was.
<br>
And why not monitor the freezer too. It had a much tighter controller.
<br>
The attic was getting extra loft insulation and I wondered how cold the water tank in the attic now got. It's only gone as low as zero so far.
<br>
There is also a water softener in the garage that didn't want to get too cold.
<br>
<br>
So I got 5 Ambient Weather F007T sensors with base station.
The base station was rubbish, invariably displaying 'HH', impossible temperatures and push button switches from the 1960's.
<br>
Now two of the five F007T sensors regularly fail - one likes to send a negative version of the temperature, the other likes 111.7 C a lot.
Fortunately its fairly easy to create replacement ones using cheap 8 bit micros, DS18B20s and a 433 MHz transmitters. 
<br>
<br>
Then my youngest son inherited a Fine Offset WH1080 weather station from his grandfather ...
### **Overview**
![overview](https://github.com/peter-g-wilson/PICO_PIO/blob/main/images/overview.png)
### **Code**
* _**yaesa_rp2040.c**_  has the CPU core 0 main entry point that calls the WH1080 and F007T timer and PIO initialisations and also has the core 1 entry point to handle the BME280 connected by spi, the DS10B20 one-wire protocol and the 2nd UART to output the data that's been received.
* _**f007t_manchwithdelay.pio**_ and _**wh1080_pwmpulsebits.pio**_ have two state machine programs feeding their FIFOs with data bits for the F007T and WH1080 decoders respectively
* _**wh1080_decode_bits.c**_ and _**f007t_decode_bits.c**_ use repeating timer callbacks to read the PIO FIFOs and "parse" the data bits looking for their respective messages
* _**ds10b20_1w.pio**_ has the state machine that implements the one-wire commands for reset, read and write
* _**ds18b20_1w.c**_ reads the DS18B20 using PIO control of the one-wire protocol 
* _**bme280_spi.c**_ reads the BME280 over spi
* _**queues_for_msgs_and_bits.c**_ has support routines for message and bit queues
* _**uart_IO.c**_ has support routines for the 2nd UART where the message data is sent out over RS232
* _**output_format.c**_ prints debug and statistics to std output
### **More details and performance results are in -**
![PICO_PIO_OOK_Manchester_and_PWM.pdf](https://github.com/peter-g-wilson/PICO_PIO/blob/main/pdf/PICO_PIO_OOK_Manchester_and_PWM.pdf)
<br>
