#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "openglWp.h"
#include "navi.h"

#define EXIT(...) do { printf(__VA_ARGS__); exit(1); } while(true)

void fill_from_sbd((char*)& buf, char* filename)
{
    static bool init = false;
    if (init)
    {
        init = false;
        buf = malloc(sizeof(bath_data_packet_t));
        FILE *f = fopen(filename, "rb");
        if (!f)
            EXIT("no such file: %s\n", filename);

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = (char*)malloc(size);
        if (!buf)
            EXIT("couldn't allocate buf with size: %ld\n", size);

        {
            size_t read = fread(buf, 1, size, f);
            if (read != size)
                EXIT("read %d instead of %d bytes from file\n", read, size);
        }
        fclose(f);
    }
}

void fill_from_tcp((char*)& buf)
{
    static bool init = true;
    static int sock;
    static int size = sizeof(bath_data_packet_t);
    if (init)
    {
        init = false;
        buf = (char*)malloc(2 * sizeof(bath_data_packet_t));
        tcp_client.init(ip, port);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        assert(sock != INVALID_SOCKET);
    }

    // move remainder from last fill
    for (char* c = buf + sizeof(bath_data_packet_t); c < buf + size; ++c)
        *c = c[sizeof(bath_data_packet_t)]
    size -= sizeof(bath_data_packet_t);
    if (size > sizeof(int64_t))
        assert(*(int64_t*)buf == 0xdeadbeef);

    bool found_deadbeef = false;
    while (size < sizeof(bath_data_packet_t))
    {
        printf("size: %d of %d\n", size, sizeof(bath_data_packet_t));
        int n_read = read(sock, buf + size, sizeof(bath_data_packet_t));
        assert(n_read != -1);
        size += n_read;

        for (char* c = buf + size - n_read; !found_deadbeef && (c != buf + size - sizeof(int64_t)); ++c)
        {
            //printf("%x ", *c);
            if (*(int64_t*)c == 0xdeadbeef)
            {
                int deadbeef_index = c - buf;
                printf("deadbeef at %d\n", deadbeef_index);
                size -= deadbeef_index;
                for (int i=0; c != buf + deadbeef_index + size; ++c)
                    buf[i++] = *c;
                    
                found_deadbeef = true;
            }
        }
    }
}

int main(int argc, const char** argv)
{
    const char* filename = NULL;
    const char* ip = NULL;
    int port = 0;
    enum { RAW, SBD } format = RAW;
    if (argc > 1)
    {
        if (strcmp(argv[1], "-h") == 0)
            EXIT("Options:\n\t-h : help\n\t-f <file> : plot data from file\n\t-u <ip> <port> : plot data from ip\n\t-s : plot sbd format\n\t-a : plot raw sonar data\n");
        if (strcmp(argv[1], "-f") == 0)
            filename = argv[2];
        else if (strcmp(argv[1], "-u") == 0)
        {
            if (argc > 3)
            {
                ip = argv[2];
                port = atoi(argv[3]);
            }
            else
                EXIT("usage: %s -u <ip> <port>\n", argv[0]);
        }
    }
    else
        EXIT("usage: %s -f <filename>\n", argv[0]);

    char* buf;
    if (filename)
    {
        //FILE *f = fopen(filename, "rb");
        //if (!f)
        //    EXIT("no such file: %s\n", filename);

        //fseek(f, 0, SEEK_END);
        //long size = ftell(f);
        //fseek(f, 0, SEEK_SET);
        //buf = (char*)malloc(size);
        //if (!buf)
        //    EXIT("couldn't allocate buf with size: %ld\n", size);

        //{
        //    size_t read = fread(buf, 1, size, f);
        //    if (read != size)
        //        EXIT("read %d instead of %d bytes from file\n", read, size);
        //}
        //fclose(f);
    }
    else if(ip)
    {
        fill_from_tcp(buf);
    }

    // plotting init
    int rc = initWp();
    assert(rc == 0);
    using point_t = Eigen::Vector3f;
    std::vector<point_t> vertices;
    std::vector<point_t> colors;

    for (int t=0;;++t)
    {
        bath_data_packet_t* bath = NULL;
        if (format == SBD)
        {
            fill_from_sbd(buf);
            SbdEntryHeader* header = (SbdEntryHeader*)buf;

            printf("header: type: 0x%02x relative_time: %d, absolute_time: %d %d, size: %d\n", header->entry_type, header->relative_time, header->absolute_time.tv_sec, header->absolute_time.tv_usec, header->entry_size);

            const char* data = buf + sizeof(*header);
            switch (header->entry_type)
            {
                case SbdEntryHeader::WBMS_BATH:
                {
                    bath = (bath_data_packet_t*)data;
                    assert(header->entry_size == 10352);
                    assert(header->entry_size == sizeof(*bath));


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
            //buf += sizeof(*header) + header->entry_size;
        }
        else
        {
            if (ip)
            {
                bath = (bath_data_packet_t*)buf;
            }
            else
            {
                bath = (bath_data_packet_t*)buf;
                //p += sizeof(*bath);
            }
        }

        if (bath != NULL)
        {
            assert(bath->header.preamble == 0xdeadbeef);
            printf("ping_number: %d, freq: %f, tx_angle: %f, swath_open: %f\n", bath->sub_header.ping_number, bath->sub_header.tx_freq, bath->sub_header.tx_angle, bath->sub_header.swath_open);
            assert(bath->sub_header.N == 512);
            assert(bath->sub_header.sample_rate == 78125.f);

            if (vertices.size() != bath->sub_header.N)
                vertices.resize(bath->sub_header.N);
            if (colors.size() != bath->sub_header.N)
                colors.resize(bath->sub_header.N);

            std::vector<float> range(bath->sub_header.N, 0.f);

            assert(sizeof(bath->header) + sizeof(bath->sub_header) + sizeof(detectionpoint_t) * bath->sub_header.N == sizeof(*bath));

            for (int index = 0; index < bath->sub_header.N; ++index)
            {
                printf("sample_number: %u %f %u %u %f %u %u %u\n", bath->dp[index].sample_number, bath->dp[index].angle, bath->dp[index].upper_gate, bath->dp[index].lower_gate, bath->dp[index].intensity, bath->dp[index].flags, bath->dp[index].quality_flags, bath->dp[index].quality_val);
                //for (char* i = (char*)(sample + index); i != (char*)(sample + index + 1); ++i)
                //    printf("%02x", *i);
                //printf("\n");
                //printf("%x\n", bath->dp[index].sample_number >> 24);
                range[index] = (bath->dp[index].sample_number * bath->sub_header.snd_velocity) / (2 * bath->sub_header.sample_rate);
                printf("range %d:\t%f\n", index, range[index]);

                point_t vertex;
                //float x = ((float)index / (float)bath->sub_header.N) * 2 - 1;
                //float y = (range[index] / 10.f) * 2 - 1;

                const float scale = .04;
                float x = cos(bath->dp[index].angle) * range[index] * scale;
                float y = sin(bath->dp[index].angle) * range[index] * scale;
                //printf("x: %f\ty: %f\n", x, y);

                //float x = cos(x + (float)t / 500.f);
                //float y = sin(x + (float)t / 500.f);
                vertex << y, x, -1;
                vertices.push_back(vertex);

                point_t color;
                color << 1, 1, 1;
                colors.push_back(color);
            }

            Eigen::Matrix3f projection;
            projection.setIdentity();

            updateWp(vertices, colors, projection);
            usleep(50000);

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
        }

        if (*p == '\0')
        {
            printf("done\n");
            break;
        }
    }


    free(buf);

    return 0;
}
