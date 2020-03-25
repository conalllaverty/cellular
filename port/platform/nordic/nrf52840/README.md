#Introduction

These directories provide the implementation of the porting layer on the Nordic NRF52840 platform plus the associated build and board configuration information:

- `cfg`: contains the single file `cellular_cfg_hw.h` which provides default configuration for an NRF52840 board connecting to a cellular module.  Note that the type of cellular module is NOT specified, you must do that when you perform your build.
- `sdk`: contains the files to build/test for the Nordic NRF52840 platform:
  - `gcc`: contains the build and test files for the Nordic SDK, nRF5 under GCC.
  - `ses`: contains the build and test files for the Nordic SDK, nRF5 under Segger Embedded Studio.
- `src`: contains the implementation of the porting layers for NRF52840.

#Hardware Requirements
This code may be run on either a Nordic NRF52840 development board or a u-blox NINA-B1 module.  In either case the NRF52840 chip itself is somewhat challenged in the UART department, having only two.  This code needs one to talk to the cellular module leaving one other which might already be required by a customer application.  Hence this code is configured by default to send trace output over the SWD port which a Segger J-Link debugger can interpret (see the #Debugging section below).

Such a debugger is *already* included on the NRF58240 develoment board however if you're working to a bare NF58240 chip or a bare u-blox NINA-B1 module you REALLY MUST equip yourself with a Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger.

For debugging you will need the Segger J-Link tools, of which the Windows ones can be found here:

https://www.segger.com/downloads/jlink/JLink_Windows.exe

If you don't have an NRF52840 board with Segger J-Link built in or you have a bare module etc. and are without a Segger J-Link box, it may be possible to fiddle with the `sdk_config.h` file down in the `cfg` directory to make it spit strings out of the spare UART instead but I don't recommended, it's hell down there.  You would need to enable a UART port, switch off `NRF_LOG_BACKEND_RTT_ENABLED` and fiddle with the likes of `NRF_LOG_BACKEND_UART_ENABLED`, `NRF_LOG_BACKEND_UART_TX_PIN` and `NRF_LOG_BACKEND_UART_BAUDRATE`.  Good luck!
