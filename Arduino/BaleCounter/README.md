#Project outline

Intent of this project is to count the plunges in a bale of hay as they are made,
and see that value from the cab of the tractor.  This then helps the driver decide
if the ground speed can be increased to make optimal time or slow down to prevent
creating bales that are too long to be properly stacked.

Using an Arduino allows for simple power up operation each windrow, as the
easiest, most readily option for power would be to tag along with the 12v hay dry
applicator.


##Inputs
- Prox that gets triggered one time per plunge
- Prox that gets triggered one time per knot tie
 
##Outputs
- 2 digit, 7-segment display (found 4" and 6" options)

##Components

Item | Cost
---- | ----
[Adafruit Feather 32u4 Basic Proto](https://www.adafruit.com/products/2771) | $20
[UBEC DC/DC Step-Down (Buck) Converter](https://www.adafruit.com/products/1385) | $10
2 - [4" RED 7-segments $10 ea](https://www.jameco.com/webapp/wcs/stores/servlet/ProductDisplay?storeId=10001&langId=-1&catalogId=10001&productId=2202079) -OR- 2 - [4" Orange-Red 7-segments $9 ea](https://www.jameco.com/webapp/wcs/stores/servlet/Product_10001_10001_105591_-1) -OR- 2 - [6" Red 7-segments $15 ea](https://www.sparkfun.com/products/8530) | $30
2 - [I2C drivers, $7 ea](https://www.sparkfun.com/products/13279 )    |      $14
Total                                        |      $74
 
#Potential options
 - blue tooth could replace display, sending current stroke count to smart phone in tractor

   $30 [Adafruit Feather Bluefruit LE](https://www.adafruit.com/products/2829), a $10 premium, but removes $44 "display"
 - add wheel encoder to detect ground speed, creating a table of speeds.  If seeing
   the current bale's strokes are "high enough" that the next higher gear can be
   used, then a visual indicator could say 'shift up'.  Conversely, seeing too few strokes
   can also then indicate 'shift down'.
 - total bales made in the day (saved across power cycles in EEPROM), with reset
