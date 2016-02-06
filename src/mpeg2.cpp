#include "mpeg2.h"
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

using namespace mpeg2;

int mpeg2_packet::get_pid()
{
    return ( ( _ts->ts_header[ 1 ] << 8 ) + _ts->ts_header[ 2 ] ) & 0x1fff;
}

/*static uint64_t ts_get_pcr(mpeg2_ts *ts )
{
    uint64_t pcr;
    pcr = ts->pcr[ 0 ] << 25;
    pcr += ts->pcr[ 1 ] << 17;
    pcr += ts->pcr[ 2 ] << 9;
    pcr += ts->pcr[ 3 ] << 1;
    pcr += ( ts->pcr[ 4 ] & 0x80 ) >> 7;
    pcr *= 300;
    pcr += ( ts->pcr[ 4 ] & 0x1 ) << 8;
    pcr += ts->pcr[ 5 ];
    return pcr;
}
*/

uint64_t mpeg2_packet::get_pts(uint8_t pts_a[])
{
    printf("[lua_http_change_pcr] PTS get: %hhx %hhx %hhx %hhx %hhx\n",
      pts_a[0], pts_a[1], pts_a[2], pts_a[3], pts_a[4]);

    uint64_t raw;
    raw = ((uint64_t)pts_a[0] << 32) // this is a 64bit integer, lowest 36 bits contain a timestamp with markers
        | ((uint64_t)pts_a[1] << 24)
        | ((uint64_t)pts_a[2] << 16)
        | ((uint64_t)pts_a[3] << 8)
        | ((uint64_t)pts_a[4] << 0);

    uint64_t pts = 0;
    pts |= (raw >> 3) & (0x0007ULL << 30); // top 3 bits, shifted left by 3, other bits zeroed out
    pts |= (raw >> 2) & (0x7fffULL << 15); // middle 15 bits
    pts |= (raw >> 1) & (0x7fffULL <<  0); // bottom 15 bits

    return pts;
}

void mpeg2_packet::set_pts(uint8_t pts_a[], uint64_t pts)
{
    pts_a[0] &= 0xF1;
    pts_a[0] |= (pts >> 29) & 0xe;

    pts_a[1] = (pts >> 22) & 0xff;

    pts_a[2] &= 0x01;
    pts_a[2] |= (pts >> 14) & 0xfe;

    pts_a[3] = (pts >> 7) & 0xff;

    pts_a[4] &= 0x01;
    pts_a[4] |= (pts << 1) & 0xfe;

    printf("[lua_http_change_pcr] PTS set: %hhx %hhx %hhx %hhx %hhx\n",
      pts_a[0], pts_a[1], pts_a[2], pts_a[3], pts_a[4]);
}

bool mpeg2_packet::has_pcr(int pid)
{
    if ( ( pid == -1 ) || (pid == get_pid()))
    {
        if ( ( _ts->ts_header[ 3 ] & 0x20 )  // adaptation field present
            && ( _ts->af.adapt_len > 0 )
            && ( _ts->af.adapt_flags & 0x10 ) )
        {
            return true;
        }
    }

    return false;
}

mpeg2_pes* mpeg2_packet::get_pes_header()
{
    if (_ts->ts_header[3] & 0x20)    // adaptation field present
    {
        return (mpeg2_pes*)((char*)_ts + sizeof(_ts->ts_header) + 1 + _ts->af.adapt_len);
    }

    return &_ts->pes;
}

bool mpeg2_packet::has_pts()
{
    if ( (_ts->ts_header[1] & 0x40))   // payload unit start indicator
    {
        mpeg2_pes *pes = get_pes_header();
        return pes->extension[1] & 0x80; // PTS data present
    }

    return false;
}

