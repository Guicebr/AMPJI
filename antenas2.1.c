/**
 * Computación Paralela (curso 1516)
 *
 * Colocación de antenas
 * Versión MPI
 *
 * @author Guillermo Cebrian
 * @author Alberto Gil
 * 
 * 
 */


// Includes generales
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <mpi.h>


// Include para las utilidades de computación paralela
#include "cputils.h"


/**
 * Estructura antena
 */

typedef struct {
	int y;
	int x;
} Antena;

/**
 * Macro para acceder a las posiciones del mapa
 */
#define m(y,x) mapa[ (y * cols) + x ]


/**
 * Función de ayuda para imprimir el mapa
 */
void print_mapa(int * mapa, int rows, int cols, Antena * a){


	if(rows > 50 || cols > 30){
		printf("Mapa muy grande para imprimir\n");
		return;
	};

	#define ANSI_COLOR_RED     "\x1b[31m"
	#define ANSI_COLOR_GREEN   "\x1b[32m"
	#define ANSI_COLOR_RESET   "\x1b[0m"

	printf("Mapa [%d,%d]\n",rows,cols);
	int i,j;
	for(i=0; i<rows; i++){
		for(j=0; j<cols; j++){

			int val = m(i,j);

			if(val == 0){
				if(a != NULL && a->x == j && a->y == i){
					printf( ANSI_COLOR_RED "   A"  ANSI_COLOR_RESET);
				} else { 
					printf( ANSI_COLOR_GREEN "   A"  ANSI_COLOR_RESET);
				}
			} else {
				printf("%4d",val);
			}
		}
		printf("\n");
	}
	printf("\n");
}



/**
 * Distancia de una antena a un punto (y,x)
 * @note Es el cuadrado de la distancia para tener más carga
 */
int manhattan(Antena a, int y, int x){

	int dist = abs(a.x -x) + abs(a.y - y);
	return dist * dist;
}



/**
 * Actualizar el mapa con la nueva antena
 */
void actualizar(int * mapa, int rows, int cols, Antena antena){

	int i,j;
	m(antena.y,antena.x) = 0;

	for(i=0; i<rows; i++){
		for(j=0; j<cols; j++){

			int nuevadist = manhattan(antena,i,j);

			if(nuevadist < m(i,j)){
				m(i,j) = nuevadist;
			}

		} // j
	} // i
}



/**
 * Calcular la distancia máxima en el mapa
 */
