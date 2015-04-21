decipede
========

*decipede* is like a serial port spliter.
It is intended to read data from a serial port
and to copy the data into some pseudo terminal devices.
You can use the standard input
so that you can input any charactors to the pseudo devices.

## Usage

    ~~~~
    Usage: decipede [-dh] [-n num] [-o name] [-C] (dev)

        It reads data from the device specified in the end of parameters.
        And, it writes the data into some pseudo terminal that it created
        when it had started.  The baud rate of the pseudo terminal is 115200
        for that devices.  You can use a special word "con" to write
        the data into the standard output.

        -n: specifies the number of pseudo devices to be created. (default: 1)
        -b: specifies the baud rate of the read dev. (default is 115200)
        -o: specifies the file name in which decipede will put the prepared device
            names.  The device names are printed out to the standard output
            if this option is not specified.
        -C: writes data into the console as the one of the pseudo devices.
        -x: writes data in hex string.
    ~~~~

## TODO

- think to use libev for evolving into *centipede*.
