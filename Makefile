.PHONY: all clean

CFLAGS := -Wall -Wextra -pedantic -std=c99

all: viper

viper: viper.c viper.h schemes.h
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f viper