void mpeg2_packet::adjust_pts()
{
    /*if (has_pcr(-1))
    {
        uint64_t original_pcr = ts_get_pcr(ts);
        ts->pcr[1] += 20;

        printf("[lua_http_change_pcr] adaptation field found in: 0x%08x, pcr: %llu\n", *(int*)buf, original_pcr);
        printf("[lua_http_change_pcr] adaptation field found in: %#1hhx %#1hhx %#1hhx %#1hhx %#1hhx %#1hhx\n",
         ts->pcr[0], ts->pcr[1], ts->pcr[2], ts->pcr[3], ts->pcr[4], ts->pcr[5]);
    }*/

    if (has_pts())
    {
        mpeg2_pes* pes = get_pes_header();

        uint64_t pts = get_pts(pes->pts);

        pts += 3 * 60 * 60 * 90000;

        if (pts > 0x1ffffffffLL)
        {
            pts = 0x1ffffffffLL;
        }

        set_pts(pes->pts, pts);

        int pid = get_pid();

        if (pes->extension[1] & 0x40)
        {
            uint64_t dts = get_pts(pes->dts);
            
            dts += 3 * 60 * 60 * 90000;
            
            if (dts > 0x1ffffffffLL)
            {
                dts = 0x1ffffffffLL;
            }

            set_pts(pes->dts, dts);

            //printf("[lua_http_change_pcr] PID: %d, PTS: %llu, DTS: %llu\n", pid, pts, dts);
        }
        else
        {
            //printf("[lua_http_change_pcr] PID: %d, PTS: %llu\n", pid, pts);
        }
    }
}


bool mpeg2_packet::suppress_pid(mpeg2_ts* ts, int suppress_pid, int suppress_count)
{
    int pid = get_pid();

    if (pid == suppress_pid)
    {
        printf("[mpeg2_packet::suppress_pid] packet suppressed for PID: %d\n", pid);
        
        ts->ts_header[1] |= 0x1f;
        ts->ts_header[2] |= 0xff;

        return true;
    }

    return false;
}

void message_queue::create(int maxmsg)
{
    printf("[message_queue::create] %s, size: %i\n", _name, maxmsg);

    mq_unlink(_name);

    /* initialize the queue attributes */
    mq_attr attr;
    attr.mq_flags = 0;        // blocking read/write
    attr.mq_maxmsg = maxmsg;   // maximum number of messages allowed in queue
    attr.mq_msgsize = 1316;   // messages are contents of mpeg2 packets
    attr.mq_curmsgs = 0;      // number of messages currently in queue

    /* create the message queue */
    _mq = mq_open(_name, O_CREAT | O_EXCL | O_WRONLY, 0644, &attr);
    
    if (_mq == -1)
    {
        perror("[message_queue::create] Failed to create message queue");
        return;
    }    
}

void message_queue::open()
{
    printf("[message_queue::open] %s\n", _name);

    /* open the message queue */
    _mq = mq_open(_name, O_RDONLY);
    
    if (_mq == -1)
    {
        perror("[message_queue::open] Failed to create message queue");
        return;
    }

    int curmsgs = get_curmsgs();
    printf("[message_queue::open] %s, messages currently in queue: %i\n", _name, curmsgs);
}

void message_queue::close()
{
    printf("[message_queue::close] %s\n", _name);

    int status = mq_close(_mq);
    if (status == -1)
    {
        perror("[message_queue::close] Failed closing message queue");
    }

    _mq = 0;
}

void message_queue::unlink()
{
    printf("[message_queue::unlink] %s\n", _name);

    int status = mq_unlink(_name);
    if (status == -1)
    {
        perror("[message_queue::unlink] Failed deleting message queue");
    }
}

int message_queue::send(const char *msg_ptr, size_t msg_len)
{
    timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);

    tm.tv_sec += 1;  // Set for 1 seconds

    //int status = mq_send(_mq, msg_ptr, msg_len, 0);
    int status = mq_timedsend(_mq, msg_ptr, msg_len, 0, &tm);
    if (status == -1)
    {
        perror("[message_queue::send] Failed to send message");
    }

    int curmsgs = get_curmsgs();   
    if (curmsgs % 1000 == 0 && curmsgs != 0)
    {
        printf("[message_queue::send] %s, messages currently in queue: %i\n", _name, curmsgs);
    }

    return status;    
}

ssize_t message_queue::receive(char *msg_ptr, size_t msg_len)
{
    timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);

    tm.tv_sec += 1;  // Set for 1 seconds
    
    //ssize_t n = mq_receive(_mq, msg_ptr, msg_len, NULL);
    ssize_t n = mq_timedreceive(_mq, msg_ptr, msg_len, NULL, &tm ); 
    if (n == -1)
    {
        perror("[message_queue::receive] Failed to receive message");
    }

    return n;    
}

int message_queue::get_curmsgs()
{
    mq_attr attr;
    int status = mq_getattr(_mq, &attr);
    
    if (status == -1)
    {
        perror("[message_queue::get_curmsgs] Failed getting attributes");
        return -1;
    }

    return attr.mq_curmsgs;
}

