---
title: UNRM.FATX(8)
date: February 2026
footer: FATX
header: System Administration Commands
---

# NAME

unrm.fatx - tries to recover deleted files on an Xbox 360 FATX filesystem

# SYNOPSIS

**unrm.fatx** [**-aflntvy**]
  [**--diff** *diff-file*]
  [**--nodate**]
  [**--nolost**]
  [**--offset** *offset*]
  [**--size** *size*]
    [**--version**]
  [**-b** | **--table** **file**|**mu**|**usb**|**hd**|**kit**]
  [**-p** | **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**]
  [**-i** | **--input**] *device*

# DESCRIPTION

**unrm.fatx** can recover deleted files in a FATX filesystem.

*device* is the device file where the filesystem is stored (e.g. /dev/hdXX).

If the **--local** option is used, the program recovers files in the current
directory outside the FATX filesystem. Otherwise, it recovers files and
directories in the FATX filesystem.

# OPTIONS

**-a**, **--auto**
:   Give the default answer to all questions. Allows **unrm.fatx** to be used
    non-interactively.

**-b**, **--table** **file**|**mu**|**usb**|**hd**|**kit**
:   Choose the partition table on the device. By default, Xbox 360 retail hard
    disk partition table is expected.

    **file** — for plain file

    **mu** — for memory unit

    **usb** — for USB drive

    **hd** — for Xbox 360 retail hard disk

    **kit** — for devkit hard disk

**-f**, **--nofat**
:   Disable recovery of lost chains in file allocation table.

**-h**, **--help**
:   Print summary of options and exit.

**-i**, **--input** *device*
:   Set the *device* file where the filesystem is.

**-l**, **--local**
:   Recovered files are written in the current directory instead of in the FATX
    filesystem. This option implies the **-t** option: the filesystem is opened
    read only.

**-n**, **--no**
:   Assume an answer of "no" to all questions. Allows **unrm.fatx** to be used
    non-interactively.

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

**-t**, **--test**
:   Open the filesystem read-only.

**-v**, **--verbose**
:   Verbose mode.

**-y**, **--yes**
:   Assume an answer of "yes" to all questions. Allows **unrm.fatx** to be used
    non-interactively.

**--diff** *diff-file*
:   Use *diff-file* as a separate file from the input device to handle
    modifications. With this option, the input device remains unchanged.

**--nodate**
:   Disable dates precedence of deleted files in the recovery algorithm.

**--nolost**
:   Disable preservation of lost chains in the recovery algorithm.

**--offset** *offset*
:   Force *offset* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--size** *size*
:   Force *size* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--version**
:   Print version information and exit.

# EXIT CODE

The exit code returned by **unrm.fatx** is the sum of the following conditions:

0
:   no error

8
:   operational error

16
:   usage or syntax error

# REPORTING BUGS

If you manage to find a filesystem which causes **unrm.fatx** to crash, please
report it to the author.

Please include as much information as possible in your bug report. Ideally,
include a complete transcript of the **unrm.fatx** run, so I can see exactly what
error messages are displayed. If you have a writeable filesystem where the
transcript can be stored, the **script**(1) program is a handy way to save the
output of **unrm.fatx** to a file.

Always include the full version string which **unrm.fatx** displays when it is run
with **--version** option, so I know which version you are running.

# AUTHOR

This version of **unrm.fatx** is written by Christophe Duverger \<fatx@christopheduverger.fr\>.

# SEE ALSO

**mkfs.fatx**(8), **label.fatx**(8), **fsck.fatx**(8), **fusefatx**(8)
