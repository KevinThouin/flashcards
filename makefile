all: flashcards

card.o: src/card.cpp src/card.h
	g++ -c -O3 -std=c++20 -Wall -Wextra -Wconversion -pedantic-errors -Werror -Irapidjson/include -Isrc $< -o $@

json_io.o: src/json_io.cpp src/json_io.h src/card.h
	g++ -c -O3 -std=c++20 -Wall -Wextra -Wconversion -pedantic-errors -Werror -Irapidjson/include -Isrc $< -o $@

main.o: src/main.cpp src/card.h src/json_io.h
	g++ -c -O3 -std=c++20 -Wall -Wextra -Wconversion -pedantic-errors -Werror -Irapidjson/include -Isrc $< -o $@

flashcards: json_io.o main.o card.o
	g++ $^ -o $@

clean:
	rm -f card.o json_io.o main.o flashcards

.PHONY: all clean
