#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include "sem.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>


// COMPILE COMMANDS: THOTH
// gcc -m32 -o aptsim -I /afs/pitt.edu/home/m/p/mpc57/private/linux-2.6.23.1/include/ aptsim.c
// DOWNLOAD COMMANDS: QEMU
// scp mpc57@thoth.cs.pitt.edu:~/private/aptsim .
// RUNNING TEST COMMAND:
// ./aptsim -m 5 -k 2 -pt 70 -dt 2 -pa 70 -da 2

void down(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_down, sem);
}

void up(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_up, sem);
}

int * getArgs(int argc, char **argv);
int arrayContains(char **argv, char *key, int argc);
int tenantCreate(int pid, clock_t begin, int * options);
int agentCreate(int pid, clock_t begin, int * options);
int tenantArrives(clock_t begin, int num);
int agentArrives(clock_t begin, int num);
int printOptions(int * options);
int agentProcess(clock_t begin, int num);
int tenantProcess(clock_t begin, int num, int x);
int viewApt(clock_t begin, int num);
int openApt(clock_t begin, int num);
int agentLeaves(clock_t begin, int num);
int tenantLeaves(clock_t begin, int num, int x);
int addPID();
int acquire(struct cs1550_sem *sem);
int lock(struct cs1550_sem *sem);
int signalMax(struct cs1550_sem *sem);

// global variables
int *pidArr;
int *pidCount;
int *tenantCount;
int *agentCount;
int *currTenantsCount;
int *maxTenantsCount;
struct cs1550_sem *mutex;
struct cs1550_sem *no_tenant;
struct cs1550_sem *another_agent;
struct cs1550_sem *inspecting_tenants;
struct cs1550_sem *no_agent;
struct cs1550_sem *max_tenants;
#define MAX 10

