CC		= gcc
CFLAGS		= -g -Os -fPIC
CPPFLAGS	=  
LDFLAGS		= 
LDSHFLAGS	= -shared
INSTALLCMD	= /usr/bin/install -c
SAMBA_SOURCE=/home/adie/repo/samba-3.5.21/source3
FLAGS		=  $(CFLAGS) -I/usr/include -Iinclude -I$(SAMBA_SOURCE)/librpc -I$(SAMBA_SOURCE)/tdb/include -I$(SAMBA_SOURCE)/../lib/replace -I$(SAMBA_SOURCE)/include -I$(SAMBA_SOURCE)/ubiqx -I$(SAMBA_SOURCE)/smbwrapper  -I. $(CPPFLAGS) -I$(SAMBA_SOURCE) -I$(SAMBA_SOURCE)/..
SHLIBEXT	= so
OBJEXT		= o


prefix		= /usr/local/samba
libdir		= ${prefix}/lib

VFS_LIBDIR	= $(libdir)/vfs

# Auto target
default: $(patsubst %.c,%.$(SHLIBEXT),$(wildcard *.c))

# Pattern rules

%.$(SHLIBEXT): %.$(OBJEXT)
	@echo "Linking $@"
	@$(CC) $(LDSHFLAGS) $(LDFLAGS) -o $@ $<

%.$(OBJEXT): %.c
	@echo "Compiling $<"
	@$(CC) $(FLAGS) -c $<


install: default
	$(INSTALLCMD) -d $(VFS_LIBDIR)
	$(INSTALLCMD) -m 755 *.$(SHLIBEXT) $(VFS_LIBDIR)

# Misc targets
clean:
	rm -rf .libs
	rm -f core *~ *% *.bak *.o *.$(SHLIBEXT)


