#ifndef _THREAD_MONITOR_H_
#define _THREAD_MONITOR_H_

/* 
 * Thread monitor header - provides functions to monitor thread counts
 * and fix potential issues with stuck childcount values
 */

/* Start thread monitoring for a specific service */
int start_thread_monitor(struct srvparam *srv);

/* Stop thread monitoring */
void stop_thread_monitor(void);

#endif