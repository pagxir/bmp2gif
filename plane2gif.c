#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef struct
{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

typedef struct
{
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

typedef struct gifScrDesc{
    uint16_t width;
    uint16_t depth;
    struct GlobalFlag{
        unsigned palBits: 3;
        unsigned sortFlag: 1;
        unsigned colorRes: 3;
        unsigned globalPal: 1;
    }__attribute__((packed))globalFlag;
    uint8_t backGround;
    uint8_t aspect;
}__attribute__((packed))GIFSCRDESC;

typedef struct gifImage{
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t depth;
    struct LocalFlag{
        unsigned palBits: 3;
        unsigned reserved: 2;
        unsigned sortFlag: 1;
        unsigned interlace: 1;
        unsigned localPal: 1;
    }__attribute__((packed))localFlag;
}__attribute__((packed))GIFIMAGE;

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
    int i;
    char buffer[256];
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
        buffer[0] = 0xff;
        for (i=0; i+255<=__outcnt; i+=255){
            memcpy(buffer+1, __outbuff+i, 255); 
            fwrite(buffer, 256, 1, fp);
        }
        memmove(__outbuff, __outbuff+i, __outcnt-i);
        __outcnt -= i;
    }
    if (mask < __dicode){
        ++__bitcnt;
    }
}

void finish(size_t code, FILE *fp)
{
    int i;

    output(code, fp);
    output(257, fp);
   
    if (__outbit_cnt > 0){
        char outch = (__outbit_buff&0xFF);
        __outbuff[__outcnt++] = outch;
    }
    __outbit_buff = 0;
    __outbit_cnt = 0;

    char buffer[256];
    size_t cpcnt = 0;
    for (i=0; i<__outcnt; i+=cpcnt){
        cpcnt = 0xff;
        if (cpcnt+i > __outcnt)
            cpcnt = __outcnt-i;
        buffer[0] = cpcnt;
        memcpy(buffer+1, __outbuff+i, cpcnt); 
        fwrite(buffer, cpcnt+1, 1, fp);
    }

    buffer[0] = 0;
    fwrite(buffer, 1, 1, fp);
    __outcnt = 0;
}

static char buffer[1024*1024*3*3];

int main(int argc, char *argv[])
{
    int v, u;
    char flag = 0;
    GIFSCRDESC desc;
    FILE *fpo = fopen("output.gif", "wb");

    fwrite("GIF89a", 1, 6, fpo);
    desc.width = 470;
    desc.depth = 353;
    desc.aspect = 0;
    desc.globalFlag.palBits = 7;
    desc.globalFlag.sortFlag = 0;
    desc.globalFlag.colorRes = 0;
    desc.globalFlag.globalPal = 0;
    assert(sizeof(desc)==7);
    fwrite(&desc, 1, 7, fpo);
    /* write plane */

    char lcb[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};
    fwrite(lcb, 1, sizeof(lcb), fpo);

    for (v=1; v<argc; v++){
        char gcb[] = {0x21, 0xf9, 0x04, 0x08, 0x7F, 0x00, 0x1f, 0x00};
        fwrite(gcb, 1, sizeof(gcb), fpo);

        flag = 0x2c;
        fwrite(&flag, 1, 1, fpo);

        FILE *fp = fopen(argv[v], "rb");
        int count = fread(buffer, 1, sizeof(buffer), fp);
        fclose(fp);

        BITMAPFILEHEADER *pbfhdr = NULL;
        pbfhdr = (BITMAPFILEHEADER*)buffer;
        BITMAPINFOHEADER *pbihdr = NULL;
        pbihdr = (BITMAPINFOHEADER*)(pbfhdr+1);
        assert(pbihdr->biBitCount==8);
        printf("Height: %d\n", pbihdr->biHeight);

        GIFIMAGE gifImage;
        gifImage.left = 0;
        gifImage.top = 0;
        gifImage.width = pbihdr->biWidth;
        gifImage.depth = abs(pbihdr->biHeight);
        gifImage.localFlag.localPal = 1;
        gifImage.localFlag.palBits = 7;
        gifImage.localFlag.interlace = 0;
        gifImage.localFlag.sortFlag = 0;
        gifImage.localFlag.reserved = 0;

        assert(9==sizeof(gifImage));
        fwrite(&gifImage, 1, sizeof(gifImage), fpo);

        char color[3];
        char *color_start = (char*)(pbihdr+1);
        for (u=0; u<256; u++){
            color[2] = *color_start++;
            color[1] = *color_start++;
            color[0] = *color_start++;
            fwrite(color, 1, 3, fpo);
            color_start++;
        }

        flag = 0x8;
        fwrite(&flag, 1, 1, fpo);

        int prefix = -1;

        restart();
        output(256, fpo);

        int off = 0;

        int i, j, depth, line;
        char *doff, *base = (char*)(buffer+pbfhdr->bfOffBits);
   
        if (pbihdr->biHeight < 0){
            line  = (pbihdr->biWidth+3)&~0x3;
            depth = -pbihdr->biHeight;
            doff  = base;
        }else{
            line = -((pbihdr->biWidth+3)&~0x3);
            depth = pbihdr->biHeight;
            doff = base-(depth*line)+line;
        }

        for (i=0; i<depth; i++){
            for (j=0; j<pbihdr->biWidth; j++){
                int code = 0xff&doff[j];
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
                output(prefix, fpo);
                if (update(prefix, code) < 4096){
                    prefix = code;
                    continue;
                }
                output(256, fpo);
                prefix = code;
                restart();
                off++;
            }
            doff += line;
        }
        finish(prefix, fpo);
    }
    flag = 0x3b;
    fwrite(&flag, 1, 1, fpo);
    fclose(fpo);
    return 0;
}
