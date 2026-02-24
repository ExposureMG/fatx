---
title: MKFS.FATX(8)
date: February 2026
footer: FATX
header: System Administration Commands
---

# NAME

mkfs.fatx - creates an Xbox 360 FATX filesystem

# SYNOPSIS

**mkfs.fatx** [**-antvy**]
  [**--diff** *diff-file*]
  [**--offset** *offset*]
  [**--size** *size*]
  [**-b** | **--table** **file**|**mu**|**hd**|**kit**]
  [**-p** | **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**]
  [**-l** | **--label** *label*]
  [**-c** | **--cls-size** *cls-size*]
  [**-i** | **--input**] *device*

# DESCRIPTION

**mkfs.fatx** creates a FATX filesystem on the specified *device*.

*device* is the device file where the filesystem shall be created (e.g. /dev/hdXX).

If the *device* contains an existing partition table, **mkfs.fatx** uses it to
determine where to create the filesystem. Otherwise, **mkfs.fatx** considers
*device* to be a Xbox 360 retail hard disk and selects the x2 partition.

If the **--cls-size** option is not used, default values are taken for the number
of blocks per cluster.

# OPTIONS

**-a**, **--auto**
:   Give the default answer to all questions. Allows **mkfs.fatx** to be used
    non-interactively.

**-b**, **--table** **file**|**mu**|**hd**|**kit**
:   Choose the partition table on the device.

    **file** — for plain file

    **mu** — for memory unit

    **hd** — for Xbox 360 retail hard disk

    **kit** — for devkit hard disk

**-c**, **--cls-size** *cls-size*
:   Set the number of blocks per cluster to *cls-size*.

**-h**, **--help**
:   Print summary of options and exit.

**-i**, **--input** *device*
:   Set the *device* file where the filesystem shall be created.

**-l**, **--label** *label*
:   Set the filesystem name to *label*.

**-n**, **--no**
:   Assume an answer of "no" to all questions. Allows **mkfs.fatx** to be used
    non-interactively.

**-p**, **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**
:   Choose the partition, depending on the device.

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
:   Assume an answer of "yes" to all questions. Allows **mkfs.fatx** to be used
    non-interactively.

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

The exit code returned by **mkfs.fatx** is the sum of the following conditions:

0
:   no error

8
:   operational error

16
:   usage or syntax error

# REPORTING BUGS

If you manage to find a filesystem which causes **mkfs.fatx** to crash, or which
**mkfs.fatx** is unable to create, please report it to the author.

Please include as much information as possible in your bug report. Ideally,
include a complete transcript of the **mkfs.fatx** run, so I can see exactly what
error messages are displayed. If you have a writeable filesystem where the
transcript can be stored, the **script**(1) program is a handy way to save the
output of **mkfs.fatx** to a file.

Always include the full version string which **mkfs.fatx** displays when it is run
with **--version** option, so I know which version you are running.

# AUTHOR

This version of **mkfs.fatx** is written by Christophe Duverger \<fatx@christopheduverger.fr\>.

# SEE ALSO

**label.fatx**(8), **fsck.fatx**(8), **unrm.fatx**(8), **fusefatx**(8)
