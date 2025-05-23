#include "proxy.h"

/*
 * Thread monitor implementation to fix issue with stuck thread count
 * This periodically checks and resets the childcount if it doesn't match reality
 */

#define MONITOR_INTERVAL 60 /* Check every 60 seconds */

static pthread_t monitor_thread;
static int monitor_running = 0;

/* Count actual client threads by traversing the linked list */
static int count_active_clients(struct srvparam *srv) {
    struct clientparam *cp;
    int count = 0;
    
    if (!srv) return 0;
    
    pthread_mutex_lock(&srv->counter_mutex);
    cp = srv->child;
    while (cp) {
        count++;
        cp = cp->next;
    }
    pthread_mutex_unlock(&srv->counter_mutex);
    
    return count;
}

/* Thread monitor function that periodically checks and corrects childcount */
#ifdef _WIN32
DWORD WINAPI thread_monitor(LPVOID p) {
#else
void * thread_monitor(void *p) {
#endif
    struct srvparam *srv = (struct srvparam *)p;
    int actual_count, reported_count;
    unsigned char buf[128];
    
    while (monitor_running) {
        /* Sleep for the monitoring interval */
        usleep(MONITOR_INTERVAL * 1000000);
        
        if (!monitor_running) break;
        
        /* Count actual clients */
        actual_count = count_active_clients(srv);
        
        /* Get reported count */
        pthread_mutex_lock(&srv->counter_mutex);
        reported_count = srv->childcount;
        
        /* If counts don't match, adjust and log */
        if (actual_count != reported_count) {
            sprintf((char *)buf, "Thread count mismatch detected: reported=%d, actual=%d. Fixing.", 
                    reported_count, actual_count);
            srv->childcount = actual_count;
            
            /* Use default logging instead of creating a temporary param */
            if (!srv->silent) dolog(NULL, buf);
        }
        pthread_mutex_unlock(&srv->counter_mutex);
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* Start the thread monitor */
int start_thread_monitor(struct srvparam *srv) {
    if (monitor_running) return 1; /* Already running */
    
    monitor_running = 1;
    
#ifdef _WIN32
    HANDLE h;
    unsigned threadid;
    h = (HANDLE)_beginthreadex(NULL, 16384, (void *)thread_monitor, (void *)srv, 0, &threadid);
    if (h) {
        monitor_thread = (pthread_t)threadid;
        CloseHandle(h);
        return 0;
    }
#else
    pthread_attr_t pa;
    pthread_attr_init(&pa);
    pthread_attr_setstacksize(&pa, PTHREAD_STACK_MIN + 16384);
    pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED);
    
    if (pthread_create(&monitor_thread, &pa, thread_monitor, (void *)srv) == 0) {
        pthread_attr_destroy(&pa);
        return 0;
    }
    pthread_attr_destroy(&pa);
#endif

    monitor_running = 0;
    return -1;
}

/* Stop the thread monitor */
void stop_thread_monitor() {
    monitor_running = 0;
}