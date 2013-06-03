/***************************************************************************
 *   Copyright (C) 2013 by Terraneo Federico                               *
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

#include "file.h"
#include <stdio.h>
#include "file_access.h"

namespace miosix {

//
// class File
//

FileBase::~FileBase()
{
    if(parent) parent->fileCloseHook();
}

//
// class NullFile
//

ssize_t NullFile::write(const void *data, size_t len)
{
    return len;
}

ssize_t NullFile::read(void *data, size_t len)
{
    return -EBADF;
}

off_t NullFile::lseek(off_t pos, int whence)
{
    switch(whence)
    {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            return -EBADF;
        default:
            return -EINVAL;
    }
}

int NullFile::fstat(struct stat *pstat) const
{
    return -EBADF; //TODO
}

int NullFile::isatty() const
{
    return 0;
}

int NullFile::sync()
{
    return 0;
}

//
// class ZeroFile
//

ssize_t ZeroFile::write(const void *data, size_t len)
{
    return -EBADF;
}

ssize_t ZeroFile::read(void *data, size_t len)
{
    memset(data,0,len);
    return len;
}

off_t ZeroFile::lseek(off_t pos, int whence)
{
    switch(whence)
    {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            return -EBADF;
        default:
            return -EINVAL;
    }
}

int ZeroFile::fstat(struct stat *pstat) const
{
    return -EBADF; //TODO
}

int ZeroFile::isatty() const
{
    return 0;
}

int ZeroFile::sync()
{
    return 0;
}

//
// class TerminalDevice
//

//TODO FileBase(0): should we override getPartent() to return device->getParent()?
TerminalDevice::TerminalDevice(intrusive_ref_ptr<FileBase> device)
        : FileBase(0), device(device), echo(false), binary(false), skipNewline(false) {}

void TerminalDevice::changeDevice(intrusive_ref_ptr<FileBase> newDevice)
{
    device=newDevice;
}

ssize_t TerminalDevice::write(const void *data, size_t len)
{
    if(binary) return device->write(data,len);
    const char *dataStart=static_cast<const char*>(data);
    const char *buffer=dataStart;
    const char *chunkstart=dataStart;
    //Try to write data in chunks, stop at every \n to replace with \r\n
    for(size_t i=0;i<len;i++,buffer++)
    {
        if(*buffer=='\n')
        {
            size_t chunklen=buffer-chunkstart;
            if(chunklen>0)
            {
                ssize_t r=device->write(chunkstart,chunklen);
                if(r<=0) return chunkstart==dataStart ? r : chunkstart-dataStart;
            }
            ssize_t r=device->write("\r\n",2);//Add \r\n
            if(r<=0) return chunkstart==dataStart ? r : chunkstart-dataStart;
            chunkstart=buffer+1;
        }
    }
    if(chunkstart!=buffer)
    {
        ssize_t r=device->write(chunkstart,buffer-chunkstart);
        if(r<=0) return chunkstart==dataStart ? r : chunkstart-dataStart;
    }
    return len;
}

ssize_t TerminalDevice::read(void *data, size_t len)
{
    if(binary)
    {
        ssize_t result=device->read(data,len);
        if(echo && result>0) device->write(data,result); //Ignore write errors
        return result;
    }
    char *dataStart=static_cast<char*>(data);
    char *buffer=dataStart;
    //Trying to be compatible with terminals that output \r, \n or \r\n
    //When receiving \r skipNewline is set to true so we skip the \n
    //if it comes right after the \r
    for(size_t i=0;i<len;i++,buffer++)
    {
        ssize_t r=device->read(buffer,1);
        if(r<=0) return buffer==dataStart ? r : buffer-dataStart;
        switch(*buffer)
        {
            case '\r':
                *buffer='\n';
                if(echo) device->write("\r\n",2);
                skipNewline=true;
                return i+1;    
            case '\n':
                if(skipNewline)
                {
                    skipNewline=false;
                    //Discard the \n because comes right after \r
                    //Note that i may become -1, but it is acceptable.
                    i--;
                    buffer--;
                } else {
                    if(echo) device->write("\r\n",2);
                    return i+1;
                }
                break;
            default:
                skipNewline=false;
                if(echo) device->write(buffer,1);
        }
    }
    return len;
}

off_t TerminalDevice::lseek(off_t pos, int whence)
{
    return device->lseek(pos,whence);
}

int TerminalDevice::fstat(struct stat *pstat) const
{
    return device->fstat(pstat);
}

int TerminalDevice::isatty() const { return device->isatty(); }

int TerminalDevice::sync() { return device->sync(); }

} //namespace miosix
