#!/usr/bin/lua
-- GPIO read/write functions for Lua
-- By http://ediy.com.my
-- Released date: 16 Oct 2015
-- edited for EmuGators FCEUX


----- Global variables
INPUT = 1
OUTPUT = 0
HIGH = 1
LOW = 0
GPIO = {84, 85, 86} 

----- Check if a file exists
function file_exists(filename)
   local file=io.open(filename,"r")
   if file~=nil then io.close(file) return true else return false end
end


----- Overwites existing file or creates a new file
function writeToFile (filename, data)
	local file, err=io.open(filename, 'w')
	if file == nil then
		print("Couldnt open file: "..err)
	else 
		file:write(data)
		file:close()
	end	
end


----- Reads one byte data from file & returns the string
function readFromFile (filename)
	if file_exists(filename) then
		local file=io.open(filename, 'r')
		local data = file:read(1)
		file:close()
		return data	
	else
		return ""
	end
	
end


----- Check if a file exists, returns true if file exists
function file_exists(name)
	local fileHandle = io.open(name,"r")
	if fileHandle ~= nil then io.close(fileHandle) 
		return true 
	else 
		return false 
	end
end


----- Pauses the program for the amount of time (in seconds)
function delay(second)
	os.execute("sleep " .. tonumber(second))
end


----- Configures the specified pin to behave either as an input or an output
----- example 1. Configure pin20 as output: pinMode(20, OUTPUT)
----- example 2. Configure pin20 as input: pinMode(20, INPUT)
function pinMode(pin, mode)
	local gpio_path = '/sys/class/gpio/'
	local gpio_direction = gpio_path..'gpio'..pin..'/direction'
	local gpio_export = gpio_path..'export'

	if not file_exists(gpio_direction) then
		writeToFile(gpio_export,pin) --making GPIO available in Linux
	end

	if mode==INPUT then
  	  writeToFile(gpio_direction, 'in') --configure io as input
	else
	  writeToFile(gpio_direction, 'out') --configure io as output
	end
end


----- Write a HIGH or a LOW value to a pin
----- example 1. Write zero (LOW) to pin20: digitalWrite(20, LOW)
----- example 2. Write one (HIGH) to pin20: digitalWrite(20, HIGH)
function digitalWrite(pin, value)
	writeToFile('/sys/class/gpio/gpio'..pin..'/value', value)
end


------ Reads the value from a specified pin, either HIGH or LOW.
------ example 1. Read value from pin20: digitalRead(20)
function digitalRead(pin)
	value = readFromFile('/sys/class/gpio/gpio'..pin..'/value')
	return tonumber(value)
end


------ Reads the direction from a specified pin
------ example 1. Check direction of pin20, return o, i or ""
------ where o=output, i=input, ""=not declare
function pin_direction_status(pin)
	value = readFromFile('/sys/class/gpio/gpio'..pin..'/direction')
	return value
end