int main(int argc, char **argv) {
  int *options = getArgs(argc, argv);
  //printOptions(options);
  // shared memory of variables / semaphores
  pidArr = mmap(NULL,sizeof(int) * 100, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  pidCount = mmap(NULL,sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  *pidCount = 0;
  tenantCount = mmap(NULL,sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  *tenantCount = 0;
  agentCount = mmap(NULL,sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  *agentCount = 0;
  currTenantsCount = mmap(NULL,sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  *currTenantsCount = 0;
  maxTenantsCount = mmap(NULL,sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  *maxTenantsCount = 0;
  mutex = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  no_tenant = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  another_agent = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  inspecting_tenants = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  no_agent = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  max_tenants = mmap(NULL,sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  // initialize semaphores
  mutex->value = 1;
  no_tenant->value = 0;
  another_agent->value = 1;
  inspecting_tenants->value = 0;
  no_agent->value = 0;
  max_tenants->value = 0;

  printf("The apartment is now empty.\n");
  int pid = fork();
  clock_t begin = time(NULL);
  if (pid == 0) {
    addPID();
    agentCreate(pid, begin, options);
  } else {
    tenantCreate(pid, begin, options);
    int i;
    for (i = 0; i < *pidCount; i++) { // wait for ALL children to finish before exiting
      waitpid(pidArr[i], NULL, WUNTRACED);
    }
  }
  return 0;
}

int * getArgs(int argc, char **argv) {
  static int args[8];
  char * m = "-m";    // 0 -> number of tenants
  char * k = "-k";    // 1 -> number of agents
  char * pt = "-pt";  // 2 -> probability of a tenant arriving after another tenant
  char * dt = "-dt";  // 3 -> time delay between arriving tenants
  char * st = "-st";  // 4 -> random seed for tenant probability
  char * pa = "-pa";  // 5 -> probability of an agent arriving after another agent
  char * da = "-da";  // 6 -> time delay between arriving agents
  char * sa = "-sa";  // 7 -> random seed for agent probability
  char * cmdFlags[] = {m, k, pt, dt, st, pa, da, sa};

  int flagNumber = 7;
  while (flagNumber >= 0) {
    int indexFound;
    if ((indexFound = arrayContains(argv, cmdFlags[flagNumber], argc)) != -1
      && indexFound + 1 < argc) {
      // get the next argument, set it to the flag value
      args[flagNumber] = atoi(argv[indexFound + 1]);
    } else {
      args[flagNumber] = 0;
    }
    flagNumber --;
  }
  return args;
}

int arrayContains(char **argv, char *key, int argc) {
  int i;
  for (i = 1; i < argc; i ++) { // skip the first arg (argv[0] = './aptsim')
    if (strstr(argv[i], key) != NULL) {
      return i; // return the index of where the string was found
    }
  }
  return -1; // flag if array does not contain key
}

int printOptions(int * options) {
  printf("The number of tenants (-m) is %d\n", options[0]);
  printf("The number of agents (-k) is %d\n", options[1]);
  printf("The probability of a tenant arriving after another tenant (-pt) is %d\n", options[2]);
  printf("The time delay between arriving tenants (-dt) is %d\n", options[3]);
  printf("The random tenant seed (-st) is %d\n", options[4]);
  printf("The probability of a agent arriving after another agent (-pa) is %d\n", options[5]);
  printf("The time delay between arriving agents (-da) is %d\n", options[6]);
  printf("The random agent seed (-sa) is %d\n\n", options[7]);
}

int tenantCreate(int pid, clock_t begin, int * options) {
  if (options[0] == 0) { return; }
  srand(options[4]);
  int num = 0;
  do {
    int pid = fork();
    if (pid == 0) {
      addPID();
      tenantProcess(begin, num, options[0]);
      exit(0);
    }
    if ((rand() % 100) >= options[2] && num < options[0] - 1) {
        // if the rand() falls OUTSIDE [0, prob], sleep()
        sleep(options[3]);
    }
  } while (++num < options[0]);
  return 0;
}

int agentCreate(int pid, clock_t begin, int * options) {
  if (options[1] == 0) { return; }
  srand(options[7]);
  int num = 0;
  do {
    int pid = fork();
    if (pid == 0) {
      addPID();
      agentProcess(begin, num);
      exit(0);
    }
    if ((rand() % 100) >= options[5] && num < options[1] - 1) {
        // if the rand() falls OUTSIDE [0, prob], sleep()
        sleep(options[6]);
    }
  } while (++num < options[1]);
  return 0;
}

int tenantArrives(clock_t begin, int num) {
  down(mutex);
  printf("Tenant %d arrives at time %f.\n", num, (double)(time(NULL) - begin));
  *tenantCount += 1;
  signal(no_tenant);
  up(mutex);
  return 0;
}

int agentArrives(clock_t begin, int num) {
  down(mutex);
  printf("Agent %d arrives at time %f.\n", num, (double)(time(NULL) - begin));
  signal(no_agent);
  up(mutex);
  return 0;
}

int viewApt(clock_t begin, int num) {
  down(mutex);
  if (*maxTenantsCount == MAX) { // we cannot assign this tenant to the current agent, he has already seen his max
    //printf("\tDEBUG: Tenant %d waiting because current agent has seen all %d tenants\n", num, *maxTenantsCount);
    acquire(max_tenants); // begin waiting for next agent
  }

  if (*agentCount == 0) { // must wait for at least one agent
    //printf("\tDEBUG: Tenant %d waiting for an agent before viewing\n", num);
    acquire(no_agent);
  }

  *maxTenantsCount += 1; // keeps track of how many tenants can still be handled by the current agent
  *currTenantsCount += 1; // keeps track of how many tenants are mid-inspection
  up(mutex);
  sleep(2); // inspecting...
  down(mutex);
  printf("Tenant %d inspects the apartment at time %f.\n", num, (double)(time(NULL) - begin));
  up(mutex);
  return 0;
}

int openApt(clock_t begin, int num) {
  down(mutex);

  acquire(another_agent); // check to see if there is an agent in the room.  If so WAIT

  if (*tenantCount == 0) { // must wait for at least one tenant
    //printf("\tDEBUG: Agent %d waiting for a tenant before opening\n", num);
    acquire(no_tenant);
  }

  signal(no_agent); // all processes waiting for an agent can now WAKE
  signalMax(max_tenants); // Wake up the next batch of tenants waiting for another agent
  printf("Agent %d opens the apartment for inspection at time %f.\n", num, (double)(time(NULL) - begin));
  *maxTenantsCount = 0; // reset the maximum tenants this new agent can see over his lifetime
  *agentCount += 1;
  up(mutex);
  return 0;
}

int tenantLeaves(clock_t begin, int num, int x) {
  down(mutex);
  printf("Tenant %d leaves the apartment at time %f.\n", num, (double)(time(NULL) - begin));
  *tenantCount -= 1;
  *currTenantsCount -= 1;

  if (*currTenantsCount == 0) { // if there are no more tenants CURRENTLY viewing with the current agent...
    //printf("\tDEBUG: Tenant %d is the last tenant to leave, allow current agent to leave\n", num);
    up(inspecting_tenants); // the current agent waiting for all CURRENT VIEWING TENANTS to leave can now leave
  }

  up(mutex);
  return 0;
}

int agentLeaves(clock_t begin, int num) {
  down(mutex);
  //printf("\tDEBUG: Agent %d has seen %d out of %d tenants\n", num, *maxTenantsCount, MAX);
  // if (*maxTenantsCount < MAX) { // if you haven't seen 10 tenants yet... WAIT
  //   printf("\tDEBUG:Agent %d waits.  He has only seen %d of all %d of his tenants\n", num, *maxTenantsCount, MAX);
  //   acquire(max_tenants); // NOTE: Upon all current tenants viewing leaving, this is WOKEN
  // }

  //printf("\tDEBUG:Agent %d waits.  He he can't leave until all %d tenants leave\n", num, *currTenantsCount);
  up(mutex);
  down(inspecting_tenants); // you must WAIT for all tenants to leave
  down(mutex);

  printf("Agent %d leaves the apartment at time %f.\n", num, (double)(time(NULL) - begin));
  *agentCount -= 1; // used for the tenants who need at least one agent
  //printf("\tDEBUG:New agent count is %d\n", *agentCount);
  //printf("\tDEBUG:Current max tenant count is %d\n", *maxTenantsCount);
  up(another_agent); // wakeup any fellow agents waiting for you to leave
  up(mutex);
  return 0;
}

int agentProcess(clock_t begin, int num) {
  agentArrives(begin, num);
  openApt(begin, num);
  agentLeaves(begin, num);
  return 0;
}

int tenantProcess(clock_t begin, int num, int x) {
  tenantArrives(begin, num);
  viewApt(begin, num);
  tenantLeaves(begin, num, x);
  return 0;
}

int addPID() {
  down(mutex);
  int childPID = getpid();
  pidArr[*pidCount] = childPID;
  *pidCount += 1;
  up(mutex);
  return 0;
}

int signal(struct cs1550_sem *sem) {
  if (sem->value < 0) { // leave at 0 if nobody needs it
    int totalWaits = -(sem->value);
    //printf("\tDEBUG:Waking up %d processes\n", totalWaits);
    int i;
    for (i = 0; i < totalWaits; i ++) { // wakeup ALL processes waiting
      up(sem);
    }
  }
}

int signalMax(struct cs1550_sem *sem) {
  if (sem->value < 0) { // leave at 0 if nobody needs it
    int totalWaits = -(sem->value);
    if (totalWaits >= MAX) {
      totalWaits = MAX;
    } else {
      totalWaits = totalWaits % MAX;
    }
    //printf("\tDEBUG: Waking up %d tenants\n", totalWaits);
    int i;
    for (i = 0; i < totalWaits; i ++) { // wakeup ALL processes waiting
      up(sem);
    }
  }
}

int acquire(struct cs1550_sem *sem) {
  up(mutex);
  down(sem); // wait for this semaphore
  down(mutex);
}
