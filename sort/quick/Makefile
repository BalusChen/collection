CC = c++
CPPFLAGS = -std=c++14
OBJECTS = test test-quick-sort.o quick_sort.o

test: test-quick-sort.o quick_sort.o
	$(CC) $(CPPFLAGS) -o test test-quick-sort.o quick_sort.o

test-quick-sort.o: test-quick-sort.cc
	$(CC) $(CPPFLAGS) -c test-quick-sort.cc

quick_sort.o: quick_sort.cc
	$(CC) $(CPPFLAGS) -c quick_sort.cc

clean:
	-rm -f $(OBJECTS)

.PHONE: clean
