CFLAGS=-g -I/usr/local/rrdtool-1.0.33/include
LDFLAGS=-L/usr/local/rrdtool-1.0.33/lib -lrrd

mrecv: data.o mrecv.o
	$(CC) -o $@ data.o mrecv.o $(LDFLAGS)
