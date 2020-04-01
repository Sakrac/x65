#--------------------------------------------------------
# $File: Makefile,v $          
#                     
# $Date: 2019/10/20 $             
# $Author: jandersen $           
# $Revision: #1 $         
#--------------------------------------------------------
#
# ftile GCC-Gnu Makefile
#

# compile under *nix, or Windows Subsystem for Linux

SHELL	= /bin/sh
MKDIR	= mkdir

TARGET	= ftile

PROJROOT	= .

#SYSTEM      = /usr
#SYSLIBDIR   = $(SYSTEM)/lib
#SYSINCDIR   = $(SYSTEM)/include

INCCMD		=	-I$(SYSINCDIR)
INCCMD		+=	-I$(PROJROOT)/source
INCCMD		+=	-I$(PROJROOT)/include

OBJDIR		= $(PROJROOT)/obj
DEPDIR		= $(PROJROOT)/dep
LSTDIR		= $(PROJROOT)/lst

#
#  Special GnuMake Search Path Directive
#
VPATH		= $(PROJROOT)/source

#
#  Dedicated Search Paths for Specific Types
#
# Can be used to speed up compile by using this feature
# for each filetype (reducing the amount of searching)
#
vpath %.o $(OBJDIR)
vpath %.d $(DEPDIR)

LIBCMD          +=-lm

OBJS       	:= ftile.o
OBJS		+= rawdata.o

# change list of .o's into a list of .d's

DEPS	   := $(OBJS:%.o=%.d)

AS          = gcc
CC          = gcc
LD          = gcc
RM          = /bin/rm -rfv

CFLAGS      = -O2 -Wall -Werror -Wa,-al -fno-common
CXXFLAGS    = -O2 -Wall -Werror -Wa,-al -fno-common
ASFLAGS     = -c -xassembler-with-cpp -Wa,-al
LDFLAGS     = -Wl,-Map,$(TARGET).map $(LIBCMD)

# Clear Default Suffixes
.SUFFIXES:
# Set my Own Suffixes
.SUFFIXES: .c .s .cc .d .o

all: $(TARGET)

$(TARGET): $(DEPS) $(OBJS) $(LIBS)
	$(LD) -o $@ $(addprefix $(OBJDIR)/,$(OBJS)) $(LIBS) $(LDFLAGS)

# Object Rules

.s.o:
	$(AS) $(ASFLAGS) $(TMPFLAGS) $(INCCMD) -o $(OBJDIR)/$@ $< > $(LSTDIR)/$*.lst

.c.o:
	$(CC) $(CFLAGS) $(TMPFLAGS) $(INCCMD) -c $< -o $(OBJDIR)/$*.o > $(LSTDIR)/$*.lst

.cc.o:
	$(CC) $(CXXFLAGS) $(TMPFLAGS) $(INCCMD) -c $< -o $(OBJDIR)/$*.o > $(LSTDIR)/$*.lst

# Dependencie Rules
#
# for now just touch, to create the file if its not defined
#
.s.d:
	touch $(DEPDIR)/$*.d

.c.d:
	set -e; $(CC) -M $(CFLAGS) $(INCCMD) $< \
	| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $(DEPDIR)/$@; \
	[ -s $(DEPDIR)/$@ ] || rm -f $(DEPDIR)/$@

.cc.d:
	set -e; $(CC) -M $(CXXFLAGS) $(INCCMD) $< \
	| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $(DEPDIR)/$@; \
	[ -s $(DEPDIR)/$@ ] || rm -f $(DEPDIR)/$@

.PHONY: install
install: $(TARGET)
	cp $(TARGET).exe $(PROJROOT)/../bin

.PHONY: clean
clean:
	$(RM) $(OBJDIR) *.o $(DEPDIR) *.map $(LSTDIR) $(TARGET) $(TARGET).exe


########################################
# 
# HELPER TARGET RULES
#
########################################

#
# Target that forces all of the objects to be rebuilt if the makefile changes
#

$(OBJS) : Makefile
$(DEPS) : Makefile
#
# Targets that create the output object directory if it doesn't already exist
#

Makefile : $(OBJDIR) $(DEPDIR) $(LSTDIR)

$(OBJDIR) :
	$(MKDIR) $(OBJDIR)

#
# Targets that create the output dependency directory if it doesn't already exist
#

$(DEPDIR) :
	$(MKDIR) $(DEPDIR)

#
# Targets that create the output list directory if it doesn't already exist
#
  
$(LSTDIR) :
	$(MKDIR) $(LSTDIR)

#
# Generated Dependencie Files
#
-include $(wildcard $(DEPDIR)/*.d)


