OBJS = util/GLog.o util/YamlConf.o util/SocketBuffer.o util/SocketConnection.o OcrServer.o
CFLAGS = -W -Wall -Wunused-value -std=c++11 -g -rdynamic
DEPENDS = lib/glog/libglog.a lib/yaml/libyaml-cpp.a -lpthread -lev -ltesseract -llept
INCLUDE = -I. -Iutil/ -Ilib/

bin/ocr_server: main.cpp main.h $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $^ $(INCLUDE) $(DEPENDS)

util/GLog.o: util/GLog.cpp util/GLog.h
	$(CXX) $(CFLAGS) -c $< $(INCLUDE) -o $@

util/YamlConf.o: util/YamlConf.cpp util/YamlConf.h
	$(CXX) $(CFLAGS) -c $< $(INCLUDE) -o $@

util/SocketBuffer.o: util/SocketBuffer.cpp util/SocketBuffer.h
	$(CXX) $(CFLAGS) -c $< $(INCLUDE) -o $@

util/SocketConnection.o: util/SocketConnection.cpp util/SocketConnection.h
	$(CXX) $(CFLAGS) -c $< $(INCLUDE) -o $@

OcrServer.o: OcrServer.cpp OcrServer.h
	$(CXX) $(CFLAGS) -c $< $(INCLUDE) -o $@

.PHONY: clean start stop
clean:
	-$(RM) util/*.o util/*.gch
	-$(RM) bin/ocr_server *.o *.gch
	-$(RM) -rf run/supervise
	-$(RM) log/*

start:
	-make stop
	-make
	./bin/supervise.ocr_server run/ &

stop:
	-killall supervise.ocr_server 2>/dev/null
	-killall ocr_server 2>/dev/null
