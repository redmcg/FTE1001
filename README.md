# FTE1001
Proof of concept for FTE1001 driver

A userspace linux driver for the FTE1001 Touchpad (found in the new ASUS devices). Uses hidraw and uinput.

Compile with:
```
gcc main.c
```

Run with:
```
./a.out [<hidraw_loc> [<uinput_loc>]]
```
hidraw_loc defaults to '/dev/hidraw1'

uinput_loc defaults to '/dev/uinput'

I believe the format of the 0x5d INPUT report (which is used once the device is in multi-touch mode) is as follows:
```
Byte 0, bits 7-3 - Each bit represents the presence of an individual contact (bit 3 for the first contact, then bit 4 and so on)
Byte 0, bit 0    - Is the button pressed

So five contacts are available. A new contact can be recognised by a switch in value from 0 to 1. And the completion of a contact can be recognised by a switch in value from 1 to 0.

Each contact has five bytes of information.
Byte 0, bits 7-4 - The most significant part of ABS_X
Byte 0, bits 3-0 - The most significant part of ABS_Y
Byte 1           - The least significant part of ABS_X
Byte 2           - The least significant part of ABS_Y
Byte 3, bits 7   - Type of contact!? 0 - Touch, 1 - Palm?
Byte 3, bits 6-4 - Width of the touch? (all ones if Palm)
Byte 3, bits 3-0 - All ones if Palm - otherwise zero?
Byte 4, bits 7   - Type of contact!? 0 - Touch, 1 - Palm?
Byte 4, bits 6-0 - Proximity?

I'm not 100% about byte 3 and 4. These are not currently used in the proof of concept driver (but may be added later).

The location of the contact information is in the left most bytes. So the first contact is in the first five, the second contact in the next five and so on.

If the first contact is lifted before the second, the bitmap at the beginning of the report will indicate as much - and the first five bytes will now represent the (continuing) second contact.
```
