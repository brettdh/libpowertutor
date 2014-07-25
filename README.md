LibPowerTutor
=============

This library provides calculation of energy usage according to the
mobile device power models developed in the [PowerTutor][powertutor-home]
project. It was developed independently (though also at
the University of Michigan) for use in the
[Informed Mobile Prefetching][imp-paper] project.

Currently it includes models for the HTC Dream (G1) and the Nexus One phones,
and the choice of power model is hard-coded to the Nexus One.
It is designed for easily adding new power models. The [PowerTutor
Android application][powertutor-github] includes power models for three phones
(libpowertutor does not support the HTC Sapphire), and the original paper
describes a method for deriving power models based on a suite of benchmarks
that isolate and exercise various device components.

## Installation/Build setup

First, clone and set up the lone dependency: [Mocktime][mocktime].
From there, follow the steps below for your platform.

### Android

The `android_project` directory contains an Android library that you can use
by importing it in Eclipse and adding it as a library project to your Android
project.  See the Android documentation for more details.

Alternatively, to use from JNI, first symlink the `jni` directory into your NDK modules path:

    $ mkdir $NDK_MODULE_PATH/edu.umich.mobility
    $ ln -s /path/to/libpowertutor/jni $NDK_MODULE_PATH/edu.umich.mobility/libpowertutor

then add this line to the end of your `Android.mk`:

    $(call import-module, edu.umich.mobility/libpowertutor)

### Other

    $ cd cpp_wrapper
    $ make
    $ sudo make install

Only tested on Linux; may work on other platforms by happy accident.  Note:
if you're not running libpowertutor on an Android device, presumably it's because
it's running in the server at the other end, which is attempting to predict
the energy impact of sending data back to one of the client's multiple network addresses.
It will not be of much use if you are attempting to estimate the energy usage
of a server.

## Usage

For Java, you can use the `EnergyUsage` class to track energy usage, either total
or by component.  The two components are network and CPU, which are tracked
separately.

For energy prediction, use the static methods of the `EnergyEstimates` class.
For the most part, they are thin wrappers for the similarly-named functions
in `libpowertutor.h`; refer to the documentation there for details.

For C/C++, see documentation in libpowertutor.h. Its functions fall roughly into
three categories:

* Estimating future energy usage
* Reporting past energy usage
* Experimental/unsupported functions

## Caveats

LibPowerTutor estimates energy usage of the phone's 3G data radio and CPU.
It may run without breaking on other phones than those listed above, but
its estimates may be wildly inaccurate -- particularly if the phone uses
LTE, since the energy usage of an LTE radio differs significantly from
that of a 3G radio.

[powertutor-home]: http://powertutor.org/
[powertutor-github]: http://github.com/msg555/powertutor
[imp-paper]: http://bretthiggins.me/papers/mobisys12.pdf
[mocktime]: http://github.com/brettdh/mocktime
