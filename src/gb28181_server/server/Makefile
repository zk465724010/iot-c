
ROOT := $(shell pwd)

JDK_HEMO = /home/zk/work/extends/jdk-16.0.1

#JAVA_FILE_DIR = $(ROOT)/unis
JAVA_FILE_DIR = $(ROOT)

#JAVA_FILE = $(notdir $(wildcard $(JAVA_FILE_DIR)/*.java))
JAVA_FILE = $(wildcard $(JAVA_FILE_DIR)/gb28181_server.java)
CLASS_NAME = $(patsubst %.java,%,$(notdir $(JAVA_FILE)))

JNI_SOURCE_FILE = $(CLASS_NAME).cpp


.PHONY: all clean build $(CLASS_NAME) test

all: build


build: $(JAVA_FILE)
	@#javac $^
	@#javah -classpath . -jni unis.$(CLASS_NAME)
	javac $^ -h .
	@echo
	@echo "$(CLASS_NAME).h 头文件已生成..."
	@echo



$(CLASS_NAME): $(JNI_SOURCE_FILE)
	@#gcc -I. -I$(JDK_HEMO)/include -I$(JDK_HEMO)/include/linux -fpermissive -fPIC -shared -o lib$@.so $^
	gcc -I. -I$(SRC) -I$(JDK_HEMO)/include -I$(JDK_HEMO)/include/linux -fPIC -shared -o lib$@.so $^
	@echo
	@echo "lib$@.so 库文件已生成..."
	@echo


clean:
	@-rm -f *.class
	@-rm -f *.so
	@-rm -f *.h


test: test.java
	javac $^
	java -Djava.library.path='../../build/bin/' test



# export LD_LIBRARY_PATH="/home/zk/work/projects/gb28181_server/build/bin:/home/zk/work/projects/gb28181_server/libs/eXosip2-4.0.0/x64/lib":$LD_LIBRARY_PATH
# java test