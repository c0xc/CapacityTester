PROGRAM=CapacityTester

SRCDIR=src
INCDIR=inc
OBJDIR=obj
BINDIR=bin
SUBDIR=sub

CC_OPT_O=-o 
LD_OPT_O=$(CC_OPT_O)

VERSIONFILE_DEFAULT=$(INCDIR)/version.hpp.default
VERSIONFILE=$(INCDIR)/version.hpp

EXECUTABLE=$(BINDIR)/$(PROGRAM)

PLATFORM=g++
include Makefile_$(PLATFORM)_inc

CFLAGS+="-DPROGRAM=\"$(PROGRAM)\""

CFLAGS+=$(CFLAGS_QT)
CFLAGS+=$(ADDCFLAGS)

LDFLAGS+=$(ADDLDFLAGS)

MODULES+=main
MODULES+=size
MODULES+=capacitytestergui
MODULES+=volumetester

HEADERS=$(MODULES:%=$(INCDIR)/%.hpp)
SOURCES=$(MODULES:=.cpp)
SOURCES_QT=$(MODULES:=.moc.cpp)
OBJECTS=$(SOURCES:%.cpp=$(OBJDIR)/%.obj)
OBJECTS_QT=$(SOURCES:%.cpp=$(OBJDIR)/%.moc.obj)

rebuild: clean compile link

build: compile link

build-32bit: compile-32bit link-32bit

rebuild-32bit: clean compile-32bit link-32bit

compile: $(OBJDIR) $(VERSIONFILE) $(HEADERS) $(OBJECTS) $(OBJECTS_QT) clean-moc

compile-32bit: CFLAGS+=-m32

compile-32bit: compile

$(OBJDIR):
	mkdir $@

$(BINDIR):
	mkdir $@

$(VERSIONFILE): $(VERSIONFILE_DEFAULT)
	cp $< $@

$(OBJDIR)/%.obj: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) $< $(CC_OPT_O)$@

$(SRCDIR)/%.moc.cpp: $(INCDIR)/%.hpp
	$(MOC) $< -o $@

$(INCDIR)/%.hpp.pre: $(INCDIR)/%.hpp
	$(CC) $(CFLAGS) -E $< > $@

$(EXECUTABLE): link

link: $(BINDIR) $(OBJECTS) $(OBJECTS_QT)
	$(LD) $(OBJECTS) $(OBJECTS_QT) $(LDFLAGS) $(LDFLAGS_QT) $(LD_OPT_O)$(EXECUTABLE)

link-32bit: LDFLAGS+=-m32

link-32bit: link

clean:
ifeq ($(CMD_CLEAN),)
	rm -f $(OBJDIR)/*.o $(OBJDIR)/*.obj $(SRCDIR)/*.moc.cpp
else
	$(CMD_CLEAN)
endif

clean-moc:
ifeq ($(CMD_CLEAN_MOC),)
	rm -f $(SRCDIR)/*.moc.cpp
else
	$(CMD_CLEAN_MOC)
endif

list-sources:
	@echo $(SOURCES)

list-objects:
	@echo $(OBJECTS)

list-objects-qt:
	@echo $(OBJECTS_QT)

show-executable:
	@echo $(EXECUTABLE)

show-cflags:
	@echo $(CFLAGS)

show-qtdir:
	@echo $(QTDIR)

