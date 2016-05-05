

FLAGS=-std=c11 -O3

antenasmpi: antenas.c cputils.h
	mpicc -DCP_MPI -O3  antenas.c -o antenasmpi

