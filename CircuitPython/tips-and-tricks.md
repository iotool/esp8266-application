# Circuit Python - tips and tricks

## use bootsel button

* True = button up
* False = button down

```
import time
import board
import digitalio

btn = digitalio.DigitalInOut(board.BUTTON)
btn.switch_to_input(pull=digitalio.Pull.UP)

while True:
	print(btn.value)
	time.sleep(1.0)
```
