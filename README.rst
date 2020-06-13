fetchtime - Query time from NTP server
======================================
`fetchtime` is a small program to query time from NTP servers. `fetchtime` can
set the system clock from the fetched NTP time, or simply print it to stdout.

Installation
------------

Verify that compilation flags and installation paths defined in ``config.mk``
match your expectations. Then compile `fetchtime` code with

::

    make

and install it with

::

    make install

Usage
-----

Print time to stdout

::

    $ fetchtime pool.ntp.org
    Wed Jan  1 00:00:01 2020

Set system time

::

    $ fetchtime -s pool.ntp.org

License
-------

`fetchtime` is distributed under MIT license. `fetchtime` also includes the
``ntp.h`` header from the ``openntp`` project that is distributed under ISC
license.

Limitations
-----------

`fetchtime` sets system time in a single step without gradual adjustment.

`fetchtime` does not correct time delays caused by network transport.

Current version of `fetchtime` will not work correctly past January 2030.

