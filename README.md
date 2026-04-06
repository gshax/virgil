# virgil

replacement preloader for Grandstream devices. the goal is to make custom firmware development easier and provide a rescue path for bricked devices.

## why?

the stock preloader (Bootastic) provides a UART boot method, but it depends on the BootROM's broken XMODEM implementation (which corrupts images >32KB). additionally, as far as i am aware, the only way to get into the (broken) UART mode is by chainloading from BootROM rescue mode (which sets `bootsel` to 5). these properties make it completely unsuitable for getting out of any brick condition involving U-Boot. HT818 U-Boot images are far bigger than 32KB!

also, it's kind of just cool :)

## current status

supports the Grandstream HT818 and an XMODEM boot method (without the BootROM's 32KB bug). virgil currently accepts only valid, decrypted Grandstream-formatted images.

support for DVF99xx devices and NAND boot is planned so that virgil will be able to completely replace Bootastic.

## building

```sh
# clone with submodules
git clone https://github.com/gshax/virgil.git --recursive

cd libgsfw

# build mkboot
make tools

# build libgsfw (image header parsing and min libc)
make EMBEDDED=1 libgsfw

cd ..

# build virgil
make
```

## using it

### NAND

1. get root on the device
2. copy the `virgil` image and `flash.sh` to `/tmp`
3. `./flash.sh virgil`
4. reboot

### recovery mode

1. [get UART on the device](https://gshax.github.io/research/uart/)
2. [get the device into recovery mode](https://gshax.github.io/research/uart/recovery_mode.html)
3. upload the `virgil` image via XMODEM
4. that's it
