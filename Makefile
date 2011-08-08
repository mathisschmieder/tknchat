all: unicast
debug:
		g++  -DDEBUG -lpthread -lncurses tknchat-unicast.cc -o tknchat
unicast:
		g++ -lpthread -lncurses tknchat-unicast.cc -o tknchat

