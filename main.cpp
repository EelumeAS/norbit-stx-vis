#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "openglWp.h"

#define EXIT(...) do { printf(__VA_ARGS__); exit(1); } while(true)

#include "navi.h"

int main(int argc, const char** argv)
{
    const char* filename;
    if (argc > 1)
        filename = argv[1];
    else
        filename = "in.sbd";

    FILE *f = fopen(filename, "rb");
    if (!f)
        EXIT("no such file: %s\n", filename);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size);
    if (!buf)
        EXIT("couldn't allocate buf with size: %ld\n", size);

    {
        size_t read = fread(buf, 1, size, f);
        if (read != size)
            EXIT("read %d instead of %d bytes from file\n", read, size);
    }
    fclose(f);

    // plotting init
    int rc = initWp();
    assert(rc == 0);
    using point_t = Eigen::Vector3f;
    std::vector<point_t> vertices;
    std::vector<point_t> colors;

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
                bath_data_packet_t* bath = (bath_data_packet_t*)data;
                assert(header->entry_size == 10352);

                assert(bath->header.preamble = 0xdeadbeef);
                printf("ping_number: %d, freq: %f\n", bath->sub_header.ping_number, bath->sub_header.tx_freq);
                assert(bath->sub_header.N == 512);
                assert(bath->sub_header.sample_rate == 78125.f);

                if (vertices.size() != bath->sub_header.N)
                    vertices.resize(bath->sub_header.N);
                if (colors.size() != bath->sub_header.N)
                    colors.resize(bath->sub_header.N);

                std::vector<float> range(bath->sub_header.N, 0.f);

                assert(sizeof(bath->header) + sizeof(bath->sub_header) + sizeof(detectionpoint_t) * bath->sub_header.N == header->entry_size);

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
                    float x = ((float)index / (float)bath->sub_header.N) * 2 - 1;
                    float y = (range[index] / 50.f) * 2 - 1;
                    //float y = ((float)(bath->dp[index].sample_number >> 24) / (float)0x20) * 2 - 1;
                    //float y = sin(x + (float)t / 500.f);
                    vertex << x, y, -1;
                    vertices.push_back(vertex);

                    point_t color;
                    color << 1, 1, 1;
                    colors.push_back(color);
                }

                Eigen::Matrix3f projection;
                projection.setIdentity();

                updateWp(vertices, colors, projection);
                usleep(20000);

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


    free(buf);

    return 0;
}
