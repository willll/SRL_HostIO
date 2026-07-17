# Handle assembly source files in the project
# (e.g. src/sgclib/binwrap.s)
ASM_SOURCES := $(filter %.s, $(SOURCES))
SOURCES := $(filter-out %.s, $(SOURCES))
OBJECTS += $(ASM_SOURCES:.s=.o)

%.o : %.s
	$(CC) $< $(CCFLAGS) -o $@

