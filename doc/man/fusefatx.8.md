---
title: FUSEFATX(8)
date: February 2026
footer: FATX
header: System Administration Commands
---

# NAME

fusefatx - mounts an Xbox 360 FATX filesystem

# SYNOPSIS

**fusefatx** [**-cdfsrtv**]
  [**--diff** *diff-file*]
  [**--nodate**]
  [**--nolost**]
    [**--version**]
  [**--uid** *uid*]
  [**--gid** *gid*]
  [**--mask** *mask*]
  [**-o** | **--option** *options*]
  [**--offset** *offset*]
  [**--size** *size*]
  [**-b** | **--table** **file**|**mu**|**usb**|**hd**|**kit**]
  [**-p** | **--partition** **sc**|**gc**|**se1**|**se2**|**xdv**|**x1**|**x2**]
  [**-i** | **--input**] *device*
  [**-m** | **--mount**] *mountpoint*

# DESCRIPTION

**fusefatx** mounts a FATX filesystem.

*device* is the device file where the filesystem is stored (e.g. /dev/hdXX).

**fusefatx** uses FUSE (filesystem in user space) to mount the filesystem. The
**--option** option can pass *options* directly to FUSE.

If the **--recover** option is used, the program first analyses the filesystem to
find deleted files and then mounts the filesystem in read only mode (implies **-t**
option) with deleted files and directories visible.

# OPTIONS

**-b**, **--table** **file**|**mu**|**usb**|**hd**|**kit**
:   Choose the partition table on the device. By default, Xbox 360 retail hard
    disk partition table is expected.

    **file** — for plain file

    **mu** — for memory unit

    **usb** — for USB drive

    **hd** — for Xbox 360 retail hard disk

    **kit** — for devkit hard disk

**-c**, **--cutname**
:   Disable long file names errors; long file names will be truncated to
    filesystem capacity. This may cause misuse of names (same name for 2
    different entries).

**-d**, **--debug**
:   This option enables FUSE to print debug information on the terminal. This
    option implies the **-f** option.

**-f**, **--foregrd**
:   This option enables the FUSE process not to be detached from the terminal.

**-h**, **--help**
:   Print summary of options and exit.

**-i**, **--input** *device*
:   Set the *device* file where the filesystem is.

**-m**, **--mount** *mountpoint*
:   Set the *mountpoint* directory to be used as the root of the filesystem.

**-o**, **--option** *options*
:   Define the *options* to be passed to FUSE. See **fuse**(8) manpage for more
    details.

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

**-R**, **--recover**
:   Mount the filesystem with the deleted files visible. This option implies the
    **-t** option: the filesystem is opened read only.

**-s**, **--singlethr**
:   This option tells FUSE not to use multiple threads for filesystem operations.

**-t**, **--test**
:   Open the filesystem read-only.

**-v**, **--verbose**
:   Verbose mode.

**--diff** *diff-file*
:   Use *diff-file* as a separate file from the input device to handle
    modifications. With this option, the input device remains unchanged.

**--gid** *gid*
:   Set the group id of the files mounted.

**--mask** *mask*
:   Set the permission mask of the files mounted. *mask* must be in octal format.

**--nodate**
:   Disable dates precedence of deleted files in the recovery algorithm. This
    option is only valid in conjunction with the **-R** option.

**--nolost**
:   Disable preservation of lost chains in the recovery algorithm. This option is
    only valid in conjunction with the **-R** option.

**--offset** *offset*
:   Force *offset* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--size** *size*
:   Force *size* of the partition in the device. This option disables the
    identification of partition mapping of the device.

**--uid** *uid*
:   Set the user id of the files mounted.

**--version**
:   Print version information and exit.

# EXIT CODE

The exit code returned by **fusefatx** is the sum of the following conditions:

0
:   no error

*code*
:   FUSE error code in case of filesystem operation error or FUSE internal error

8
:   operational error

16
:   usage or syntax error

# REPORTING BUGS

If you manage to find a filesystem which causes **fusefatx** to crash, or which
**fusefatx** is unable to mount, please report it to the author.

Please include as much information as possible in your bug report. Ideally,
include a complete transcript of the **fusefatx** run, so I can see exactly what
error messages are displayed. If you have a writeable filesystem where the
transcript can be stored, the **script**(1) program is a handy way to save the
output of **fusefatx** to a file.

Always include the full version string which **fusefatx** displays when it is run
with **--version** option, so I know which version you are running.

# AUTHOR

This version of **fusefatx** is written by Christophe Duverger \<fatx@christopheduverger.fr\>.

# SEE ALSO

**mkfs.fatx**(8), **label.fatx**(8), **fsck.fatx**(8), **unrm.fatx**(8)
