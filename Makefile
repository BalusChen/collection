
CC = c++
CPPFLAGS = -std=c++11 -Wall

filter: bloom-filter/bloom-filter.cc
	$(CC) $(CPPFLAGS) -o filter bloom-filter/bloom-filter.cc


.PHONE: clean

clean:
	-rm filter

