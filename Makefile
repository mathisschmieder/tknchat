debug:
		g++  -DDEBUG -lpthread -lncurses tknchat-masterping.cc -o tknchat
masterping:
		g++ -lpthread -lncurses tknchat-masterping.cc -o tknchat

all: masterping
