#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

static size_t old = 0;
static size_t cmax = 0;
static size_t cclear = 256;

static size_t   bitmask = 0;
static size_t   bitsize = 9;

static char * dictp[4096];
static size_t dictl[4096];

static char output[1024*1024*3*3];
static char * outptr = output;

int lzw_init()
{
    int i;
    cclear  = 256;
    static char color[256];
    for (i=0; i<cclear; i++){
        dictp[i] = color+i;
        dictl[i] = 1;
        color[i] = i;
    }
    old = 0;
    bitsize = 9;
    outptr  = output;
    bitmask  = (1<<bitsize)-1;
    cmax = cclear+1;
    return cmax;
}

int lzw_code(size_t code)
{
    if (code > 4096){
        printf("%x %x %x %d %x\n", code, cmax, bitmask, bitsize, old);
    }
    assert(code <= 4096);
    if (code > cmax){
        printf("%d %d\n", cmax, code);
    }
    assert(code <= cmax);
    char *oldptr = outptr;
    if (code < cmax){
        size_t dl = dictl[code];
        memcpy(outptr, dictp[code], dl);
        dictl[cmax] = dictl[old]+1;
        char *p = outptr-dictl[old];
        //assert(!memcmp(p, dictp[old], dictl[old]));
        dictp[cmax] = p;
        outptr += dl;
    }else{
        size_t dl  = dictl[old];
        memcpy(outptr, dictp[old], dl);
        outptr[dl] = *outptr;
        dictp[cmax] = outptr;
        dictl[cmax] = dl+1;
        outptr += (dl+1);
    }

#if 0
    printf("------------------------\n");
    while (oldptr<outptr)
        printf("%02x ", 0xff&*oldptr++);
    printf("\n");
#endif
    old = code;
    return ++cmax;
}

int main(int argc, char *argv[])
{
    int i, j, count;
    uint8_t buffer[8192];

    int outbit_cnt = 0;
    uint32_t outbit_buff = 0;

    FILE *fout = fopen("output.orig", "wb");

    lzw_init();
    for (i=1; i<argc; i++){
        FILE *fp = fopen(argv[i], "rb");

        if (fp == NULL){
            continue;
        }

        while (!feof(fp)){ 
            count = fread(buffer, 1, sizeof(buffer), fp);
            for (j=0; j<count; j++){
                outbit_buff |= (buffer[j]<<outbit_cnt);
                outbit_cnt  += 8;

                while (outbit_cnt >= bitsize){
                    size_t code = bitmask&outbit_buff;
                    outbit_buff >>= bitsize;
                    outbit_cnt -= bitsize;

                    if (code == cclear){
                        fwrite(output, outptr-output, 1, fout);
                        lzw_init();
                        continue;
                    }
                    if (code == cclear+1){
                        printf("end stream!\n");
                        fwrite(output, outptr-output, 1, fout);
                        outptr = output;
                        break;
                    }
                    if (lzw_code(code) > bitmask){
                        if (bitsize < 12){
                            bitmask = bitmask*2+1;
                            bitsize++;
                        }
                    }
                }
            }
        }
        fclose(fp);
    }
    fwrite(output, outptr-output, 1, fout);
    outptr = output;
    fclose(fout);
    return 0;
}

