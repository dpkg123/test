#where to put the temp output
OUTPUT		=	objs

#the default configuration is either "debug" or "release"
#but user can change it, eg. "make CONFIG=release" or "make CONFIG=debug"
ifdef CONFIG
  ifneq ($(CONFIG),debug)
    ifneq ($(CONFIG),release)
      CHECK_ERROR = $(error Only debug or release currently supported for CONFIG)
    endif
  endif
else
  CONFIG	=	release
endif

SRCS = \
	edgelzo1x_acc_fccm.h \
	edgelzo1x_config1x.h \
	edgelzo1x_copy_lit.ch \
	edgelzo1x_copy_lit_impl.ch \
	edgelzo1x_lzo1_d.ch \
	edgelzo1x_lzo1x_d4.c \
	edgelzo1x_lzo1x_d4s.c \
	edgelzo1x_lzo_conf.h \
	edgelzo1x_lzo_dict.h \
	edgelzo1x_lzo_ptr.h \
	edgelzo1x_miniacc.h \
	edgelzo1x_plzo1_c.ch \
	edgelzo1x_plzo1_d_fc.h \
	edgelzo1x_plzo1x_1f.c \
	edgelzo1x_plzo1x_1g.c \
	edgelzo1x_plzo1x_1h.c \
	edgelzo1x_plzo1x_1i.c \
	edgelzo1x_plzo1x_1j.c \
	edgelzo1x_plzo1x_1k.c \
	edgelzo1x_plzo1x_1l.c \
	edgelzo1x_plzo1x_1m.c \
	edgelzo1x_plzo1x_1n.c \
	edgelzo1x_plzo1x_c.ch \
	edgelzo1x_plzo1x_d.ch \
	edgelzo1x_plzo_dict.h

#.c source files
C_SRCS		=	$(filter %.c,$(SRCS))
#.cpp source files
CPP_SRCS	=	$(filter %.cpp,$(SRCS))

#.o objects for .c compilation
C_OBJS		=	$(patsubst %,$(OUTPUT)/%,$(patsubst %.c,%.o,$(C_SRCS)))
#.o objects for .cpp compilation
CPP_OBJS	=	$(patsubst %,$(OUTPUT)/%,$(patsubst %.cpp,%.o,$(CPP_SRCS)))

#.o objects to build .a library
OBJS		=	$(C_OBJS) $(CPP_OBJS)

#.a (library) file
AS		=	lzo1xcompress.a



#build whats needed (this must be first target!)
.PHONY: build
build: $(CHECK_ERROR) $(AS)
	@echo

#clean output if desired		    
.PHONY: clean
clean: $(CHECK_ERROR)
	@echo
	@echo clean output:
	rm -r -f $(OUTPUT)/*
	rm -f $(AS)

#clean output and build	everything
.PHONY: all
all: clean build


#flags for .c and .cpp		 
C_CPP_FLAGS =	-g -W -Wall -Wshadow -Werror -Wp,-MMD,$(patsubst %.o,%.d,$@) -Wp,-MT,$@ -D__GCC__ -Iinclude
ifeq ($(CONFIG),release)
  C_CPP_FLAGS += -O3 -DNDEBUG
else
  C_CPP_FLAGS += -O0 -D_DEBUG
endif 

#flags for .c
C_FLAGS	=	$(C_CPP_FLAGS) -std=c99

#flags for .cpp
CPP_FLAGS =	$(C_CPP_FLAGS) -std=c++98

#compile c files
$(C_OBJS): $(OUTPUT)/%.o: %.c
	@echo
	@echo compile $(notdir $<)
	@mkdir -p $(dir $@)
	gcc -c $(C_FLAGS) -o $@ $<

#compile cpp files
$(CPP_OBJS): $(OUTPUT)/%.o: %.cpp
	@echo
	@echo compile $(notdir $<)
	@mkdir -p $(dir $@)
	g++ -c $(CPP_FLAGS) -o $@ $<


#build .a (library)
$(AS): $(OBJS)
	@echo
	@echo build $(notdir $@) library
	@mkdir -p $(dir $@)
	ar -r $@ $^
