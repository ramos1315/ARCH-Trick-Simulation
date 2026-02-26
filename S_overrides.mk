# S_overrides.mk – ARCH Habitat Trick Build Overrides
#
# This file is sourced by Trick's build system (trick-CP)
# to set compiler flags and include paths for the ARCH models.

# Compiler flags
TRICK_CFLAGS   += -I${TRICK_HOME}/include -Imodels
TRICK_CXXFLAGS += -I${TRICK_HOME}/include -Imodels

# Link math library
TRICK_LDFLAGS  += -lm

# Source files for the habitat model
TRICK_CFLAGS   += -Wall -Wextra -O2
TRICK_CXXFLAGS += -Wall -Wextra -O2

# Enable Trick data recording
TRICK_CFLAGS   += -DTRICK_DATA_RECORD