int calcular_max(int * mapa, int rows, int cols, int size){

	int i,j;
	int max = 0;
	int tam = cols;
	
	// creacion de un array de maximos
	
	int * maximo = malloc((size_t) tam * sizeof(int));
	
	// reduccion de la matriz en el 0
	
	MPI_Reduce(mapa, maximo,tam, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
	for(i=0; i<rows; i++){
		
		if(maximo[i]>max){
			max = maximo[i];			
		}
	} // i
	return max;
	
}


/**
 * Calcular la posición de la nueva antena
 */
Antena nueva_antena(int * mapa, int rows, int cols, int min){

	int i,j;

	for(i=0; i<rows; i++){
		for(j=0; j<cols; j++){

			if(m(i,j)==min){

				Antena antena = {i,j};
				return antena;
			}

		} // j
	} // i
}



/**
 * Función principal
 */
int main(int nargs, char ** vargs){

	// 1. Variables
	int rank, size;
	
	// 2. Iniciar MPI
	MPI_Init( &nargs, &vargs );
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	MPI_Comm_size( MPI_COMM_WORLD, &rank );

	// 2.1 Check number of processors
	if(size != 2)
	{
		if(rank == 0) fprintf(stderr, "I need 2 processors\n");
		MPI_Finalize();
		return EXIT_FAILURE;
	}

	// 3. Creacion del tipo derivado MPI_Antena
	MPI_Datatype MPI_Antena;
	Antena antena;
	
	// 3.1 direcciones de los campos
	MPI_Aint address_antena;
	MPI_Aint address_y;
	MPI_Aint address_x;
	
	MPI_Get_address(&antena, &address_antena);
	MPI_Get_address(&antena.y, &address_y);
	MPI_Get_address(&antena.x, &address_x);

	//
	// 1. LEER DATOS DE ENTRADA
	//
	
	int rows,cols,distMax,nAntenas;
	int * mapa;
	Antena * antenas;
	// Comprobar número de argumentos
	if(nargs < 7){
		fprintf(stderr,"Uso: %s rows cols distMax nAntenas x0 y0 [x1 y1, ...]\n",vargs[0]);
		return -1;
	}

	// Leer los argumentos de entrada
	rows = atoi(vargs[1]);
	cols = atoi(vargs[2]);
	distMax = atoi(vargs[3]);
	nAntenas = atoi(vargs[4]);

	if(nAntenas<1 || nargs != (nAntenas*2+5)){
		fprintf(stderr,"Error en la lista de antenas\n");
		return -1;
	}

	if(rank == 0){
		// Mensaje
		printf("Calculando el número de antenas necesarias para cubrir un mapa de"
			" (%d x %d)\ncon una distancia máxima no superior a %d "
		 	"y con %d antenas iniciales\n\n",rows,cols,distMax,nAntenas);

	// Reservar memoria para las antenas
	antenas = malloc(sizeof(Antena) * (size_t) nAntenas);
	if(!antenas){
		fprintf(stderr,"Error al reservar memoria para las antenas inicales\n");
		return -1;
	}		
	
	// Leer antenas
	int i;
	for(i=0; i<nAntenas; i++){
		antenas[i].x = atoi(vargs[5+i*2]);
		antenas[i].y = atoi(vargs[6+i*2]);

		if(antenas[i].y<0 || antenas[i].y>=rows || antenas[i].x<0 || antenas[i].x>=cols ){
			fprintf(stderr,"Antena #%d está fuera del mapa\n",i);
			return -1;
		}
	}

	}// Fin rank 0

	//
	// 2. INICIACIÓN
	//

	// Medir el tiempo
	double tiempo = cp_Wtime();
	if(rank == 0)
	{
	// Crear el mapa
	mapa = malloc((size_t) (rows*cols) * sizeof(int) );

	// Iniciar el mapa con el valor MAX INT
	int i;
	for(i=0; i<(rows*cols); i++){
		mapa[i] = INT_MAX;
	}

	// Colocar las antenas iniciales
	for(i=0; i<nAntenas; i++){
		actualizar(mapa,rows,cols,antenas[i]);
	}
	}// Fin rank 0 2


	// Debug
#ifdef DEBUG
	print_mapa(mapa,rows,cols,NULL);
#endif


	//
	// 3. CALCULO DE LAS NUEVAS ANTENAS
	//

	// Contador de antenas
	int nuevas = 0;
	
	int max;
	while(1){
		//Particiones
		if(rank == 0){
			int particiones = rows;
			int p;
			int * mapaaux;
			for(p=0;p<particiones;p++){
				
				//Calculamos la particion
				//Filas de la matriz
				int p_ini = p*cols/particiones;
				int p_fin = ((p+1) * cols/particiones)-1;
				int tam = p_fin - p_ini + 1;
				// Enviamos la particion
				MPI_Send(&mapa[p_ini,p_fin],tam,MPI_INT,p+1,999,MPI_COMM_WORLD);

				// Recibimos el maximo
				MPI_Recv(&max,1,MPI_INT,0,888,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			}	
				
			// Salimos si ya hemos cumplido el maximo
			if (max <= distMax) break;	
		
			// Incrementamos el contador
			nuevas++;
				
			// Calculo de la nueva antena y actualización del mapa
			Antena antena = nueva_antena(mapa, rows, cols, max);
			actualizar(mapa,rows,cols,antena);
		
		}else{
			int * mapa;
			mapa = malloc((size_t) cols * sizeof(int));

			// Recibimos la particion de la matriz
			MPI_Recv(mapa,cols,MPI_INT,0,999,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			
			// Calcular el máximo
			int max = calcular_max(mapa, 0, cols,size);
			
			// Enviamos el maximo
			MPI_Send(&max,1,MPI_INT,0,888,MPI_COMM_WORLD);
						
			}		

		

	}

	// Debug
#ifdef DEBUG
	print_mapa(mapa,rows,cols,NULL);
#endif

	//
	// 4. MOSTRAR RESULTADOS
	//

	// tiempo
	tiempo = cp_Wtime() - tiempo;	

	// Salida
	printf("Result: %d\n",nuevas);
	printf("Time: %f\n",tiempo);
	
	MPI_Finalize();
	return EXIT_SUCCESS ;

}


