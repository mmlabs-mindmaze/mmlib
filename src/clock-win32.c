/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmtime.h"
#include "clock-win32.h"
#include "mmpredefs.h"
#include "utils-win32.h"

#include <windows.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <intrin.h>
#include <malloc.h>
#include <winnt.h>
#include <powrprof.h>


/**************************************************************************
 *                           Clock cycle counters                         *
 **************************************************************************/

//Not defined outside of DDK, but documented
typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;


#define PICOSEC_IN_SEC  1000000000000LL
#define MHZ_IN_HZ       1000000

static int64_t cpu_cycle_freq;
static int64_t cpu_cycle_tick_psec; // Time of a cpu cycle in picosecond

static
void determine_cpu_cycle_rate(void)
{
	int num_proc;
	SYSTEM_INFO sysinfo;
	PROCESSOR_POWER_INFORMATION* proc_pwr_info;

	// Get number of logical processor to allocate proc_pwr_info array size to the right one
	GetSystemInfo(&sysinfo);
	num_proc = sysinfo.dwNumberOfProcessors;

	// Get CPU information.
	// When requesting 'ProcessorInformation', the output receives one
	// PROCESSOR_POWER_INFORMATION structure for each processor that is
	// installed on the system
	proc_pwr_info = _alloca(num_proc*sizeof(*proc_pwr_info));
	CallNtPowerInformation(ProcessorInformation, NULL, 0,
                               proc_pwr_info, num_proc*sizeof(*proc_pwr_info));

	// Compute CPU max clock rate and tick length
	// On x86 processors supporting invariant TSC, the TSC clock rate is this one
	cpu_cycle_freq = proc_pwr_info[0].MaxMhz * MHZ_IN_HZ;
	cpu_cycle_tick_psec = PICOSEC_IN_SEC / cpu_cycle_freq;
}


static
void convert_cpu_cycle_to_ts(int64_t cpu_cycle, struct mm_timespec* ts)
{
	int64_t sec, nsec;

	sec = cpu_cycle / cpu_cycle_freq;
	cpu_cycle -= sec * cpu_cycle_freq;
	nsec = (cpu_cycle * cpu_cycle_tick_psec)/1000;

	ts->tv_sec = (time_t)sec;
	ts->tv_nsec = (long)nsec;
}


static
void gettimespec_tsc(struct mm_timespec* ts)
{
	unsigned int tsc_aux;
	int64_t tsc;

	tsc = __rdtscp(&tsc_aux);
	convert_cpu_cycle_to_ts(tsc, ts);
}


static
void getres_tsc(struct mm_timespec* res)
{
	long nsec;

	nsec = (long)(cpu_cycle_tick_psec/1000);
	if (nsec == 0)
		nsec = 1;

	res->tv_sec = 0;
	res->tv_nsec = nsec;
}


/**************************************************************************
 *                            QPC based clock                             *
 **************************************************************************/
static LONGLONG qpc_tick_freq;
static int nsec_in_qpc_tick;

static
void init_qpc(void)
{
	LARGE_INTEGER qpc_freq;

	QueryPerformanceFrequency(&qpc_freq);

	qpc_tick_freq = qpc_freq.QuadPart;
	nsec_in_qpc_tick = (int)((1.0 / qpc_freq.QuadPart)*1.0e9);
}


static
void gettimespec_qpc(struct mm_timespec* ts)
{
	LARGE_INTEGER count;
	LONGLONG sec, nsec;

	QueryPerformanceCounter(&count);

	sec = count.QuadPart / qpc_tick_freq;
	nsec = (count.QuadPart - sec*qpc_tick_freq)*nsec_in_qpc_tick;
	ts->tv_sec = (time_t)sec;
	ts->tv_nsec = (long)nsec;
}


static
void getres_qpc(struct mm_timespec* res)
{
	res->tv_sec = 0;
	res->tv_nsec = nsec_in_qpc_tick;
}


/**************************************************************************
 *                          monotonic clock                               *
 **************************************************************************/

#define EAX_REG_INDEX	0
#define EBX_REG_INDEX	1
#define ECX_REG_INDEX	2
#define EDX_REG_INDEX	3
#define NUM_REG         4

// To get the value that must be passed to cpuid, see the ISA documentation from Intel
#define CPUID_LEAF_EXTENDED     0x80000001
#define RDTSCP_EDX_MASK         (1<<27)
#define CPUID_LEAF_TSC          0x80000007
#define INVARIANT_TSC_EDX_MASK  (1<<8)

static bool monotonic_use_tsc;

static
void init_monotonic_clock(void)
{
	int cpu_regs[NUM_REG];
	bool rdtscp_supported = false;
	bool is_tsc_invariant = false;

	// Check support for rdtscp instruction
	__cpuid(cpu_regs, CPUID_LEAF_EXTENDED);
	if (cpu_regs[EDX_REG_INDEX] & RDTSCP_EDX_MASK)
		rdtscp_supported = true;

	// Check that processor provides invariant TSC
	__cpuid(cpu_regs, CPUID_LEAF_TSC);
	if (cpu_regs[EDX_REG_INDEX] & INVARIANT_TSC_EDX_MASK)
		is_tsc_invariant = true;

	// Most modern processors support invariant TSC and rdtscp instruction
	if (is_tsc_invariant && rdtscp_supported)
		monotonic_use_tsc = true;
	else
		monotonic_use_tsc = false;
}


LOCAL_SYMBOL
void gettimespec_monotonic_w32(struct mm_timespec* ts)
{
	if (monotonic_use_tsc)
		gettimespec_tsc(ts);
	else
		gettimespec_qpc(ts);
}


LOCAL_SYMBOL
void getres_monotonic_w32(struct mm_timespec* res)
{
	if (monotonic_use_tsc)
		getres_tsc(res);
	else
		getres_qpc(res);
}


/**************************************************************************
 *                             other clocks                               *
 **************************************************************************/

LOCAL_SYMBOL
void gettimespec_wallclock_w32(struct mm_timespec* ts)
{
	FILETIME curr;

	GetSystemTimePreciseAsFileTime(&curr);
	filetime_to_timespec(curr, ts);
}


LOCAL_SYMBOL
void getres_wallclock_w32(struct mm_timespec* res)
{
	// GetSystemTimePreciseAsFileTime() express time in term of number of
	// 100-nanosecond intervals
	res->tv_sec = 0;
	res->tv_nsec = 100;
}


LOCAL_SYMBOL
void gettimespec_thread_w32(struct mm_timespec* ts)
{
	ULONG64 cycles;

	QueryThreadCycleTime(GetCurrentThread(), &cycles);
	convert_cpu_cycle_to_ts((int64_t)cycles, ts);
}


LOCAL_SYMBOL
void getres_thread_w32(struct mm_timespec* res)
{
	getres_tsc(res);
}


LOCAL_SYMBOL
void gettimespec_process_w32(struct mm_timespec* ts)
{
	ULONG64 cycles;

	QueryProcessCycleTime(GetCurrentProcess(), &cycles);
	convert_cpu_cycle_to_ts((int64_t)cycles, ts);
}


LOCAL_SYMBOL
void getres_process_w32(struct mm_timespec* res)
{
	getres_tsc(res);
}


/**************************************************************************
 *                        Clocks initialization                           *
 **************************************************************************/

MM_CONSTRUCTOR(init_clocks_win32)
{
	determine_cpu_cycle_rate();
	init_qpc();
	init_monotonic_clock();
}
