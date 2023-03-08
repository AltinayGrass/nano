/* gcc -pthread -o dispatch dispatch.c */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <sysexits.h>
#include <glob.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <fcntl.h>

/****************************************************************************/

// Application parameters
#define FREQUENCY 500
#define CLOCK_TO_USE CLOCK_MONOTONIC
//#define MEASURE_TIMING 1

/****************************************************************************/
#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
        (B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/****************************************************************************/

static unsigned int counter = 0;
const struct timespec cycletime = {0, PERIOD_NS};

/****************************************************************************/

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    } else {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

/****************************************************************************/

int keycode_of_key_being_pressed() { 
  FILE *kbd;
  glob_t kbddev;                                   // Glob structure for keyboard devices
  glob("/dev/input/by-path/*-kbd", 0, 0, &kbddev); // Glob select all keyboards
  int keycode = -1;                                // keycode of key being pressed
  for (int i = 0; i < kbddev.gl_pathc ; i++ ) {    // Loop through all the keyboard devices ...
    if (!(kbd = fopen(kbddev.gl_pathv[i], "r"))) { // ... and open them in turn (slow!)
      perror("Run as root to read keyboard devices"); 
      exit(1);      
    }

    char key_map[KEY_MAX/8 + 1];          // Create a bit array the size of the number of keys
    memset(key_map, 0, sizeof(key_map));  // Fill keymap[] with zero's
    ioctl(fileno(kbd), EVIOCGKEY(sizeof(key_map)), key_map); // Read keyboard state into keymap[]
    for (int k = 0; k < KEY_MAX/8 + 1 && keycode < 0; k++) { // scan bytes in key_map[] from left to right
      for (int j = 0; j <8 ; j++) {       // scan each byte from lsb to msb
        if (key_map[k] & (1 << j)) {      // if this bit is set: key was being pressed
          keycode = 8*k + j ;             // calculate corresponding keycode 
          break;                          // don't scan for any other keys
        }
      }   
    }

    fclose(kbd);
    if (keycode)
      break;                              // don't scan for any other keyboards
  }
  return keycode;
}

#define BUFFER_SIZE 1024


static int run1=1;
int32_t vel1=0;
int32_t vel2=0;

void *keyboardListener(void *);

int main() {
    pthread_t tid;
    char buffer[BUFFER_SIZE];

    int ret = 0;
    int count=0;
    struct timespec wakeupTime, time;

    struct timespec startTime, endTime, lastStartTime = {};
    u_int32_t period_ns = 0, exec_ns = 0, latency_ns = 0,
             latency_min_ns = 0, latency_max_ns = 0,
             period_min_ns = 0, period_max_ns = 0,
             exec_min_ns = 0, exec_max_ns = 0;
    
    
    // Klavye dinleyen thread'i başlat
    if (pthread_create(&tid, NULL, keyboardListener, NULL) != 0) {
        fprintf(stderr, "Klavye dinleyici thread'i başlatılamadı.\n");
        exit(EXIT_FAILURE);
    }
 
    // get  time
    clock_gettime(CLOCK_TO_USE, &wakeupTime);
    
    while (run1) {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        
        clock_gettime(CLOCK_TO_USE, &startTime);
        latency_ns = DIFF_NS(wakeupTime, startTime);
        period_ns = DIFF_NS(lastStartTime, startTime);
        exec_ns = DIFF_NS(lastStartTime, endTime);
        lastStartTime = startTime;

        if (latency_ns > latency_max_ns) {
            latency_max_ns = latency_ns;
        }
        if (latency_ns < latency_min_ns) {
            latency_min_ns = latency_ns;
        }
        if (period_ns > period_max_ns) {
            period_max_ns = period_ns;
        }
        if (period_ns < period_min_ns) {
            period_min_ns = period_ns;
        }
        if (exec_ns > exec_max_ns) {
            exec_max_ns = exec_ns;
        }
        if (exec_ns < exec_min_ns) {
            exec_min_ns = exec_ns;
        }

        if (counter) {
            counter--;
        } else { // do this at 1 Hz
            counter = FREQUENCY;
            // output timing stats
            printf("period     %10u ... %10u\n",
                    period_min_ns, period_max_ns);
            printf("exec       %10u ... %10u\n",
                    exec_min_ns, exec_max_ns);
            printf("latency    %10u ... %10u\n",
                    latency_min_ns, latency_max_ns);
            printf("vel 1: %d  vel 2: %d\n",
                    vel1, vel2);
            period_max_ns = 0;
            period_min_ns = 0xffffffff;
            exec_max_ns = 0;
            exec_min_ns = 0xffffffff;
            latency_max_ns = 0;
            latency_min_ns = 0xffffffff;

        // calculate new process data
        // Klavyeden girilen metni oku
        // fgets(buffer, BUFFER_SIZE, stdin);

        // "exit" girilirse programı sonlandır
        //if (strcmp(buffer, "exit\n") == 0) {
        //    break;
        //    }

        // Klavyeden girilen metni ekrana yazdır
        //printf("[%lu] Girilen metin: %s \r", wakeupTime.tv_nsec, buffer);
        clock_gettime(CLOCK_TO_USE, &endTime);
        }

    }

    // Klavye dinleyen thread'i sonlandır
    pthread_cancel(tid);
    return 0;
}

void *keyboardListener(void *arg) {
    setvbuf(stdout, NULL, _IONBF, 0); // Set stdout unbuffered
    while (1) {
        int c = keycode_of_key_being_pressed();
        // "exit" girilirse programı sonlandır
        if (c == KEY_Q) break;
        if (c == KEY_W) vel1+=1;
        if (c == KEY_S) vel1-=1;   
        if (c == KEY_E) vel2+=1;
        if (c == KEY_D) vel2-=1;   
        if (c == KEY_0) {
            vel1=0;
            vel2=0;
        }
    usleep(50000);

    }
    
    run1=0;
    pthread_exit(NULL);
}
