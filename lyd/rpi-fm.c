/*
FM output Code based on code originally from

http://www.icrobotics.co.uk/wiki/index.php/Turning_the_Raspberry_Pi_Into_an_FM_Transmitter

*/


#define RPI_BUF_SIZE  1
int rpi_sample_rate = 9000; /* we don't need to obey any standard, just
                                be low enough that we have enough headroom
                                to outweigh most glitches
                              */

static float stochastic_iters = 450;  /* this is auto-tuned on the fly */


#define RPI_N_ADJUST            128   /* auto adjust every N samples proceesed*/


#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#include <alloca.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

//added by rgrasell@gmail.com
int freq_const;
//added by rgrasell@gmail.com

int  mem_fd;
unsigned char *gpio_mem, *gpio_map;
unsigned char *spi0_mem, *spi0_map;

// I/O access
volatile unsigned *gpio;
volatile unsigned *allof7e;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(gpio+13) // sets   bits which are 1 ignores bits which are 0

#define ACCESS(base) *(volatile int*)((int)allof7e+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

void setup_io();

#define CM_GP0CTL (0x7e101070)
#define GPFSEL0 (0x7E200000)
#define CM_GP0DIV (0x7e101074)
#define DMABASE (0x7E007000)

struct GPCTL {
    char SRC         : 4;
    char ENAB        : 1;
    char KILL        : 1;
    char             : 1;
    char BUSY        : 1;
    char FLIP        : 1;
    char MASH        : 2;
    unsigned int     : 13;
    char PASSWD      : 8;
};

void fm_setup(float frequency)
{
    setup_io();

    float temp = 500.0 / frequency;
    temp *= (16*16*16);
    freq_const = (int)temp;

    allof7e = (unsigned *)mmap(
                  NULL,
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x20000000  //base
              );

    if ((int)allof7e==-1) exit(-1);

    SETBIT(GPFSEL0 , 14);
    CLRBIT(GPFSEL0 , 13);
    CLRBIT(GPFSEL0 , 12);
 
    struct GPCTL setupword = {6/*SRC*/, 1, 0, 0, 0, 1,0x5a};
    ACCESS(CM_GP0CTL) = *((int*)&setupword);
}

//function slightly modified by rgrasell@gmail.com to allow for changing frequencies
static inline void fm_modulate(int m)
{
    ACCESS(CM_GP0DIV) = (0x5a << 24) + freq_const  + m;
}

static struct timeval start_time;

#define usecs(time)    ((time.tv_sec - start_time.tv_sec) * 1000000 + time.tv_usec)

static int sample_no = 0;

static void
init_ticks (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;
  gettimeofday (&start_time, NULL);
}

static inline long
get_ticks (void)
{
  struct timeval measure_time;
  gettimeofday (&measure_time, NULL);
  return usecs (measure_time) - usecs (start_time);
}

static void sample_adjust (void)
{
  static int corrective = 1;
  static int first = 1;
  static int prev_ticks = 0;
  long elapsed;
  if (first)
    {
      prev_ticks = get_ticks ();
      first = 0;
    }
  else
    {
      long ticks = get_ticks ();
      elapsed = ticks - prev_ticks;
     
      float play_rate = (RPI_N_ADJUST * 1.0) / (elapsed / 1000000.0);
      float target_rate = rpi_sample_rate;
      float diff, adiff;

      diff = target_rate - play_rate;
      adiff = diff > 0?diff:-diff;

      /* if difference between actual sample rate and desired sample rate
         is larger than 5% start correcting
       */

      if (adiff / target_rate > 0.05)
        corrective = 1;
      if (corrective)
        {
          if (adiff / target_rate < 0.001)
            {
              corrective = 0;
            }
          else
            {
              corrective++;
              if (diff < 0)
                stochastic_iters *= 1.005;
              else
                stochastic_iters *= 0.995;
            }
        }

      prev_ticks = ticks;
    }
}

static inline void fm_play_sample (float sample)
{
  float dval = sample*25.0;
  int intval = (int)((dval));
  float frac = dval - (float)intval;
  unsigned int fracval = (unsigned int)(frac*((float)(1<<16))*((float)(1<<16)));
  static unsigned int rnd=1;
  int i;

  for (i=0; i<stochastic_iters; i++)
  {
    rnd = (rnd >> 1) ^ (-(rnd & 1u) & 0xD0000001u);
    fm_modulate( intval + (fracval>rnd?1:0));
  }

  sample_no++;
  if (sample_no % RPI_N_ADJUST == 0)
    sample_adjust ();
}

void playWav(char* filename)
{
    int fp = open(filename, 'r');
    int sz = lseek(fp, 0L, SEEK_END);
    int j;
    lseek(fp, 0L, SEEK_SET);
    short* data = (short*)malloc(sz);
    read(fp, data, sz);
    for (j=22; j<sz/2; j++)
      fm_play_sample((float)(data[j])/65536.0);
}

struct CB {
    unsigned int TI;
    unsigned int SOURCE_AD;
    unsigned int DEST_AD;
    unsigned int TXFR_LEN;
    unsigned int STRIDE;
    unsigned int NEXTCONBK;
};

#include <stdio.h>

// Set up a memory regions to access GPIO
//
void setup_io()
{
    int g;
    init_ticks ();
    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit (-1);
    }
    /* mmap GPIO */
    // Allocate MAP block
    if ((gpio_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
        printf("allocation error \n");
        exit (-1);
    }
    // Make sure pointer is on 4K boundary
    if ((unsigned long)gpio_mem % PAGE_SIZE)
        gpio_mem += PAGE_SIZE - ((unsigned long)gpio_mem % PAGE_SIZE);
    // Now map it
    gpio_map = (unsigned char *)mmap(
                   gpio_mem,
                   BLOCK_SIZE,
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED|MAP_FIXED,
                   mem_fd,
                   GPIO_BASE
               );
    if ((long)gpio_map < 0) {
        printf("mmap error %d\n", (int)gpio_map);
        exit (-1);
    }
    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;

    // Switch GPIO 7..11 to output mode
    /************************************************************************\
     * You are about to change the GPIO settings of your computer.          *
     * Mess this up and it will stop working!                               *
     * It might be a good idea to 'sync' before running this program        *
     * so at least you still have your code changes written to the SD-card! *
    \************************************************************************/
    // Set GPIO pins 7-11 to output
    for (g=7; g<=11; g++) {
        INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
    }
} // setup_io


#ifndef HAVE_PIFM
int main(int argc, char **argv)
{
    fm_setup(108);
    fm_modulate(0);
    rpi_sample_rate = 22050;
    stochastic_iters = 270;

   //if statement modified by rgrasell@gmail.com to check for new argument, and to set freq_const
    if (argc==3){
      float temp = 500.0 / atof(argv[2]);
      temp *= (16*16*16);
      freq_const = (int)temp;
      playWav(argv[1]);
   }
    else
      fprintf(stderr, "Usage:   program wavfile.wav frequency \n\nWhere wavfile is 16 bit 44.1kHz Monon and frequency is a floating point number\n");
    
    return 0;
} // main
#endif

