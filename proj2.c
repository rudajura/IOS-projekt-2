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
#define SEM_NAME4 "/xjuris02.sem.counter_mutex"

#define SHARED_MEM_INIT(var) {(var) = mmap(NULL, sizeof(*(var)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}


int *oxy_cnt = NULL;
int *hydro_cnt = NULL;
int *line_cnt = NULL;
int *oxy_bar_cnt = NULL;
int *hydro_bar_cnt = NULL;
int *molecule_cnt = NULL;

sem_t *oxy_queue = NULL;
sem_t *hydro_queue = NULL;
sem_t *water_mutex = NULL;
sem_t *counter_mutex = NULL;

FILE *fp;


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

    if ((counter_mutex = sem_open(SEM_NAME4, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
		perror("semaphore init failed\n");
		exit(1);
	}
}




// sem_post() - inkrementovat hodnotu semaforu
// sem_wait() - dekrementovat
//pokud je hodnota semaforu 0, sem_wait bude blokovat, dokud nebude vetsi jak 0
//hodnota semaforu nesmi byt mensi jak 0 - jinak vypsat chybove hlaseni, uvolnit alokovane zdroje a ukoncit s error(1)

//nepouzivat wait_pid - aktivni cekani



/*probouzeni - kdyz jakykoliv z jeho child procesu zmeni svuj stav -> returne se 

pousteni urciteho poctu procesu - sem_post - tak, jak to ma ve funkci gen_riders (for loop nebo while loop)
*/




/*
struct sMonitor{
  semaphore Mutex, Cond, Prio;
  int condCount, prioCount;
}
void monitorInit(struct sMonitor *m){
  init(m->Mutex, 1);
  init(m->Cond, 0);
  init(m->Prio, 0);
  m->condCount = m->prioCount = 0;
}
void monitorEnter(struct sMonitor *m){
  lock(m->Mutex);
}
void condWait(struct sMonitor *m){
  m->condCount++;
  if(m->prioCount > 0) unlock(m->Prio)
  else unlock(m->Mutex)
  lock(m->Cond);
  m->condCount--;
}
void condSignal(struct sMonitor *m){
  if(m->condCount > 0){
    m->prioCount++;
    unlock(m->Cond);
    lock(m->Prio);
    m->prioCount--;
  }
}
void monitorExit(struct sMonitor *m){
  if(m->prioCounter > 0) unlock(m->Prio);
  else unlock(m->Mutex);
}

*/


/*
struct sBarrier{
    int waiting;
    semaphore enter, wait, leave;
}

void initBarrier(struct sBarier *b){
    init(b->enter, 1);
    init(b->wait, 0);
    init(b->leave, 0);
    b->waiting = 0;
}

void barrier(struct sBarier *b)
{
    lock(enter);
    b->waiting++;
    if(b->waiting < N){
        unlock(b->enter);
        lock(b->wait);
        unlock(b->leave);
    }
    else{
        for(int i = 0; i < N-1; i++){
            unlock(b->wait);
            lock(b->leave);
        }
        b->waiting = 0;
        unlock(b -> enter);
    }
};

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


void oxy_process(int molec_time) {
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
        wait_random_time(molec_time);
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
}


void process_gen(int *line_cnt, int *oxy_cnt, int *hydro_cnt, int oxygen_total, int hydro_total, int queue_time, int molec_time) {
    while(true) {
        pid_t idO = fork();
        (void) molec_time;

        //child - vodik proces
        if(idO == 0) {
            //pokud se uz nacetly vsechny vodiky, dalsi se uz nebudou nacitat
            if(*hydro_cnt >= hydro_total) {
                break;
            }

            *line_cnt += 1;
            *hydro_cnt += 1;
            //vyresit tisknuti do souboru - zapsat nejak jinak
            //fprintf(fp, "%d: H %d: started\n", *line_cnt, *hydro_cnt);
            //fflush(fp);       toto tam mit za kazdym zapisem do souboru
            printf("%d: H %d: started\n", *line_cnt, *hydro_cnt);

            wait_random_time(queue_time);
            *line_cnt += 1;
            printf("%d: H %d: going to queue\n", *line_cnt, *hydro_cnt);
            *hydro_bar_cnt += 1;
            sem_wait(hydro_queue);  //za semaforem hydro_queue cekaji vytvorene vodiky


            

        }

        //parent - kyslik proces
        else {
            //pokud se uz nacetly vsechny kysliky, dalsi se uz nebudou nacitat
            if(*oxy_cnt >= oxygen_total) {
                break;
            }
            *line_cnt += 1;
            *oxy_cnt += 1;
            //fprintf(fp, "%d: O %d: started\n", *line_cnt, *oxy_cnt);
            printf("%d: O %d: started\n", *line_cnt, *oxy_cnt);

            wait_random_time(queue_time);
            *line_cnt += 1;
            printf("%d: O %d: going to queue\n", *line_cnt, *oxy_cnt);
            *oxy_bar_cnt += 1;
            //sem_post(hydro_queue);
            //sem_wait(oxy_queue);  //za semaforem oxy_queue cekaji vytvorene kysliky

            //oxy_process(molec_time);
        }
    }
    return;
}








/*
typedef struct {
    int line_cnt;
    int end;
    int oxy_
}
*/



int main(int argc, char **argv) {
    
    if (argc != PARAM_COUNT) {
        perror("invalid count of parameters\n");
        exit(1);
    }

    //ulozeni vstupnich parametru do promennych pro pohodlnejsi praci s nimi
    int NO = strtol(argv[1], NULL, 10);     //pocet kysliku
    int NH = strtol(argv[2], NULL, 10);     //pocet vodiku
    int TI = strtol(argv[3], NULL, 10) * 1000;      //maximalni cas v mikrosekundach, po ktery atom kysliku/vodiku ceka,
                                                    //nez se zaradi do fronty na vytvareni molekul
    int TB = strtol(argv[4], NULL, 10) * 1000;      //maximalni cas v mikrosekundach nutny pro vytvoreni jedne molekuly

    if(TI < 0 || TB < 0 || NO < 0 || NH < 0) {
        perror("invalid parameters\n");
        exit(1);
    }

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

    process_gen(line_cnt, oxy_cnt, hydro_cnt, NO, NH, TI, TB);




    //while (wait(NULL) > 0);

    clean_all();

    exit(0);
}