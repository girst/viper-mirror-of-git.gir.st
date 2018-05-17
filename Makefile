.PHONY: all clean
all: viiper

viiper: viiper.c
	gcc viiper.c -o viiper -g

clean:
	rm -f viiper
