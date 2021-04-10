#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main(int argc, char **argv){
	fibonacci (atoi(argv[1]));
	sum_of_four_int (atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	
	return EXIT_SUCCESS;
}
