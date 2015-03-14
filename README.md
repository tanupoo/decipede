decipede
========

*decipede* is like a serial port spliter.
It read data from a serial port or the console input.
And, it will just copy the data into some pseudo terminal devices.

## Usage

    ~~~~
    Usage: decipede [-dh] [-n num] [-C] (dev)

        It reads data from the device specified at the end of parameters.
        And, it writes the data into some pseudo terminal that it created
        when it had started.
        You can use a special word "con" to write the data into the standard
        output.

        -n: specifies the number of pseudo devices to be created. (default: 1)
        -C: writes data into the console as the one of the pseudo devices.
    ~~~~

## TODO

- improve to use libev for evolving into *centipede*.
