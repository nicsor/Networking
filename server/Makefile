OBJ_LIB := server.o
OBJ_APP := $(OBJ_LIB) main.o
LIBS = -pthread
INCLUDES = -I../common

.PHONY : all clean

all: server-lib.a server-test-app

%.o: %.c
	gcc $(INCLUDES) -c -o $@ $<

server-lib.a: $(OBJ_LIB)
	ar rcs $@ $(OBJ_LIB)

server-test-app: $(OBJ_APP)
	gcc -o $@ $(OBJ_APP) $(LIBS)

clean:
	rm -f $(OBJ_APP)
	rm -f server-lib.a
	rm -f server-test-app
