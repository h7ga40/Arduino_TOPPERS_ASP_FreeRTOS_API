
GCC_TARGET_PREFIX=arm-none-eabi-

CXX = $(GCC_TARGET_PREFIX)g++
AR = $(GCC_TARGET_PREFIX)ar
RANLIB = $(GCC_TARGET_PREFIX)ranlib

INCLUDES = \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/arch \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/arch/arm_m_gcc/common \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/arch/gcc \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/arch/logtrace \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/include \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/kernel \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/syssvc \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/target/samd51_gcc \
	-I../../../../Arduino_TOPPERS_ASP/asp_wo_cfg/target/samd51_gcc/lib \
	-I../../../../Arduino_TOPPERS_ASP/src \
	-I../../../src

DEFINES = \
	-DARDUINO=10813 \
	-DARDUINO_ARCH_SAMD \
	-DARM_MATH_CM4 \
	-D__FPU_PRESENT \
	-D__SAMD51P19A__ \
	-D__SAMD51__

OPTIONS = \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-mlong-calls \
	-mthumb \
	-O1 \
	-g3 \
	-Wall \
	-ffunction-sections \
	-fno-exceptions \
	-fno-rtti

CXXFLAGS = $(OPTIONS) $(INCLUDES) $(DEFINES)

all: libToppersASP_FreeRTOS_API.a

DEPDIR = objs

$(DEPDIR):
	@mkdir -p $@

$(DEPDIR)/ToppersASP_FreeRTOS_API.o: ../../../lib/ToppersASP_FreeRTOS_API.cpp
	$(CXX) -o $@ -c $(CXXFLAGS) -MD -MF "$(@:%.o=%.d)" -MP -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" $<

libToppersASP_FreeRTOS_API.a: $(DEPDIR)/ToppersASP_FreeRTOS_API.o
	-rm -f $@
	$(AR) -rcs $@ $<
	$(RANLIB) $@

.PHONY: clean
clean:
	rm -f $(DEPDIR)/ToppersASP_FreeRTOS_API.o $(DEPDIR)/ToppersASP_FreeRTOS_API.d libToppersASP_FreeRTOS_API.a
