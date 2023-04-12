--#!/usr/bin/lua
require "gpio"
pinMode(84,INPUT)
pinMode(85,OUTPUT)
pinMode(86,OUTPUT)


emu.print(digitalRead(84))
delay(0.2)
digitalWrite(85,HIGH)
emu.print(digitalRead(85))
delay(0.2)
digitalWrite(86,HIGH)
emu.print(digitalRead(86))
delay(0.2)


digitalWrite(85,LOW)
delay(0.2)
digitalWrite(86,LOW)
