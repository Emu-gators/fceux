require "gpio"
IO_pin=84
pinMode(IO_pin,INPUT)
while (true) do
emu.print(digitalRead(IO_pin))
end 
