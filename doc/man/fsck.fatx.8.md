---
title: FSCK.FATX(8)
date: February 2026
footer: FATX
header: System Administration Commands
---

# NAME

fsck.fatx - check an Xbox 360 FATX filesystem

# SYNOPSIS

**fsck.fatx** [**-afntvy**]
  [**--diff** *diff-file*]
  [**--offset** *offset*]
  [**--size** *size*]
    [**--version**]
  [**-b** | **--table** **file**|**mu**|**usb**|**hd**|**kit**]
  [**-p** | **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**]
  [**-i** | **--input**] *device*

# DESCRIPTION

**fsck.fatx** checks the FATX filesystem.

*device* is the device file where the filesystem is stored (e.g. /dev/hdXX).

Note that in general it is not safe to run **fsck.fatx** on mounted filesystems.
The only exception is if the **-t** option is specified. However, even if it is
safe to do so, the results printed by **fsck.fatx** are not valid if the filesystem
is mounted.

# OPTIONS

**-a**, **--auto**
:   Automatically repair the filesystem without any questions.

**-b**, **--table** **file**|**mu**|**usb**|**hd**|**kit**
:   Choose the partition table on the device. By default, Xbox 360 retail hard
    disk partition table is expected.

    **file** — for plain file

    **mu** — for memory unit

    **usb** — for USB drive

    **hd** — for Xbox 360 retail hard disk

    **kit** — for devkit hard disk

**-f**, **--nofat**
:   Disable file allocation table sanity check.

**-h**, **--help**
:   Print summary of options and exit.

**-i**, **--input** *device*
:   Set the *device* file to be checked.

**-n**, **--no**
:   Assume an answer of "no" to all questions. Allows **fsck.fatx** to be used
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
:   Assume an answer of "yes" to all questions. Allows **fsck.fatx** to be used
    non-interactively.

**--diff** *diff-file*
:   Use *diff-file* as a separate file from the input device to handle
    modifications. With this option, the input device remains unchanged.

**--offset** *offset*
:   Force *offset* of the partition to be checked in the device. This option
    disables the identification of partition mapping of the device.

**--size** *size*
:   Force *size* of the partition to be checked in the device. This option
    disables the identification of partition mapping of the device.

**--version**
:   Print version information and exit.

# EXIT CODE

The exit code returned by **fsck.fatx** is the sum of the following conditions:

0
:   no error

1
:   filesystem errors corrected

4
:   filesystem errors left uncorrected

8
:   operational error

16
:   usage or syntax error

# REPORTING BUGS

If you manage to find a filesystem which causes **fsck.fatx** to crash, or which
**fsck.fatx** is unable to repair, please report it to the author.

Please include as much information as possible in your bug report. Ideally,
include a complete transcript of the **fsck.fatx** run, so I can see exactly what
error messages are displayed. If you have a writeable filesystem where the
transcript can be stored, the **script**(1) program is a handy way to save the
output of **fsck.fatx** to a file.

Always include the full version string which **fsck.fatx** displays when it is run
with **--version** option, so I know which version you are running.

# AUTHOR

This version of **fsck.fatx** is written by Christophe Duverger \<fatx@christopheduverger.fr\>.

# SEE ALSO

**mkfs.fatx**(8), **label.fatx**(8), **unrm.fatx**(8), **fusefatx**(8)
