# Target library
lib := libuthread.a
objs := sem.o queue.o thread.o tps.o 

CC := gcc
CFLAGS := -Wall -Werror
CFLAGS += -g

all: $(lib)

deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libuthread.a: $(objs)
	ar rcs libuthread.a $(objs)

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	$(Q) rm -f $(targets) $(objs) $(deps)

