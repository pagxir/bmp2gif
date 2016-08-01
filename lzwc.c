#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

static int __bitcnt = 9;
static int __dicode = 256+2;
static int __dictbl[4096][256];

inline void restart()
{
    __bitcnt = 9;
    __dicode = 256+2;
    memset(__dictbl, 0xFF, sizeof(__dictbl));
}

inline int find(int prefix, int code)
{
    return __dictbl[prefix][code];
}

inline int update(int prefix, int code)
{
    __dictbl[prefix][code] = __dicode++;
    return __dicode;
}

static int __outcnt = 0;
static char __outbuff[8192+4];

static int __outbit_cnt = 0;
static uint32_t __outbit_buff = 0;

void output(size_t code, FILE *fp)
{
    size_t mask = (1<<__bitcnt)-1;

    __outbit_buff |= ((code&mask)<<__outbit_cnt);
    __outbit_cnt  += __bitcnt;

    while (__outbit_cnt >= 8){
        char outch = (__outbit_buff&0xFF);
        __outbuff[__outcnt++] = outch;
        __outbit_buff >>= 8;
        __outbit_cnt -= 8;
    }
    if (__outcnt >= 8192){
        fwrite(__outbuff, 1, __outcnt, fp);
        __outcnt = 0;
    }
    if (mask < __dicode){
        ++__bitcnt;
    }
}

void finish(size_t code, FILE *fp)
{
    output(code, fp);
    output(257, fp);
    output(0, fp);
    fwrite(__outbuff, 1, __outcnt, fp);
    __outcnt = 0;
}

int main(int argc, char *argv[])
{
    int i, j, count;
    uint8_t buffer[8192];

    int prefix = -1;

    restart();

    FILE *fout = fopen("output.lzw", "wb");
    output(256, fout);
    for (i=1; i<argc; i++){
        FILE *fp = fopen(argv[i], "rb");

        if (fp == NULL){
            continue;
        }

        while (!feof(fp)){ 
            count = fread(buffer, 1, sizeof(buffer), fp);
            for (j=0; j<count; j++){
                int code = buffer[j];
                if (prefix == -1){
                    prefix = code;
                    continue;
                }
                int prefix1 = find(prefix, code);
                if (prefix1 != -1){
                    assert(prefix1<=__dicode);
                    prefix = prefix1;
                    continue;
                }
                output(prefix, fout);
                if (update(prefix, code)<4096){
                    prefix = code;
                    continue;
                }
                output(256, fout);
                prefix = code;
                restart();
            }
        }
        fclose(fp);
    }
    finish(prefix, fout);
    fclose(fout);
    return 0;
}
