/*****************
 * Projekt 2 do IOS
 * Autor: Rudolf Jurišica <xjuris02@stud.fit.vutbr.cz>
 * Práce se semafory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#define PARAM_COUNT 5   //pocet vstupnich parametru musi byt 5

#define SEM_NAME1 "/xjuris02.sem.oxy_queue"
#define SEM_NAME2 "/xjuris02.sem.hydro_queue"
#define SEM_NAME3 "/xjuris02.sem.water_mutex"
#define SEM_NAME4 "/xjuris02.sem.wait_mutex"

#define SHARED_MEM_INIT(var) {(var) = mmap(NULL, sizeof(*(var)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}

//sdilene pameti
int *oxy_cnt = NULL;
int *hydro_cnt = NULL;
int *line_cnt = NULL;
int *oxy_bar_cnt = NULL;
int *hydro_bar_cnt = NULL;
int *molecule_cnt = NULL;

//semafory
sem_t *oxy_queue = NULL;
sem_t *hydro_queue = NULL;
sem_t *water_mutex = NULL;
sem_t *wait_mutex = NULL;

FILE *fp;

typedef struct {
    int NO;
    int NH;
    int TI;
    int TB;
} Params;

//funkce pro inicializaci semaforu
void semaphores_init() {
    sem_unlink(SEM_NAME1);
    sem_unlink(SEM_NAME2);
    sem_unlink(SEM_NAME3);
    sem_unlink(SEM_NAME4);

    if ((oxy_queue = sem_open(SEM_NAME1, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    if ((hydro_queue = sem_open(SEM_NAME2, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    if ((water_mutex = sem_open(SEM_NAME3, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    if ((wait_mutex = sem_open(SEM_NAME4, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}
}

//ziskani vstupnich parametru
int get_params(int argc, char **argv, Params *params) {
    if (argc != PARAM_COUNT) {
        perror("invalid count of parameters\n");
        exit(1);
    }

    params->NO = strtol(argv[1], NULL, 10);     //pocet kysliku
    params->NH = strtol(argv[2], NULL, 10);     //pocet vodiku
    params->TI = strtol(argv[3], NULL, 10) * 1000;   //maximalni cas v mikrosekundach, po ktery atom kysliku/vodiku ceka,
                                                    //nez se zaradi do fronty na vytvareni molekul
    params->TB = strtol(argv[4], NULL, 10) * 1000;   //maximalni cas v mikrosekundach nutny pro vytvoreni jedne molekuly

    if(params->TI < 0 || params->TB < 0 || params->NO <= 0 || params->NH <= 0) {
        perror("invalid parameters\n");
        exit(1);
    }
    return 0;
}


//nepouzivat wait_pid - aktivni cekani



/*probouzeni - kdyz jakykoliv z jeho child procesu zmeni svuj stav -> returne se 

pousteni urciteho poctu procesu - sem_post - tak, jak to ma ve funkci gen_riders (for loop nebo while loop)
*/




//funkce na vygenerovani delky cekani - parametr max_time je bud TI, nebo TB
void wait_random_time(int max_time) {
    usleep(rand()%(max_time+1));
}

//funkce na uzavreni vseho, co bylo vytvoreno (otevreno) v prubehu programu
void clean_all() {
    munmap(oxy_cnt, sizeof(oxy_cnt));
    munmap(line_cnt, sizeof(line_cnt));
    sem_unlink(SEM_NAME1);
    sem_unlink(SEM_NAME2);
    sem_unlink(SEM_NAME3);
    fclose(fp);
}


