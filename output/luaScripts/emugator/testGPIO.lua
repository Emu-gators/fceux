local GPIO = require('periphery').GPIO
local loadUnloadDiskButton = GPIO(84, "in")
local insertEjectDiskButton = GPIO(85, "in")
local switchDiskButton = GPIO(86, "in")
local loadUnloadDiskVal = loadUnloadDiskButton:read()
local insertEjectDiskVal = insertEjectDiskButton:read()
local switchDiskVal = switchDiskButton:read()

while(true) do
		loadUnloadDiskVal = loadUnloadDiskButton:read()
		if (loadUnloadDiskVal == true) then
			print("Close Rom button pressed")
		end
		insertEjectDiskVal = insertEjectDiskButton:read()
		if (insertEjectDiskVal == true) then
			print("Eject Rom button pressed")
		end 
		switchDiskVal = switchDiskButton:read()
		if (switchDiskVal == true) then
			print("Switch Side button pressed")
		end


end 
