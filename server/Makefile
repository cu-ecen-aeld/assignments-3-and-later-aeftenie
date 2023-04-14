default: all

all: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(INCLUDES) aesdsocket.c -o aesdsocket $(LDFLAGS)
    
clean:
	-rm -f *.o
	-rm -f aesdsocket
