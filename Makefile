.PHONY: all clean
all: viiper

viiper: viiper.c viiper.h schemes.h
	gcc viiper.c -o viiper -g

clean:
	rm -f viiper
