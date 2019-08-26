
.phony: all play

INC := -I /mylib/mp4v2/include/
INC += -I /usr/local/include/ 

LIB := -L /mylib/mp4v2/lib/
LIB += -L /usr/local/lib/

FLAGS :=  -lmp4v2
FLAGS +=  -lstdc++
FLAGS +=  -lmp3lame
FLAGS +=  -lfaac

all:
	gcc -o build/test src/test4.c $(INC) $(LIB) $(FLAGS)

play:
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mylib/mp4v2/lib/:/usr/local/lib/; ./build/test misc/ttt.cms tmp/ttt.mp4


clean:
	rm a.out
