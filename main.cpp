#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "openglWp.h"

#define EXIT(...) do { printf(__VA_ARGS__); exit(1); } while(true)

#include "navi.h"

int process_bath(const bath_data_packet_t* bath)
{
    using point_t = Eigen::Vector3f;
    std::vector<point_t> vertices(bath->sub_header.N);
    std::vector<point_t> colors(bath->sub_header.N);
    std::vector<float> range(bath->sub_header.N, 0.f);

    assert(bath->header.preamble == 0xdeadbeef);
    printf("ping_number: %d, freq: %f, tx_angle: %f, swath_open: %f, time: %f\n", bath->sub_header.ping_number, bath->sub_header.tx_freq, bath->sub_header.tx_angle, bath->sub_header.swath_open, bath->sub_header.time);
    assert(bath->sub_header.N == 512);
    assert(bath->sub_header.sample_rate == 78125.f);

    for (int index = 0; index < bath->sub_header.N; ++index)
    {
        //printf("sample_number: %u %f %u %u %f %u %u %u\n", bath->dp[index].sample_number, bath->dp[index].angle, bath->dp[index].upper_gate, bath->dp[index].lower_gate, bath->dp[index].intensity, bath->dp[index].flags, bath->dp[index].quality_flags, bath->dp[index].quality_val);
        //for (char* i = (char*)(sample + index); i != (char*)(sample + index + 1); ++i)
        //    printf("%02x", *i);
        //printf("\n");
        //printf("%x\n", bath->dp[index].sample_number >> 24);
        range[index] = (bath->dp[index].sample_number * bath->sub_header.snd_velocity) / (2 * bath->sub_header.sample_rate);
        //printf("range %d:\t%f\n", index, range[index]);

        point_t vertex;
        //float x = ((float)index / (float)bath->sub_header.N) * 2 - 1;
        //float y = (range[index] / 10.f) * 2 - 1;

        const float scale = .04;
        float x = sin(bath->dp[index].angle) * range[index] * scale;
        float y = -cos(bath->dp[index].angle) * range[index] * scale;
        //printf("x: %f\ty: %f\n", x, y);

        //float x = cos(x + (float)t / 500.f);
        //float y = sin(x + (float)t / 500.f);
        vertex << x, y, -1;
        vertices[index] = vertex;

        point_t color;
        color << 1, 1, 1;
        colors[index] = color;
    }

    Eigen::Matrix3f projection;
    projection.setIdentity();

    updateWp(vertices, colors, projection);

    SDL_Event e;
    while ( SDL_PollEvent(&e) ) {
      switch (e.type) {
        case SDL_QUIT:
          return 1;
        break;
        case SDL_MOUSEBUTTONDOWN:
        {
          //int x, y;
          //SDL_GetMouseState(&x, &y);
          //down = true;
          //down_at.x = 2*(float)x/width - 1;
          //down_at.y = 1 - 2*(float)y/height;
          //down_at_phi = phi;
          //down_at_theta = theta;
          //down_at_xi = xi;
        }
        break;
        case SDL_MOUSEBUTTONUP:
        {
          //int x, y;
          //SDL_GetMouseState(&x, &y);
          //down = false;
        }
        break;
        case SDL_KEYDOWN:
        {
          switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
              return 1;
            break;
          }
        break;
        }
      }
    }

    return 0;
}

void print_usage(char** argv)
{
    EXIT("Usage: %s -f <file> -t <tcp_ip>:<port> -m <raw|sbd>\n", argv[0]);
}

