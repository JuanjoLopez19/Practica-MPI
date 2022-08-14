#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>

#define CHAR_NF 32
#define CHAR_MAX 127
#define CHAR_MIN 33

#define PESO_COMPROBAR 5000000
#define PESO_GENERAR 10000000

#define ES 0
#define CHK 100
#define GNT 200

#define MSG_WHO_AM_I 800
#define MSG_WORD 801
#define MSG_GENERATED 802
#define MSG_CHECKED 803
#define MSG_NEW_CHARS_FOUND 804
#define MSG_FOUND 805
#define MSG_HINT 806
#define MSG_ERROR 807

#define MAX_LENGTH 500

int isNumber(char number[]);
void fuerza_espera(unsigned long peso);


int main (int argc, char**argv)
{
    // Vars
    int nprocs;
    int id;
    int checkers;
    int generators;
    int hintMode = 0;
    int who_am_i;
    int wordlong = -1;
    int found;
    int flag, flagFound = 0, flagChecked = 0;
    int hinted = 0;
    int brexit = 0;
    int counter_gen = 0, counter_checked = 0;
    int i, j=1;

    char *word = NULL;
    char *checkingWord = NULL;
    char *previousWord = NULL;
    char *wordTry = NULL;
    char *hintWord = NULL;

    MPI_Status status;
    MPI_Status statusFound, statusChecked;
    MPI_Request request;

    struct timeval t_start, t_end;
    double t_exec;

    FILE *fp = NULL;
   
    //---------------------- Inicio API MPI ----------------------//


	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);


    //---------------------- Comprobaciones iniciales ----------------------//


    if(id == ES){
        //start = clock();
        //time(&start);
        gettimeofday(&t_start, NULL);
    }

    if (argc != 4 && id == ES)
    {
        printf("\n\tERROR: wrong invocation. Try with:\n\n\t\t mpirun -np <numProcs> (-oversubscribe) <executable> <numCheckers> <wordLong> <hintMode>\n\n");
        MPI_Finalize();
        exit(0);
    }
    else if (argc != 4)
    {
        MPI_Finalize();
        exit(0);
    }
    
    if (nprocs < 3 && id == ES)
    {
        printf("\n\tERROR: numProcs must be at least 3.\n\n");
        MPI_Finalize();
        exit(0);
    }
    else if (nprocs < 3)
    {
        MPI_Finalize();
        exit(0);
    }

    if(isNumber(argv[1]) != 1){
        checkers = atoi(argv[1]);
    } else {
        if(id == ES)printf("\n\tERROR: numCheckers must be a number.\n\n");
        MPI_Finalize();
        exit(0);
    }

    if (checkers > nprocs - 2 && id == ES ){
        printf("\n\tERROR: numCheckers must leave at least one process to generators.\n\n");
        MPI_Finalize();
        exit(0);
    }
    else if (checkers > nprocs - 2){
        MPI_Finalize();
        exit(0);
    }
    
    generators = nprocs - checkers - 1;

    if(isNumber(argv[2]) != 1){
        wordlong = atoi(argv[2]);
    } else {
        if(id == ES)printf("\n\tERROR: the length of the word must be a number.\n\n");
        MPI_Finalize();
        exit(0);
    }

    if(wordlong > MAX_LENGTH){
        printf("\n\tERROR: The length of the word must be less than 500.\n\n");
        MPI_Finalize();
        exit(0);
    }
    if(NULL == (word = malloc(sizeof(char)*(wordlong+1)))){
        printf("\n\tERROR: fatal error doing malloc.\n\n");
        MPI_Finalize();
        exit(0);
    }

    if(id == ES){
        srand(291); // Siempre genera la misma, ya que la semilla es estática.
        for(i=0;i<wordlong;i++) word[i] = rand()%(CHAR_MAX-CHAR_MIN+1)+CHAR_MIN;
        word[wordlong] = '\0';
    }

    if(isNumber(argv[3]) != 1){
        hintMode = atoi(argv[3]);
    } else {
        if(id == ES)printf("\n\tERROR: hintMode must be 0 (OFF) or 1 (ON).\n\n");
        MPI_Finalize();
        exit(0);
    }

    if ((hintMode != 0 && hintMode != 1) && id == ES)
    {
        printf("\n\tERROR: hintMode must be 0 (OFF) or 1 (ON).\n\n");
        MPI_Finalize();
        exit(0);
    }
    else if (hintMode != 0 && hintMode != 1)
    {
        MPI_Finalize();
        exit(0);
    }

    if(id == ES) printf("\nNUMERO DE PROCESOS: Total %d: E/S: 1, Comprobadores: %d, Generadores: %d\n",nprocs, checkers, generators);


    //---------------------- Asignación de roles ----------------------//


    if (id == ES){
        printf("\nTIPO DE PROCESOS:\n");
        who_am_i = CHK;
        for (i = 1; i <=checkers; i++){
            MPI_Send(&who_am_i, 1, MPI_INT, i, MSG_WHO_AM_I, MPI_COMM_WORLD);
            printf("%2d) CHECKER.\n",i);
        }
        
        for (i = checkers +1 ; i < nprocs ; i++){
            if(j>checkers) j = 1;
            MPI_Send(&j, 1, MPI_INT, i, MSG_WHO_AM_I, MPI_COMM_WORLD);
            printf("%2d) GENERATOR -> Checker nº%2d.\n",i, j);
            j++;
        }
        who_am_i = ES;
    } else {
        MPI_Recv(&who_am_i,1, MPI_INT, ES, MSG_WHO_AM_I, MPI_COMM_WORLD, &status);
    }


    //---------------------- Reserva de memoria auxiliar ----------------------//


    if (id == ES){
        if(NULL == (wordTry = malloc(sizeof(char)*(wordlong+1)))){
            printf("\n\tERROR: fatal error doing malloc.\n\n");
            for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
        }
    } else if (who_am_i == CHK){
        if(NULL == (word = malloc(sizeof(char)*(wordlong+1)))){
            printf("\n\tERROR: fatal error doing malloc.\n\n");
            for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
        }
        if(NULL == (checkingWord = malloc(sizeof(char)*(wordlong+1)))){
            printf("\n\tERROR: fatal error doing malloc.\n\n");
            for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
        }
    } else {
        if(NULL == (word = malloc(sizeof(char)*(wordlong+1)))){
            printf("\n\tERROR: fatal error doing malloc.\n\n");
            for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
        }
        if(NULL == (previousWord = malloc(sizeof(char)*(wordlong+1)))){
            printf("\n\tERROR: fatal error doing malloc.\n\n");
            for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
        }
        if(hintMode){
            if(NULL == (hintWord = malloc(sizeof(char)*(wordlong+1)))){
                printf("\n\tERROR: fatal error doing malloc.\n\n");
                for(i=0;i<nprocs;i++) MPI_Send(&i, 1, MPI_INT, i, MSG_ERROR, MPI_COMM_WORLD);
            }
        }   
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Iprobe(MPI_ANY_SOURCE, MSG_ERROR, MPI_COMM_WORLD, &flag, &status);
    if(flag){
        MPI_Finalize();
        exit(0);
    }


    //---------------------- Comunicación de la palabra ----------------------//


    MPI_Bcast(&wordlong, 1, MPI_INT, ES, MPI_COMM_WORLD);

    if (id == ES){
        printf("\nNOTIFICACION PALABRA COMPROBADORES:\n");
        for (i = 1; i <=checkers; i++){
            MPI_Send(word, wordlong+1, MPI_CHAR, i, MSG_WORD, MPI_COMM_WORLD);
            printf("%2d) %s - (%d).\n", i, word, wordlong );
        }
        wordTry[wordlong] == '\0';
    } else if (who_am_i == CHK){
        MPI_Recv(word, wordlong+1, MPI_CHAR, ES, MSG_WORD, MPI_COMM_WORLD, &status);
        word[wordlong] = '\0';
        checkingWord[wordlong] = '\0';
    } else {
        if(hintMode) hintWord[wordlong] = '\0';
        for(i=0;i<wordlong;i++) word[i] = CHAR_NF;
        word[wordlong] = '\0';
        previousWord[wordlong] = '\0';
    }


    //---------------------- Separamos la ejecución ----------------------//


    switch(who_am_i){
    case ES:
        printf("\nBUSCANDO:\n");
        while(1){
            MPI_Recv(wordTry, wordlong+1, MPI_CHAR, MPI_ANY_SOURCE, MSG_NEW_CHARS_FOUND, MPI_COMM_WORLD, &status);
            printf("%02d) PISTA.....: %s\n", status.MPI_SOURCE, wordTry);

            if(strcmp(word,wordTry) == 0) {
                for(i = 1 ; i < nprocs ; i++){
                    MPI_Send(&i, 1, MPI_INT, i, MSG_FOUND, MPI_COMM_WORLD);
                }
                
                printf("\nPALABRA ENCONTRADA POR %d.\n", status.MPI_SOURCE);
                printf("BUSCADA...: %s\n", word);
                printf("ENCONTRADA: %s\n", wordTry);

                free(word);
                free(wordTry);
                break;
            }

            if(hintMode){
                for(i = checkers+1 ; i < nprocs ; i++){
                    if(i != status.MPI_SOURCE){
                        MPI_Send(wordTry, wordlong+1, MPI_CHAR, i, MSG_HINT, MPI_COMM_WORLD);
                    }
                }
            }
            
        }
        
        break;
    case CHK:

        while(1){

            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if(status.MPI_TAG == MSG_FOUND){
                break;
            }
            if(status.MPI_TAG == MSG_GENERATED){
                MPI_Recv(checkingWord, wordlong+1, MPI_CHAR, MPI_ANY_SOURCE, MSG_GENERATED, MPI_COMM_WORLD, &status);
                for(i=0;i<wordlong;i++){
                    if(checkingWord[i] != word[i] ){
                        checkingWord[i] = CHAR_NF;
                    }
                }
                counter_checked++;
                fuerza_espera(PESO_COMPROBAR);
                MPI_Send(checkingWord, wordlong+1, MPI_CHAR, status.MPI_SOURCE, MSG_CHECKED, MPI_COMM_WORLD);
            }
        }

        free(checkingWord);
        free(word);
        break;

    default:

        srand(id*40);

        while(!brexit){

            strcpy(previousWord,word);

            for(i=0;i<wordlong;i++){
                if(word[i] == CHAR_NF){
                    word[i] = rand()%(CHAR_MAX-CHAR_MIN+1)+CHAR_MIN;
                }
            }
            counter_gen++;
            fuerza_espera(PESO_GENERAR);

            MPI_Send(word, wordlong+1, MPI_CHAR, who_am_i, MSG_GENERATED, MPI_COMM_WORLD);

            while(1){
                MPI_Iprobe(ES, MSG_FOUND, MPI_COMM_WORLD, &flagFound, &statusFound);
                MPI_Iprobe(who_am_i, MSG_CHECKED, MPI_COMM_WORLD, &flagChecked, &status);

                if(flagFound){
                    brexit = 1;
                    break;
                }

                if(flagChecked){
                    MPI_Recv(word, wordlong+1, MPI_CHAR, status.MPI_SOURCE, MSG_CHECKED, MPI_COMM_WORLD, &status);
                    if(hintMode){
                        MPI_Iprobe(ES, MSG_HINT, MPI_COMM_WORLD, &flag, &status);
                        while(flag){
                            MPI_Recv(hintWord, wordlong+1, MPI_CHAR, ES, MSG_HINT, MPI_COMM_WORLD, &status);
                            for(i=0;i<wordlong;i++){
                                if(word[i] == CHAR_NF && hintWord[i] != CHAR_NF){
                                    word[i] = hintWord[i];
                                }
                            }
                            MPI_Iprobe(ES, MSG_HINT, MPI_COMM_WORLD, &flag, &status);
                        }  
                    }

                    if(strcmp(word,previousWord) != 0){
                        MPI_Send(word, wordlong+1, MPI_CHAR, ES, MSG_NEW_CHARS_FOUND, MPI_COMM_WORLD);
                    }
                    break;
                }
            }
        }
        if(hintMode) free(hintWord);
        free(previousWord);
        free(word);

        break;
    }


    //---------------------- Cálculo de estadísticas ----------------------//


    if(id == ES){
        gettimeofday(&t_end, NULL);
        t_exec = (double) (t_end.tv_usec - t_start.tv_usec) / 1000000 + (double) (t_end.tv_sec - t_start.tv_sec);
        printf("\nTiempo transcurrido: %f\n\n", t_exec);
    }

    MPI_Reduce(&counter_checked, &i, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&counter_gen, &j, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Finalize();

    if(id == ES){
        if(NULL == (fp = fopen("estadisticas.txt","a+"))){
            printf("\n\tERROR: can not open the file.\n");
        }else {
            fprintf(fp,"%d#1#%d#%d#%d#%d#%d#%f#%f#%f\n", nprocs, checkers, generators, hintMode, i, j, t_exec, ((double) i/t_exec), ((double) j/t_exec));
            fclose(fp);
        }
    }
    
	return 0;	
}

int isNumber(char number[]){

	int i = 0;
    
	if (number[0] == '-'){
		i = 1;
	}
	for (; number[i] != 0; i++){
		if (!isdigit(number[i])){
			return 1;
		}
	}
	return 0;
}

void fuerza_espera(unsigned long peso){
    for (unsigned long i = 1; i < 1*peso; i++) sqrt(i);
}