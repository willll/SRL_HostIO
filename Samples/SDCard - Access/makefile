# Configuration
SRL_MAX_TEXTURES = 100          # Number of VDP1 texture slots
SRL_MODE = NTSC                 # Valid options are PAL or NTSC
SRL_HIGH_RES = 0                # 480i mode
SRL_HIGH_RES_NON_INTERLACED = 0 # 240p mode
SRL_FRAMERATE = 1               # Framerate control (0=dynamic, 1=< 60/value)
SRL_MAX_CD_BACKGROUND_JOBS = 1  # Maximum number of files GFS can open at once
SRL_MAX_CD_FILES = 256          # Maximum number of files on a CD
SRL_MALLOC_METHOD = SIMPLE

# Sound driver specific configuration
SRL_USE_SGL_SOUND_DRIVER = 0    # Set to 1 if you want to use SGL sound driver
SRL_ENABLE_FREQ_ANALYSIS = 0

# SGL configuration
SGL_MAX_VERTICES = 2500         # Number of vertices that can be used
SGL_MAX_POLYGONS = 1500         # Number of polygons that can be used
SGL_MAX_EVENTS = 1             	# Number of events that can be used
SGL_MAX_WORKS = 1             	# Number of works that can be used 

# Disk name
CD_NAME = SDCard_Access

# Directory build will be placed into
BUILD_DROP = ./BuildDrop

# SRL installation directory
SRL_INSTALL_ROOT ?= ../../../..

# Include HostIO extra module
MODULES_EXTRA = SRL_HostIO

# Find all .c, .cxx and .s files
SOURCES = $(patsubst ./%,%,$(shell find src/ -name '*.c')) 
SOURCES += $(patsubst ./%,%,$(shell find src/ -name '*.cxx'))
SOURCES += $(patsubst ./%,%,$(shell find src/ -name '*.s'))

# Include fatfs and sgclib sources from modules_extra/SRL_HostIO/INC
SOURCES += ../../INC/fatfs/ff.c \
           ../../INC/fatfs/diskio.c \
           ../../INC/fatfs/ffsystem.c \
           ../../INC/fatfs/ffunicode.c \
           ../../INC/sgclib/api_functions.c \
           ../../INC/sgclib/log.c \
           ../../INC/sgclib/sdcard_packet.c

# Include shared makefile
SDK_ROOT = $(SRL_INSTALL_ROOT)/saturnringlib
include $(SDK_ROOT)/shared.mk
