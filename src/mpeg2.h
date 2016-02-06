#ifndef __MPEG2_H
#define __MPEG2_H

#include <sys/types.h>
#include <mqueue.h>

namespace mpeg2
{
    typedef struct tagMPEG2_AF
    {
        uint8_t adapt_len;
        uint8_t adapt_flags;
        uint8_t pcr[6];
        uint8_t data[176];
    } mpeg2_af;

    typedef struct tagMPEG2_PES
    {
        uint8_t start_code[3];
        uint8_t stream_id;
        uint16_t pes_packet_length;
        uint8_t extension[3];
        uint8_t pts[5];
        uint8_t dts[5];
    } mpeg2_pes;

    // 188-byte MPEG-2 TS packet
    typedef struct tagMPEG2_TS
    {
        uint8_t ts_header[4];
        union {
            mpeg2_af af;
            mpeg2_pes pes;
        };
    } mpeg2_ts;

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

    /*int ts_get_pid(mpeg2_ts *ts);
    uint64_t ts_get_pts(mpeg2_pes *pes);
    void ts_set_pts(mpeg2_pes *pes, uint64_t pts);
    int ts_has_pcr(mpeg2_ts *ts, int pid);
    mpeg2_pes* ts_get_pes_header(mpeg2_ts *ts);
    int ts_has_pts(mpeg2_ts *ts);*/

    class mpeg2_packet
    {
    public:
        mpeg2_packet(mpeg2_ts* ts) : _ts(ts) {};
        ~mpeg2_packet() {};
        
        void adjust_pts();
        bool suppress_pid(mpeg2_ts* ts, int suppress_pid, int suppress_count);
    private:
        bool has_pcr(int pid);
        bool has_pts();
        int get_pid();
        mpeg2_pes* get_pes_header();
        uint64_t get_pts(uint8_t pts_a[]);
        void set_pts(uint8_t pts_a[], uint64_t pts);

        mpeg2_ts* _ts;
    };

    class message_queue
    {
    public:
        message_queue(const char* name) : _mq(0), _name(name) {};
        ~message_queue() {};
        
        void create(int maxmsg);
        void open();
        void close();
        void unlink();
        int send(const char *msg_ptr, size_t msg_len);
        ssize_t receive(char *msg_ptr, size_t msg_len);
    private:
        mqd_t _mq;
        const char* _name;

        int get_curmsgs();
    };
}

#endif /* __MPEG2_H */