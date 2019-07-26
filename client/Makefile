OBJ_LIB := client.o
OBJ_APP := $(OBJ_LIB) main.o
LIBS = -pthread
INCLUDES = -I../common

.PHONY : all clean

all: client-lib.a client-test-app

%.o: %.c
	gcc $(INCLUDES) -c -o $@ $<

client-lib.a: $(OBJ_LIB)
	ar rcs $@ $(OBJ_LIB)

client-test-app: $(OBJ_APP)
	gcc -o $@ $(OBJ_APP) $(LIBS)

clean:
	rm -f $(OBJ_APP)
	rm -f client-lib.a
	rm -f client-test-app
