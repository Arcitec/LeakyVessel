all: scanner_gnu scanner_linux

scanner_gnu: src/Scanner/scanner.cpp
	mkdir -p dist
	x86_64-w64-mingw32-g++ -std=c++20 -O2 -mconsole -static src/Scanner/scanner.cpp -o dist/scanner_gnu.exe
	chmod -x dist/scanner_gnu.exe

scanner_linux: src/Scanner/scanner.cpp
	mkdir -p dist
	g++ -std=c++20 -O2 src/Scanner/scanner.cpp -o dist/scanner_linux

clean:
	rm -fv dist/scanner_gnu.exe dist/scanner_linux
