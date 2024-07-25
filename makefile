#Libs
XCB = -lxcb

#Link
LINK = src/*.c

#Output 
OUT = out/fensterchef.exe

#Functions
build:
	gcc -o $(OUT) $(LINK) $(XCB)


