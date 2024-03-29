include Makefile.inc

KERNEL=kernel.bin

ifdef MM
    MM_FLAG := -D$(MM)
else
    MM_FLAG :=
endif

SOURCES=$(wildcard *.c)
SOURCES_INTERRUPTIONS=$(wildcard interruptions/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)

OBJECTS=$(SOURCES:%.c=obj/%.o)
OBJECTS_INTERRUPTIONS=$(SOURCES_INTERRUPTIONS:interruptions/%.c=obj/%.o)
OBJECTS_ASM=$(SOURCES_ASM:asm/%.asm=obj/%.asm.o)

LOADERSRC=loader.asm

LOADEROBJECT=$(LOADERSRC:%.asm=obj/%.asm.o)
STATICLIBS=

ALL_OBJECTS= $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(OBJECTS_INTERRUPTIONS)

all: $(KERNEL)

$(KERNEL): $(STATICLIBS) $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel.elf --oformat=elf64-x86-64 $^

obj/%.o: interruptions/%.c
	$(GCC) $(GCCFLAGS) -I./include -I./interruptions -c $< -o $@

obj/%.asm.o : asm/%.asm
	$(ASM) $(ASMFLAGS) $< -o $@

obj/%.o: %.c
	$(GCC) $(GCCFLAGS) -I./include -I./interruptions -c $< -o $@ $(MM_FLAG)

$(LOADEROBJECT):
	mkdir -p obj
	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

clean:
	rm -rf obj
	rm *.bin *.elf | true

.PHONY: all clean
