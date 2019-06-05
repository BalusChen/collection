CC = c++

filter: bloom-filter/bloom-filter.cc
	${CC} -std=c++11 -o filter bloom-filter/bloom-filter.cc


.PHONE: clean

clean:
	-rm filter

