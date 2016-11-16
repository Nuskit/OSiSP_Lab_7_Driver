#include "stdafx.h"
#include "ThreadMutex.h"

ThreadMutex::ThreadMutex()
{
  InitializeCriticalSectionAndSpinCount(&m_mutex,4000);
}

ThreadMutex::~ThreadMutex()
{
  DeleteCriticalSection(&m_mutex);
}

void ThreadMutex::Lock()
{
  EnterCriticalSection(&m_mutex);
}

bool ThreadMutex::TryLock()
{
  return TryEnterCriticalSection(&m_mutex)>0?true:false;
}

void ThreadMutex::Unlock()
{
  LeaveCriticalSection(&m_mutex);
}