int main(int argc, char** argv)
{
    const char* filename = NULL;
    char ip[100] = {0};
    int port = 0;
    enum { RAW, SBD } format = RAW;

    int opt;
    const char * const opt_string = "m:f:t:";
    while ((opt = getopt(argc, argv, opt_string)) != -1)
    {
        switch(opt)
        {
            case 'm': // format
                if (strcmp(optarg, "raw") == 0)
                    format = RAW;
                else if (strcmp(optarg, "sbd") == 0)
                    format = SBD;
                else
                {
                      printf("unrecognised format %s\n", optarg);
                      return 1;
                }
            break;
            case 'f': // filename
              filename = optarg;
              break;
            case 't': // tcp url (ip:port)
            {
                int read = sscanf(optarg, "%99[^:]:%99d", ip, &port);
                assert(read == 2);
                break;
            }
            case '?':
            default:
              print_usage(argv);
              return 1;
        }
    }

    if (!filename && (!ip[1] || !port))
    {
        printf("please specify either file or tcp url\n");
        print_usage(argv);
    }
    else if (filename && (ip[1] || port))
    {
        printf("please don't specify both file and tcp url\n");
        print_usage(argv);
    }

    char* buf = NULL;
    long buf_size = 0;
    if (filename)
    {
        FILE *f = fopen(filename, "rb");
        if (!f)
            EXIT("no such file: %s\n", filename);

        fseek(f, 0, SEEK_END);
        buf_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = (char*)malloc(buf_size);
        if (!buf)
            EXIT("couldn't allocate buf with size: %ld\n", buf_size);

        {
            size_t read = fread(buf, 1, buf_size, f);
            if (read != buf_size)
                EXIT("read %d instead of %d bytes from file\n", read, buf_size);
        }
        fclose(f);
    }

    int sock = -1;
    int tcp_bytes = 0;
    if (ip[1] && port)
    {
        buf_size = 2 * sizeof(bath_data_packet_t);
        buf = (char*)malloc(buf_size);
        printf("connecting to tcp port at %s:%d\n", ip, port);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        assert(sock != -1);

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip);

        int conn = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        assert(conn == 0);

        while(tcp_bytes < sizeof(bath_data_packet_t))
        {
            int n_read = read(sock, buf + tcp_bytes, sizeof(bath_data_packet_t) - tcp_bytes);
            assert(n_read != -1);
            tcp_bytes += n_read;
        }

        //printf("find deadbeef\n");

        //const bath_data_packet_t* bath;
        //for (const char* search = (const char*)buf; search < buf + buf_size;)
        //{
        //    if (*(uint32_t*)search == 0xdeadbeef)
        //    {
        //        bath = (const bath_data_packet_t*)search;
        //        break;
        //    }
        //    else
        //    {
        //        printf("searching at %d\n", search - buf);
        //        ++search;
        //    }
        //}
        //const int bath_offset = (const char*)bath - buf;
        //printf("found deadbeef at %d\n", bath_offset);

        //// move deadbeef to start of buffer
        //for (char* from = (char*)bath; from < buf + tcp_bytes; ++from)
        //    from[-bath_offset] = *from;
        //tcp_bytes -= bath_offset;

        //bath = (const bath_data_packet_t*)((const char*)bath - bath_offset);

        //// fill in the rest
        //while(tcp_bytes < sizeof(bath_data_packet_t))
        //{
        //    int n_read = read(sock, buf + tcp_bytes, sizeof(bath_data_packet_t) - tcp_bytes);
        //    assert(n_read != -1);
        //    tcp_bytes += n_read;
        //}

        const bath_data_packet_t* bath = (const bath_data_packet_t*)buf;
        assert(tcp_bytes == sizeof(*bath));
        assert(bath->header.preamble == 0xdeadbeef);
        tcp_bytes -= sizeof(*bath);
    }

    // plotting init
    int rc = initWp();
    assert(rc == 0);

    switch(format)
    {
        case RAW:
        {
            if (filename)
            {
                for (const char* bath = buf; bath < buf + buf_size; bath += ((const bath_data_packet_t*)bath)->size())
                {
                    if (process_bath((const bath_data_packet_t*)bath))
                        return 1;
                    usleep(50000);
                }
            }
            else if(sock != -1)
            {
                for (;;)
                {
                    assert(tcp_bytes == 0);
                    const bath_data_packet_t* bath = (const bath_data_packet_t*)buf;
                    while(tcp_bytes < sizeof(bath_data_packet_t))
                    {
                        int n_read = read(sock, buf + tcp_bytes, sizeof(*bath) - tcp_bytes);
                        assert(n_read != -1);
                        tcp_bytes += n_read;
                    }
                    assert(tcp_bytes == sizeof(*bath));
                    if (bath->header.preamble != 0xdeadbeef)
                    {
                        printf("no deadbeef\n");
                        bool found_deadbeef = false;
                        while (!found_deadbeef)
                        {
                            printf("still no deadbeef\n");
                            for (const char* search = (const char*)buf; search < buf + tcp_bytes;)
                            {
                                if (*(uint32_t*)search == 0xdeadbeef)
                                {
                                    bath = (const bath_data_packet_t*)search;
                                    found_deadbeef = true;
                                    break;
                                }
                                else
                                {
                                    printf("searching at %d\n", search - buf);
                                    ++search;
                                }
                            }

                            tcp_bytes = 0;
                            while(tcp_bytes < sizeof(bath_data_packet_t))
                            {
                                int n_read = read(sock, buf + tcp_bytes, sizeof(*bath) - tcp_bytes);
                                assert(n_read != -1);
                                tcp_bytes += n_read;
                            }
                        }
                        const int bath_offset = (const char*)bath - buf;
                        printf("found deadbeef at %d\n", bath_offset);

                        // move deadbeef to start of buffer
                        for (char* from = (char*)bath; from < buf + tcp_bytes; ++from)
                            from[-bath_offset] = *from;
                        tcp_bytes -= bath_offset;

                        bath = (const bath_data_packet_t*)((const char*)bath - bath_offset);

                        // fill in the rest
                        while(tcp_bytes < sizeof(bath_data_packet_t))
                        {
                            int n_read = read(sock, buf + tcp_bytes, sizeof(bath_data_packet_t) - tcp_bytes);
                            assert(n_read != -1);
                            tcp_bytes += n_read;
                        }
                    }
                    printf("bath size: %d\n", bath->size());
                    assert(bath->size() == sizeof(*bath));
                    if (process_bath(bath))
                        return 1;
                    tcp_bytes -= sizeof(*bath);
                }
            }
            break;
        }
        case SBD:
        {
            const char* p = buf;
            for (int t=0;;++t)
            {
                SbdEntryHeader* header = (SbdEntryHeader*)p;

                printf("header: type: 0x%02x relative_time: %d, absolute_time: %d %d, size: %d\n", header->entry_type, header->relative_time, header->absolute_time.tv_sec, header->absolute_time.tv_usec, header->entry_size);

                const char* data = p + sizeof(*header);

                switch (header->entry_type)
                {
                    case SbdEntryHeader::WBMS_BATH:
                    {
                        assert(header->entry_size == 10352);

                        bath_data_packet_t* bath = (bath_data_packet_t*)data;
                        assert(bath->size() == header->entry_size);
                        if (process_bath(bath))
                            return 1;
                        usleep(50000);

                        break;
                    }
                    // the size of the nmea string (with null terminator) is the integer before the string:
                    // uint32_t size = ((uint32_t*)nmea) - 1;
                    case SbdEntryHeader::NMEA_EIHEA:
                    {
                        // EIHEA
                        const char* nmea = (const char*)data + 20;
                        printf("nmea: %s\n", nmea);

                        uint32_t len;
                        uint64_t timestamp;
                        double timeUTC;
                        double heading;
                        int sscanf_ret = sscanf(nmea,"$EIHEA,%u,%lf,%lu,%lf*",&len,&timeUTC,&timestamp,&heading);

                        if (sscanf_ret < 4)
                            printf("warning: HEA\n");

                        break;
                    }
                    case SbdEntryHeader::NMEA_EIPOS:
                    {
                        // EIPOS
                        const char* nmea = (const char*)data + 20;
                        printf("nmea: %s", nmea);

                        uint32_t len;
                        uint64_t timestamp;
                        double timeUTC;
                        double longitude;
                        double latitude;
                        char north;
                        char east;
                        int sscanf_ret = sscanf(nmea,"$EIPOS,%u,%lf,%lu,%lf,%c,%lf,%c*",&len,&timeUTC,&timestamp,&latitude,&north, &longitude,&east);

                        if (sscanf_ret < 7)
                            printf("warning: POS\n");

                        break;
                    }
                    case SbdEntryHeader::NMEA_EIORI:
                    {
                        // EIORI
                        const char* nmea = (const char*)data + 16;
                        printf("nmea: %s\n", nmea);

                        uint32_t len;
                        uint64_t timestamp;
                        double timeUTC;
                        double roll;
                        double pitch;
                        int sscanf_ret = sscanf(nmea,"$EIORI,%u,%lf,%lu,%lf,%lf*",&len,&timeUTC,&timestamp,&roll,&pitch);

                        if (sscanf_ret < 5)
                            printf("warning: ORI\n");
                        break;
                    }
                    case SbdEntryHeader::NMEA_EIDEP:
                    {
                        // EIDEP
                        const char* nmea = (const char*)data + 16;
                        printf("nmea: %s\n", nmea);

                        uint32_t len;
                        uint64_t timestamp;
                        double timeUTC;
                        double depth;
                        double altitude;
                        int sscanf_ret = sscanf(nmea,"$EIDEP,%u,%lf,%lu,%lf,m,%lf,m*",&len,&timeUTC,&timestamp,&depth,&altitude);
                        if (sscanf_ret < 5)
                            printf("warning: ORI\n");

                        break;
                    }
                    case SbdEntryHeader::HEADER:
                        printf("header\n");
                        break;
                    default:
                        assert(false);
                }

                p += sizeof(*header) + header->entry_size;
                if (*p == '\0')
                {
                    printf("done\n");
                    break;
                }
            }
            break;
        }
        default:
            assert(0);
    }


    free(buf);

    return 0;
}
