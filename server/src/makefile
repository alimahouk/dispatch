CC=clang-5.0
CFLAGS=-I /usr/local/include -I.

ODIR=obj

LIBS=-lssl -lcrypto -lz -pthread -lpthread -luuid 
LIBDIRS=/usr/local/lib

DEPS = crypto.h disk.h net.h protocol.h types.h util.h

_OBJ = crypto.o disk.o main.o net.o protocol.o util.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dispatchd: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -L$(LIBDIRS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
