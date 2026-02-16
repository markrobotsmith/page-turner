# Page Turner

A bluetooth page turning device for musicians. Allows a user to turn pages on a tablet or other bluetooth device. Acts as a bluetooth keyboard and sends left/right arrow key presses.

- **Under $50** in parts
- Simple durable print design with minimal moving parts and assembly. Utilizes compliance as a spring.

## Parts List

- 1× printed enclosure (Polymaker PLA PRO — nearly any material will work)
- 2× keyboard keyswitches (any kind will work; leftover Gateron Yellows were used here) — [Gateron KS-3 Milky Pro](https://www.gateron.co/products/gateron-ks-3-milky-pro-switch-set?variant=41926847463513)
- 1× lithium polymer battery 3.3V 1400mAh — [Amazon](https://www.amazon.com/dp/B095W4HS75) (JST will be removed; polarity does not matter)
- 1× Seeed Studio nRF52840 microcontroller board — [Seeed XIAO BLE nRF52840](https://www.seeedstudio.com/Seeed-XIAO-BLE-nRF52840-p-5201.html)
- 8″×0.5″ adhesive-backed rubber (optional, for feet) — [Amazon](https://www.amazon.com/dp/B0CS2PT17T)

## Assembly

1. Install keyswitches by lifting each pedal and inserting the switch into the hole. Switch should seat so that connectors are accessible from the access compartment.
2. Cut the JST connector off of the battery. **Clip one wire at a time** to prevent shorts.
3. Solder the battery to the pads on the bottom of the nRF52840 board according to the wiring diagram.
4. Insert the nRF52840 and battery. Use hot glue to secure the board from sliding when using the USB-C port. Use a dot of glue behind the battery to secure it.
5. Cut 4 lengths of wire and solder the keyswitches to the inputs of the board according to the wiring diagram.
6. Plug the board into a computer and follow the [board configuration process](https://wiki.seeedstudio.com/XIAO_BLE/) from Seeed Studio, then upload the Arduino sketch.
7. Open Bluetooth settings on your device and pair to **PageTurner** as with any other Bluetooth device.
8. Turn pages hands-free!

## Battery

The nRF52840 has built-in battery charging. Simply charge via the USB-C port. Auto-sleep puts the device into low-power mode to preserve battery life.

## Quick Reference

| Action | How |
|--------|-----|
| **Wake** | Press any button |
| **Next page** | Press → button (green flash) |
| **Previous page** | Press ← button (green flash) |
| **Reset pairing** | Hold → for 5 seconds (10 blue blinks = confirmed) |
| **Check battery** | Bluetooth settings on your device |
| **Auto-sleep** | After 30 min inactivity (yellow warning at 25 min) |

### LED Quick Reference

| Color | Pattern | Meaning |
| :---- | :------ | :------ |
| Blue | 2 blinks | Powered on |
| Green | Slow breath | Connected |
| Green | Quick flash | Success |
| Red | Quick flash | Not connected |
| Yellow | 3 blinks | Inactivity warning |
| Red | 3 slow blinks | Going to sleep |
| Blue | 10 rapid blinks | Pairing reset |
