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
#define SEM_NAME5 "/xjuris02.sem.hydro_barrier"
#define SEM_NAME6 "/xjuris02.sem.hydro_mol_queue"
#define SEM_NAME7 "/xjuris02.sem.oxy_mol_queue"

//vytvori sdilenou pamet s pocatecni hodnotou 0
#define SHARED_MEM_INIT(var) {(var) = mmap(NULL, sizeof(*(var)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}

//sdilene pameti
int *oxy_cnt = NULL;
int *hydro_cnt = NULL;
int *line_cnt = NULL;
int *oxy_bar_cnt = NULL;
int *hydro_bar_cnt = NULL;
int *molecule_cnt = NULL;
int *water_barrier = NULL;
int *making_water = NULL;
int *oxy_passed = NULL;
int *hydro_passed = NULL;

//semafory
sem_t *oxy_queue = NULL;
sem_t *hydro_queue = NULL;
sem_t *water_mutex = NULL;
sem_t *wait_mutex = NULL;
sem_t *hydro_barrier = NULL;
sem_t *hydro_mol_queue = NULL;
sem_t *oxy_mol_queue = NULL;

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
    sem_unlink(SEM_NAME5);
    sem_unlink(SEM_NAME6);
    sem_unlink(SEM_NAME7);

    //semafor, za kterym se bude tvorit rada s vytvorenymi kysliky
    if ((oxy_queue = sem_open(SEM_NAME1, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //semafor, za kterym se bude tvorit rada s vytvorenymi vodiky
    if ((hydro_queue = sem_open(SEM_NAME2, O_CREAT | O_EXCL, 0666, 2)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //mutex, znacici, zda se muze tvorit molekula vody, ci nikoliv
    if ((water_mutex = sem_open(SEM_NAME3, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //mutex, pokud se neco zapisuje nebo meni, ostatni procesy jsou zastavene
    if ((wait_mutex = sem_open(SEM_NAME4, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //semafor, cekaji za nim vodiky tvorici molekulu, az jim da kyslik vedet, ze je molekula dokoncena
    if ((hydro_barrier = sem_open(SEM_NAME5, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //vodiky cekajici ve fronte na tvorbu vody
    if ((hydro_mol_queue = sem_open(SEM_NAME6, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}

    //kysliky cekajici ve fronte na tvorbu vody
    if ((oxy_mol_queue = sem_open(SEM_NAME7, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
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

//funkce na vygenerovani nahodne delky cekani z intervalu - parametr max_time je bud TI, nebo TB
void wait_random_time(int max_time) {
    usleep(rand()%(max_time+1));
}

//funkce na uzavreni vseho, co bylo vytvoreno (otevreno) v prubehu programu
void clean_all() {

    //smazani alokovanych sdilenych pameti
    munmap(oxy_cnt, sizeof(oxy_cnt));
    munmap(line_cnt, sizeof(line_cnt));
    munmap(hydro_cnt, sizeof(hydro_cnt));
    munmap(oxy_bar_cnt, sizeof(oxy_bar_cnt));
    munmap(hydro_bar_cnt, sizeof(hydro_bar_cnt));
    munmap(molecule_cnt, sizeof(molecule_cnt));
    munmap(water_barrier, sizeof(water_barrier));
    munmap(making_water, sizeof(making_water));
    munmap(oxy_passed, sizeof(oxy_passed));
    munmap(hydro_passed, sizeof(hydro_passed));

    //zruseni vsech semaforu
    sem_unlink(SEM_NAME1);
    sem_unlink(SEM_NAME2);
    sem_unlink(SEM_NAME3);
    sem_unlink(SEM_NAME4);
    sem_unlink(SEM_NAME5);
    sem_unlink(SEM_NAME6);
    sem_unlink(SEM_NAME7);

    //zavreni souboru
    fclose(fp);
}

//zavola se pri chybe tvoreni procesu (vsechny procesy, semafory i pamet se ukonci)
void end_all(pid_t proc_id) {
    clean_all();
    kill(proc_id, SIGTERM);
    kill(getpid(), SIGTERM);
    exit(1);
}

//proces pro kyslik
void oxy_process(int oxy_atom_cnt, Params params, int max_molecules) {

    wait_random_time(params.TI);

    //going to queue
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: O %d: going to queue\n", *line_cnt, oxy_atom_cnt);
    fflush(fp);
    *oxy_bar_cnt += 1;
    sem_post(wait_mutex);
    //kyslik ceka v rade
    sem_wait(oxy_queue);

    //pokud uz jsou vytvorene vsechny mozne molekuly, dalsi se uz nevytvori
    if(*molecule_cnt >= max_molecules) {
        sem_wait(wait_mutex);
        *line_cnt += 1;
        fprintf(fp, "%d: O %d: not enough H\n", *line_cnt, oxy_atom_cnt);
        fflush(fp);
        sem_post(oxy_queue);
        sem_post(wait_mutex);
        return;
    }

    sem_wait(water_mutex);
    sem_wait(wait_mutex);
    *oxy_passed += 1;
    if(*hydro_passed >= 2) {
        *molecule_cnt += 1;
        sem_post(hydro_mol_queue);
        sem_post(hydro_mol_queue);
        *hydro_bar_cnt -= 2;
        sem_post(oxy_mol_queue);
        *oxy_bar_cnt -= 1;
        *hydro_passed -= 2;
        *oxy_passed -= 1;
    }
    else {
        sem_post(water_mutex);
    }
    sem_post(wait_mutex);
    sem_wait(oxy_mol_queue);

    //creating molecule
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: O %d: creating molecule %d\n", *line_cnt, oxy_atom_cnt, *molecule_cnt);
    fflush(fp);
    sem_post(wait_mutex);

    //kyslik urcity cas vytvari molekulu a pak probudi ostatni vodiky z molekuly
    wait_random_time(params.TB);
    sem_post(hydro_barrier);
    sem_post(hydro_barrier);

    //molecule created
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: O %d: molecule %d created\n", *line_cnt, oxy_atom_cnt, *molecule_cnt);
    fflush(fp);
    *water_barrier += 1;

    //pokud vsechny atomy dokoncily tvoreni molekuly, pusti se dalsi tri atomy, ktere utvori dalsi molekulu
    if(*water_barrier == 3) {
        *water_barrier = 0;
        sem_post(oxy_queue);
        sem_post(hydro_queue);
        sem_post(hydro_queue);
        sem_post(water_mutex);
    }

    sem_post(wait_mutex);

    //konec procesu kyslik
}

//proces pro vodik
void hydro_process(int hydro_atom_cnt, Params params, int max_molecules) {

    wait_random_time(params.TI);

    //going to queue
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: H %d: going to queue\n", *line_cnt, hydro_atom_cnt);
    fflush(fp);
    *hydro_bar_cnt += 1;
    sem_post(wait_mutex);
    sem_wait(hydro_queue);

    //pokud uz jsou vytvorene vsechny mozne molekuly, dalsi se uz nevytvori
    if(*molecule_cnt >= max_molecules) {
        sem_wait(wait_mutex);
        *line_cnt += 1;
        fprintf(fp, "%d: H %d: not enough O or H\n", *line_cnt, hydro_atom_cnt);
        fflush(fp);
        sem_post(hydro_queue);
        sem_post(wait_mutex);
        return;
    }

    sem_wait(water_mutex);
    sem_wait(wait_mutex);
    *hydro_passed += 1;
    if((*hydro_passed >= 2) && (*oxy_passed >= 1)) {
        *molecule_cnt += 1;
        sem_post(hydro_mol_queue);
        sem_post(hydro_mol_queue);
        *hydro_bar_cnt -= 2;
        sem_post(oxy_mol_queue);
        *oxy_bar_cnt -= 1;
        *hydro_passed -= 2;
        *oxy_passed -= 1;
    }
    else {
        sem_post(water_mutex);
    }
    sem_post(wait_mutex);
    sem_wait(hydro_mol_queue);

    //creating molecule
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: H %d: creating molecule %d\n", *line_cnt, hydro_atom_cnt, *molecule_cnt);
    fflush(fp);
    sem_post(wait_mutex);
    //vodik ceka, dokud mu kyslik neoznami, ze je molekula dokoncena 
    sem_wait(hydro_barrier);

    //molecule created
    sem_wait(wait_mutex);
    *line_cnt += 1;
    fprintf(fp, "%d: H %d: molecule %d created\n", *line_cnt, hydro_atom_cnt, *molecule_cnt);
    fflush(fp);
    *water_barrier += 1;

    //pokud vsechny atomy dokoncily tvoreni molekuly, pusti se dalsi tri atomy, ktere utvori dalsi molekulu
    if(*water_barrier == 3) {
        *water_barrier = 0;
        sem_post(oxy_queue);
        sem_post(hydro_queue);
        sem_post(hydro_queue);
        sem_post(water_mutex);
    }
    sem_post(wait_mutex);

    //konec procesu vodik
}

//vygenerovani hlavniho procesu a vodik a kyslik procesu
void process_gen(int *line_cnt, int *oxy_cnt, int *hydro_cnt, Params params) {

    //zjistime maximalni pocet molekul, ktere lze vytvorit
    int max_molecules = (params.NO < params.NH/2) ? params.NO : params.NH/2;
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
                hydro_process(i, params, max_molecules);
                exit(0);
            } else if (pid_hydro < 0) {
                perror("error making hydrogen process\n");
                end_all(id);
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
                oxy_process(j, params, max_molecules);
                exit(0);
            } else if (pid_oxy < 0) {
                perror("error making oxygen process\n");
                end_all(id);
            }
        }
    }
    //pokud se vytvoril proces s id < 0, je to chyba a program se ukonci
    else {
        perror("error making process\n");
        end_all(id);
    }
    return;
}


int main(int argc, char **argv) {

    //ziskani vstupnich parametru
    Params params;
    get_params(argc, argv, &params);

    //inicializace semaforu
    semaphores_init();

    //vytvori se .out soubor pro zapisovani vysledku
    fp = fopen("proj2.out", "w");
    if(fp == NULL) {
        perror("cant open .out file\n");
        exit(1);
    }

    //inicializujeme sdileme pameti
    SHARED_MEM_INIT(line_cnt);
    SHARED_MEM_INIT(oxy_cnt);
    SHARED_MEM_INIT(hydro_cnt);
    SHARED_MEM_INIT(oxy_bar_cnt);
    SHARED_MEM_INIT(hydro_bar_cnt);
    SHARED_MEM_INIT(molecule_cnt);
    SHARED_MEM_INIT(water_barrier);
    SHARED_MEM_INIT(making_water);
    SHARED_MEM_INIT(hydro_passed);
    SHARED_MEM_INIT(oxy_passed);

    //samotny prubeh procesu
    process_gen(line_cnt, oxy_cnt, hydro_cnt, params);

    clean_all();

    exit(0);
}