.phony all:
all:
	gcc ACS.c -pthread -o ACS
.PHONY clean:
clean:
	-rm -rf *.o *.exe



