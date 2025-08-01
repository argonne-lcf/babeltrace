
<!--
SPDX-FileCopyrightText: 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

SPDX-License-Identifier: CC-BY-SA-4.0
-->

Common Trace Format - Standards, Extensions and Libraries
Mathieu Desnoyers
September 26, 2010

This document describes the CTF library dependencies on standards, extensions
and libraries.


# Standards

## C99

This library is C99 compliant. A non-documented non-compliance should be
reported as a bug. See the ISO/IEC 9899:TC2 publication:

  http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf


## IEEE 754-2008

The IEEE 754-2008 standard is used for binary floating point arithmetic
representation. See:

  http://grouper.ieee.org/groups/754/


## GNU/C extensions

This library uses some widely GNU/C extensions widely adopted by compilers.
For detail, see:

  http://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html#C-Extensions



# Non-standard Dependencies

In some cases, standards do not provide the required primitives to write
portable code; these are listed here.


## Non-standard endian.h

endian.h is used to provide the following definitions:
```
#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN	4321
#define BYTE_ORDER	/* Your architecture: BIG_ENDIAN or LITTLE_ENDIAN */
```

## Bitfields

The ISO/IEC 9899 standard leaves bitfields implementation defined, which is
unacceptable for portability of this library. Section 6.7.2.1 - "Structure and
union specifiers", Semantic 10 specifically indicates that padding and order of
bit-fields allocation within a unit is implementation-defined.

This is why this library provides the bitfield.h header (under the MIT license),
which specifies bitfields write primitives that uses the same field order as the
GNU/C compiler, but does not require any padding for fields spreading across
units. This is therefore a superset of the GNU/C bitfields, which can be dealt
with by detecting C structure padding manually given the bit offset and
bit-field size as well as the unit size.


# Libraries

## The GLib library of C routines

The library glib 2 is used for its basic data structures. See

  http://library.gnome.org/devel/glib/2.24/glib-data-types.html
