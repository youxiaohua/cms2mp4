
.phony: all play

INC := -I lib/mp4v2/include/
LIB := -L lib/mp4v2/lib/

FLAGS :=  -lmp4v2
FLAGS +=  -lstdc++
FLAGS +=  -lmp3lame
FLAGS +=  -lfaac

all:
	gcc -o build/cms2mp4 src/main.c $(INC) $(LIB) $(FLAGS)

play:
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:lib/mp4v2/lib/:/usr/local/lib/; ./build/cms2mp4 misc/ttt.cms tmp/ttt.mp4


clean:
	rm a.out
