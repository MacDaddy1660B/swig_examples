################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: libCArcAPIClient3.6.so

# Tool invocations
libCArcAPIClient3.6.so: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -L"/home/streit/Documents/ARC_API/3.6"/Release -shared -o "libCArcAPIClient3.6.so" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(LIBRARIES)$(CC_DEPS)$(C++_DEPS)$(C_UPPER_DEPS)$(CXX_DEPS)$(OBJS)$(CPP_DEPS)$(C_DEPS) libCArcAPIClient3.6.so
	-@echo ' '

post-build:
	-cp "/home/streit/Documents/ARC_API/3.6/CArcAPIClient/Release/libCArcAPIClient3.6.so" /home/streit/Documents/JAVA/Owl/3.6/API; cp "/home/streit/Documents/ARC_API/3.6/CArcAPIClient/Release/libCArcAPIClient3.6.so" /home/streit/Documents/ARC_API/3.6/Release
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
