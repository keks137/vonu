#ifdef WIN32

static void time_init()
{
	QueryPerformanceFrequency((LARGE_INTEGER *)&g_freq);
}

f64 time_now()
{
	u64 now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	return (f64)now / (f64)g_freq;
}
void precise_sleep(f64 seconds)
{
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	LARGE_INTEGER due;
	due.QuadPart = -(LONGLONG)(seconds * 1e7); // 100ns units, negative = relative
	SetWaitableTimer(timer, &due, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#else

#include <time.h>

f64 time_now()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void precise_sleep(f64 seconds)
{
	struct timespec ts = {
		.tv_sec = (time_t)seconds,
		.tv_nsec = (long)((seconds - (time_t)seconds) * 1e9)
	};
	nanosleep(&ts, NULL);
}

#endif //WIN32
