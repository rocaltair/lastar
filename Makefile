CC = gcc
LIB = lastar.so
OBJS = lastar.o \
       AStar.o

LLIBS = -llua
CFLAGS = -c -fPIC -Wall
LDFLAGS = --shared

all : $(LIB)

$(LIB): $(OBJS)
	$(CC) -o $@ $^ $(LLIBS) $(LDFLAGS) 

$(OBJS) : %.o : %.c
	$(CC) -o $@ $(CFLAGS) $<

clean : 
	rm -f $(OBJS) $(LIB)

.PHONY : clean

