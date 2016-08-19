#include "LogFile.h"

#include <unistd.h>

LogFile::LogFile(const std::string & basename,
                 size_t rollSize,
                 int flushInterval):
    _basename(basename),
    _rollSize(rollSize),
    _flushInterval(flushInterval),
    _lastFlush(0)
{
    rollFile();
}

void LogFile::append(const char * logline, int len)
{
    time_t now = ::time(NULL);
    struct tm * pTm = localtime(&now);
    int nowDay = pTm->tm_mday;

    if(len > 0)
    {
        _file->fwrite(logline, len);
    }

    if(_file->getWrittenBytes() > _rollSize || _fileDay != nowDay)
    {
        rollFile();
    }
    else
    {
        if(now - _lastFlush > _flushInterval)
        {
            _lastFlush = now;
            _file->fflush();
        }
    }
}

bool LogFile::rollFile()
{
    time_t now;
    std::string filename = getLogFileName(_basename, now);

    _file.reset(new File(filename));

    struct tm * pTm = localtime(&now);
    _fileDay = pTm->tm_mday;

    return false;
}

std::string LogFile::getLogFileName(const std::string & basename, time_t & now)
{
    std::string filename;
    filename.reserve(basename.size() + 128);

    char szPath[1024];
    filename = getpwd(szPath, 1023);
    filename += "log/";
    mkdir(filename.c_str(), 0744);

    filename += basename;

    char timebuf[32];
    struct tm tm;
    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S", &tm);
    filename += timebuf;

    filename += ".log";

    return filename;
}

char * LogFile::getpwd( char * buf, int len)
{
    int i;
    int rslt = readlink("/proc/self/exe", buf, len - 1);
    if (rslt < 0 || (rslt >= len - 1))
    {
        return NULL;
    }
    buf[rslt] = '\0';
    for (i = rslt; i >= 0; i--)
    {
        if (buf[i] == '/')
        {
            buf[i + 1] = '\0';
            break;
        }
    }
    return buf;
}