void oxy_process(int oxy_atom_cnt, Params params) {

    wait_random_time(params.TI);

    //going to queue
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: O %d: going to queue\n", *line_cnt, oxy_atom_cnt);
    fflush(fp);
    *oxy_bar_cnt += 1;
    sem_post(wait_mutex);

    //kyslik ceka ve fronte kysliku
    sem_wait(oxy_queue);

    sem_wait(water_mutex);
    if(*hydro_bar_cnt >= 2) {
        sem_wait(wait_mutex);
        molecule_cnt += 1;
        sem_post(hydro_queue);
        *line_cnt += 1;
        fprintf(fp, "%d: ")
    }

    //sem_wait(oxy_queue);  //za semaforem oxy_queue cekaji vytvorene kysliky

/*
    sem_wait(water_mutex);
    if(*hydro_bar_cnt >= 2) {
        *molecule_cnt += 1;
        sem_post(hydro_queue);
        *line_cnt += 1;
        printf("%d: H %d: creating molecule %d\n", *line_cnt, *hydro_cnt, *molecule_cnt);
        sem_post(hydro_queue);
        *line_cnt += 1;
        printf("%d: H %d: creating molecule %d\n", *line_cnt, *hydro_cnt, *molecule_cnt);       //FIXME vypisovat spravne id atomu
        *hydro_bar_cnt -= 2;
        sem_post(oxy_queue);
        *line_cnt += 1;
        *oxy_bar_cnt -= 1;
        printf("%d: O %d: creating molecule %d\n", *line_cnt, *oxy_cnt, *molecule_cnt);
        //wait_random_time(molec_time);
        //kyslik informule vodiky, ze je molekula dokoncena

        //TODO vodiky by se mely nejak uspat, a pak zase zavolat
        *line_cnt += 1;
        printf("%d: H %d: molecule %d created\n", *line_cnt, *hydro_cnt, *molecule_cnt);
        *line_cnt += 1;
        printf("%d: H %d: molecule %d created\n", *line_cnt, *hydro_cnt, *molecule_cnt);
        *line_cnt += 1;
        printf("%d: O %d: molecule %d created\n", *line_cnt, *oxy_cnt, *molecule_cnt);
    }
    else {
        sem_wait(water_mutex);
    }
    */
}

void hydro_process(int hydro_atom_cnt, Params params) {

    wait_random_time(params.TI);

    //going to queue
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: H %d: going to queue\n", *line_cnt, hydro_atom_cnt);
    fflush(fp);
    *hydro_bar_cnt += 1;
    sem_post(wait_mutex);

    //vodik ceka ve fronte vodiku
    sem_wait(hydro_queue);

}


void process_gen(int *line_cnt, int *oxy_cnt, int *hydro_cnt, Params params) {
        pid_t pid_hydro;
        pid_t pid_oxy;
        pid_t id = fork();

        //child - vodik proces
        if(id == 0) {
            for(int i = 1; i <= params.NH; i++) {
                pid_hydro = fork();
                if(pid_hydro == 0) {
                    //started
                    sem_wait(wait_mutex);
                    *line_cnt += 1;
                    *hydro_cnt += 1;
                    fprintf(fp, "%d: H %d: started\n", *line_cnt, *hydro_cnt);
                    fflush(fp);
                    sem_post(wait_mutex);
                    hydro_process(i, params);
                    exit(0);
                }else if (pid_hydro < 0) {
                    perror("error making hydrogen process\n");
                    clean_all();
                    exit(1);
                }
            }
        }

        //parent - kyslik proces
        else if (id > 0) {
            for(int j = 1; j <= params.NO; j++) {
                pid_oxy = fork();
                if(pid_oxy == 0) {
                    //started
                    sem_wait(wait_mutex);
                    *line_cnt += 1;
                    *oxy_cnt += 1;
                    fprintf(fp, "%d: O %d: started\n", *line_cnt, *oxy_cnt);
                    fflush(fp);
                    sem_post(wait_mutex);
                    oxy_process(j, params);
                    exit(0);
                } else if (pid_oxy < 0) {
                    perror("error making oxygen process\n");
                    clean_all();
                    exit(1);
                }
            }
        }
        else {
            perror("error making process\n");
            clean_all();
            exit(1);
        }
    return;
}



int main(int argc, char **argv) {

    Params params;
    get_params(argc, argv, &params);

    semaphores_init();

    //vytvori se .out soubor pro zapisovani vysledku
    fp = fopen("proj2.out", "w");
    if(fp == NULL) {
        perror("cant open .out file\n");
        exit(1);
    }

    SHARED_MEM_INIT(line_cnt);
    SHARED_MEM_INIT(oxy_cnt);
    SHARED_MEM_INIT(hydro_cnt);
    SHARED_MEM_INIT(oxy_bar_cnt);
    SHARED_MEM_INIT(hydro_bar_cnt);
    SHARED_MEM_INIT(molecule_cnt);

    process_gen(line_cnt, oxy_cnt, hydro_cnt, params);




    //while (wait(NULL) > 0);

    clean_all();

    exit(0);
}