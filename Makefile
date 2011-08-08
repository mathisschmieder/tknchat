all: tkn
debug:
		g++  -DDEBUG -lpthread -lncurses tknchat-unicast.cc -o tknchat
tkn:
		g++ -lpthread -lncurses tknchat.cc -o tknchat

