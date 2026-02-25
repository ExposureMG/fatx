---
title: LABEL.FATX(8)
date: February 2026
footer: FATX
header: System Administration Commands
---

# NAME

label.fatx - manage an Xbox 360 FATX filesystem volume name

# SYNOPSIS

**label.fatx** [**-v**]
  [**--diff** *diff-file*]
  [**--offset** *offset*]
  [**--size** *size*]
    [**--version**]
  [**-b** | **--table** **file**|**mu**|**usb**|**hd**|**kit**]
  [**-p** | **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**]
  [**-l** | **--label** *label*]
  [**-i** | **--input**] *device*

# DESCRIPTION

**label.fatx** prints or changes the name of a FATX filesystem.

*device* is the device file where the filesystem is stored (e.g. /dev/hdXX).

If the option **--label** is not used, the program will print the current label
of the filesystem.

# OPTIONS

**-b**, **--table** **file**|**mu**|**usb**|**hd**|**kit**
:   Choose the partition table on the device. By default, Xbox 360 retail hard
    disk partition table is expected.

    **file** — for plain file

    **mu** — for memory unit

    **usb** — for USB drive

    **hd** — for Xbox 360 retail hard disk

    **kit** — for devkit hard disk

**-h**, **--help**
:   Print summary of options and exit.

**-i**, **--input** *device*
:   Set the *device* file where the filesystem is.

**-l**, **--label** *label*
:   Change the filesystem name to *label*.

**-p**, **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**
:   Choose the partition, depending on the device. By default, **x2** partition
    is selected.

    **sc** — for system cache partition

    **gc** — for game cache partition

    **se1** — for sysext partition

    **se2** — for sysext2 partition

    **xdv** — for Xbox 360 dashboard partition

    **x1** — for original Xbox compatibility partition

    **x2** — for data partition

**-v**, **--verbose**
:   Verbose mode.

**--diff** *diff-file*
:   Use *diff-file* as a separate file from the input device to handle
    modifications. With this option, the input device remains unchanged.

**--offset** *offset*
:   Force *offset* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--size** *size*
:   Force *size* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--version**
:   Print version information and exit.

# EXIT CODE

The exit code returned by **label.fatx** is the sum of the following conditions:

0
:   no error

8
:   operational error

16
:   usage or syntax error

# REPORTING BUGS

If you manage to find a file system which causes **label.fatx** to crash, or which
**label.fatx** is unable to manage, please report it to the author.

Please include as much information as possible in your bug report. Ideally,
include a complete transcript of the **label.fatx** run, so I can see exactly what
error messages are displayed. If you have a writeable file system where the
transcript can be stored, the **script**(1) program is a handy way to save the
output of **label.fatx** to a file.

Always include the full version string which **label.fatx** displays when it is run
with **--version** option, so I know which version you are running.

# AUTHOR

This version of **label.fatx** is written by Christophe Duverger \<fatx@christopheduverger.fr\>.

# SEE ALSO

**mkfs.fatx**(8), **fsck.fatx**(8), **unrm.fatx**(8), **fusefatx**(8)
