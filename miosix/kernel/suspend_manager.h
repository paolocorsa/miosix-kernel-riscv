/***************************************************************************
 *   Copyright (C) 2010, 2011, 2012 by Terraneo Federico  and Luigi Rucco  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef SUSPENDMANAGER_H
#define	SUSPENDMANAGER_H
#include "interfaces/portability.h"
#include "interfaces/suspend_support.h"
#include <list>
#include <miosix.h>

#ifdef WITH_PROCESSES
namespace miosix{

    
///This struct is used to serialize the interrupion point status for processes
///and threads spawned by that process
struct IntPointStatus{
    int intPointID; //ID of the syscall which caused the interruption
    int file_id; //file eventually opened by the syscall, -1 if no file opened
    long long absSyscallTime; //absolute time taken by the syscall to resume
    unsigned int* backupQueue; //ptr to the process queue in the backupe SRAM
    int queueSize; //size of the queue associated to the process
    char wakeNow;//set to one if the process or thread has to wake up now
    char sizeofSample; //size of the data sampled in the hibernation period
    int sampNum; //number of the samples eventually performed by smart driver
    unsigned int registers[CTXSAVE_SIZE];
    unsigned int *targetSampleMem; //process memory pointer to copy the queue
}__attribute__((packed));
    
    
///This struct is used to serialize the process status in the backup SRAM 
struct ProcessStatus 
{
    int pid;
    int ppid;
    short int status;
    short int numThreads;
    int exitCode;
    unsigned int* processImageBase;
    int processImageSize;
    unsigned int* programBase;
    int programSize;
    int fileDescriptors[MAX_OPEN_FILES];
    struct IntPointStatus interruptionPoints[1+MAX_THREADS_PER_PROCESS];
} __attribute__((packed));

//this struct is used to keep track of the resume time after a syscall for each
//process and for each thread in a process
struct syscallResumeTime
{
    int pid;
    short int threadNum;
    long long resumeTime;
    ProcessStatus* status;
}__attribute__((packed));


class SuspendManager 
{
public:
    SuspendManager();
    ~SuspendManager();
    
    /**
     * /return the base address of the process status backup area 
     */
    ProcessStatus* getProcessesBackupAreaBase()
    {
        return    reinterpret_cast<struct ProcessStatus*>(
                                reinterpret_cast<unsigned int>(backupSramBase)+
                                getAllocatorSramAreaSize()+ 
                                getBackupAllocatorSramAreaSize());
    }
    
    /**
     * /return the ptr to allocate the next process status in the backup area 
     */
    ProcessStatus* getProcessesBackupAreaPtr();
    
    
   
    /**
     * Find the the serialized processes with invalid bit set,
     * which must be serialzied again. Return -1 if no dirty processes status
     * are found
     */
    int resume();

private:
    static void wakeupDaemon(void*);
    static void hibernateDaemon(void*);
    /**
     * @param pid pid of the process in which the syscall has benn called
     * @param threadID id of the thread which called the system call
     * @param resumeTime is the time at which the thread will be resumed after
     * the suspension due to the system call
     */
    static void enterInterruptionPoint(pid_t pid, int threadID, long long resumeTime);
    
    int numSerializedProcesses;
    static std::list<syscallResumeTime> syscallReturnTime;
    ///Uset to guard access to the number of suspended processes
    static Mutex suspMutex;
    ///Used to wait on the condition that all process must be suspended to
    ///decide if to hibernate or not the system. The suspension of all processes
    ///is a necessary condition for hibernationm, but not sufficient. The final
    ///decision is taken according to the hibernation policy.
    static ConditionVariable hibernWaiting;
    ///this map lists the suspended processes
    static std::list<Process *> suspendedProcesses;
    
    
    //Needs access to process table, serialization/loading methods
    friend class Process;
    
};

}//namespace miosix
#endif //WITH_PROCESSES
#endif	/* SUSPENDMANAGER_H */

