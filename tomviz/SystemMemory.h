#ifdef _WIN32

#include <windows.h>

unsigned long long getAvailableSystemMemory()
{
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  return status.ullAvailPhys;
}

#else

#include <unistd.h>

unsigned long long getAvailableSystemMemory()
{
  long pages = sysconf(_SC_AVPHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  return pages * page_size;
}
#endif
