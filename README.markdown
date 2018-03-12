# VFSX – Samba VFS External Bridge


## Overview

VFSX is a transparent Samba Virtual File System (VFS) module which forwards operations to a process on the same machine for handing outside of the Samba daemon process (smbd). The external handler can be implemented in any language with support for Unix domain sockets (Python, Ruby, Perl, Java with Jtux) which is how VFSX and its external process communicates.

The advantage of using VFSX over a pure VFS module is that any programming language can be used to implement the transparent operations. VFS modules linked directly into the Samba daemon must be written in C or C++. while VFSX lets the developer implement a transparent module in his favorite higher-level language, with all the advantages of that language, outside of the smbd process.

The following figure illustrates a typical file server configuration with VFSX:

For Samba shares configured with VFSX as a VFS module, all client requests to manipulate files and directories will first be sent to the external handler process. For every VFS operation invoked by the Samba daemon, VFSX sends to the handler the name of the operation, the local directory path of the shared SMB service, the ID of the calling user, and any additional arguments required for the operation. The handler may respond to any operation for which it is designed. Note that since VFSX is a transparent module, file contents may not be passed between the VFSX module and its handler; The Samba daemon must still handle the file system I/O directly. However, the external handler may choose to reject an operation and return an error code which will be passed on to the client.

Included with VFSX is an external handler written in Python. Developers can extend this implementation to provide custom operation handling.


## SAMBA 3 Version 0.3-dev
Samba VFS internal API is really unstable, so it takes a bit more :effort: to support multiple version at once. Current version is tested with samba 3.6.14.


### Deploying the VFSX Module

> You find the version for Samba 3.6.x unmodified in folder module
> For Samba 4  look at the section below !

VFSX requires Samba 3.6.x. All directory and file paths described in the following instructions are based on Ubuntu 13.04 substitute paths as appropriate for your Linux distribution.

1. Install Samba 3.6.14, including the source distribution.
2. Download the VFSX source distribution: `git clone https://github.com/fudanchii/vfsx`.
3. `cd vfsx/module`
4. `./configure --prefix=<samba installation path> --with-samba-source=<samba source3 location>`
5. Build the VFSX shared library: `make`
6. Install with `make install`
7. Attach VFSX to a Samba shared directory by adding the VFSX module name to the share's configuration parameters. 
For example:  
`[myshare]`  
`comment = VFSX-Aware Shared Directory`  
`path = /home/myuser/shared/`  
`valid users = myuser`  
`read only = No`  
`vfs objects = vfsx`
8. Restart Samba
9. Run the Python external event handler:  
`python vfsx/python/vfsx.py`
10. <i>Access the share using smbclient or from a Windows system. By default the Python handler prints debug activity messages to the console. If the module has problems communicating with the external handler, error messages are written to syslog.</i>


## SAMBA 4 Version 

Tested with Samba 4.3.x, maybe it also works with versions prior 4.3
Unfortunatly since Samba 4 it is not longer possible to compile vfs modules outside the samba sourcetree. You have to copy your sources inside the samba sources and compile the whole bunch.

1. Install Samba 4.3.11, including the source distribution.
2. Download the VFSX source distribution: `git clone https://github.com/robert-boulanger/vfsx.git`.
3. `cd vfsx/samba4`
4. `cp vfs_vfsx.c <samba source3 location>/modules`
5. Add the contents of wscript_build to the end of `<samba source3 location>/modules/wscript_build`.
6. Edit `<samba source3 location>/wscript`  search for `default_shared_modules.extend`. (Around line 1610) Add `vfs_vfsx` to the list of vfs modules there. 
7. `cd <samba source3 location>`
8. `./autoconf.sh`
9. `cd ..` to `<samba source root>`
10. `./configure or ./configure.developer`
11. Build the whole samba: `make`
12. Install with `sudo cp <samba source root>/bin/default/source3/modules/libvfs_module_vfsx.so /usr/lib/x86_64-linux-gnu/samba/vfs/vfsx.so` or wherever your samba is installed. (Alternativly take the pre-compiled version from the bin dir here (intel/amd 64bit))
13. Attach VFSX to a Samba shared directory by adding the VFSX module name to the share's configuration parameters. 
For example:  
`[myshare]`  
`comment = VFSX-Aware Shared Directory`  
`path = /home/myuser/shared/`  
`valid users = myuser`  
`read only = No`  
`vfs objects = vfsx`
8. Restart Samba
9. Run the Python external event handler:  
`python vfsx/python/vfsx.py`
10. <i>Access the share using smbclient or from a Windows system. By default the Python handler prints debug activity messages to the console. If the module has problems communicating with the external handler, error messages are written to syslog.</i>


## Developing a Custom VFSX Handler with Python

1. Extend
2. Run

## Links

* [VFSX ][2]
* [Original VFSX Project Page at SourceForge][3]
* [Samba Home Page][4]
* [ Samba VFS Module Configuration][5]
* [ Samba VFS Module Developers Guide][6]
* [Python Home Page][7]
* [ Samba 4 VFS Module Developers Guide][9]

### License
VFSX is distributed under the open source [Mozilla Public License][8]. The file  `vfsx/LICENSE` contains the license terms.

Copyright (C) 2004 Steven R. Farley. All rights reserved.  
Copyright (C) 2009 Alexander Duscheleit.  
Copyright (C) 2013 Nurahmadie.

Copyright (C) 2018 Robert Boulanger.

  [1]: http://gitorious.org/vfsx/vfsx/blobs/raw/master/docs/config.png
  [2]: http://gitorious.org/vfsx
  [3]: http://sourceforge.net/projects/vfsx/
  [4]: http://www.samba.org/
  [5]: http://www.samba.org/samba/docs/man/Samba-HOWTO-Collection/VFS.html
  [6]: http://www.samba.org/samba/docs/man/Samba-Developers-Guide/vfs.html
  [7]: http://www.python.org/
  [8]: http://www.mozilla.org/MPL/MPL-1.1.html
  [9]: https://wiki.samba.org/index.php/Writing_a_Samba_VFS_Module
