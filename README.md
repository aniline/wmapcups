## APC ups status dockapp

Shows status of APC ups as reported by [apcupsd](http://www.apcupsd.org/)

## Requires

* Windowmaker or some windowmanager that can show dockapps
* [libdockapp](https://www.dockapps.net/libdockapp)
* [apcupsd](http://www.apcupsd.org/) running with its NIS interface active and monitoring the UPS you are intereseted in.

## Build

    autoreconf -i
    ./configure
    make
    make install

## Running

When run without any parameters it would attempt to contact the apcups daemon runing on localhost.

    wmdockapp [-H <apcupsd_hostname>] [-P <port>] [-h] [-t]

- `apcupsd_hostname` : Host where apcupsd is running. (Default: 127.0.0.1)
- `port` : apcupsd NIS port (if different from 3551).
- `-h` : shows online help.
- `-t` : Test connection. Would not start the dockapp.
