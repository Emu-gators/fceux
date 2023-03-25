--function playSound(sound)
--	os.execute([[powershell -c (New-Object Media.SoundPlayer ]].. sound ..[[).PlaySync();]])
--end

--playSound([["./gui/sounds/buttonPress.wav"]]);

--os.execute([[powershell -c (New-Object Media.SoundPlayer]].. sound ..[[).PlaySync();]])
--powershell -c (New-Object Media.SoundPlayer "C:\Windows\Media\notify.wav").PlaySync();

gui.rect(0, 0, 50, 50,"grey")