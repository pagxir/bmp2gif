#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <map>
#define FI 4

typedef struct {
    size_t color;
    size_t count;
    size_t minidx;
    size_t mindiff;
}stdcolor;

typedef struct
{
    uint16_t   bfType;
    uint32_t   bfSize;
    uint16_t   bfReserved1;
    uint16_t   bfReserved2;
    uint32_t   bfOffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

typedef struct
{
    uint32_t     biSize;
    int32_t      biWidth;
    int32_t      biHeight;
    uint16_t     biPlanes;
    uint16_t     biBitCount;
    uint32_t     biCompression;
    uint32_t     biSizeImage;
    int32_t      biXPelsPerMeter;
    int32_t      biYPelsPerMeter;
    uint32_t     biClrUsed;
    uint32_t     biClrImportant;
} BITMAPINFOHEADER;

int compar(const void *a, const void *b)
{
    const stdcolor *pa, *pb;
    pa = (const stdcolor *)a;
    pb = (const stdcolor *)b;
    return pb->count - pa->count;
}

int bmp_init(size_t width, size_t height, int biHeight, char *bmpbuf)
{
    size_t width3 = (width+3)&~0x3;
    BITMAPFILEHEADER fhdr;
    memcpy(&fhdr.bfType, "BM", 2);
    fhdr.bfReserved1 = 0;
    fhdr.bfReserved2 = 0;
    fhdr.bfOffBits   = sizeof(BITMAPFILEHEADER);
    fhdr.bfOffBits  += sizeof(BITMAPINFOHEADER);
    fhdr.bfOffBits  += 256*4;

    fhdr.bfSize      = width3*height+fhdr.bfOffBits;
    BITMAPINFOHEADER ihdr;
    ihdr.biSize = sizeof(ihdr);
    ihdr.biWidth = width;
    ihdr.biHeight = biHeight;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 8;
    ihdr.biCompression = 0;
    ihdr.biSizeImage   = 0; //width3*height;
    ihdr.biXPelsPerMeter = 0x1075;
    ihdr.biYPelsPerMeter = 0x1075;
    ihdr.biClrUsed       = 256;
    ihdr.biClrImportant  = 256;
    memcpy(bmpbuf, &fhdr, sizeof(fhdr));
    memcpy(bmpbuf+sizeof(fhdr), &ihdr, sizeof(ihdr));
    return sizeof(fhdr)+sizeof(ihdr);
}

size_t colordiff(size_t c1, size_t c2)
{
    int i;
    size_t cc = 0;
    uint8_t *pc1 = (uint8_t*)&c1;
    uint8_t *pc2 = (uint8_t*)&c2;
    for (i=0; i<3; i++){
        if (pc1[i]>pc2[i])
            cc += pc1[i]-pc2[i];
        else
            cc += pc2[i]-pc1[i];
    }
    return cc;
}

char buffer[1024*1024*8];
char plane_buffer[1024*1024*3];

int doconvert(const char *path)
{
    FILE *fp = fopen(path, "rb");
    int count = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    BITMAPFILEHEADER *pbfhdr = NULL;
    pbfhdr = (BITMAPFILEHEADER*)buffer;
    BITMAPINFOHEADER *pbihdr = NULL;
    pbihdr = (BITMAPINFOHEADER*)(pbfhdr+1);

    assert(pbihdr->biBitCount==24);
    assert(pbihdr->biCompression==0);

    int i, j, line;
    size_t height = pbihdr->biHeight;
    char *pd, *doff = (char*)(buffer+pbfhdr->bfOffBits);
    line = (pbihdr->biWidth*3+3)&~0x3;
   
    std::map<size_t, size_t> colorstatics;
    for (i=0; i<height; i++){
        pd = doff;
        for (j=0; j<pbihdr->biWidth; j++){
            size_t temp = 0;
            memcpy(&temp, pd, 3);
            colorstatics[temp]++;
            pd += 3;
        }
        doff += line;
    }

    size_t ccount = colorstatics.size();
    printf("clorstatics: %d\n", ccount);

    size_t vj=0;
    stdcolor *color_sort = new stdcolor[ccount];
    std::map<size_t, size_t>::iterator iter;
    for (iter=colorstatics.begin(); iter!=colorstatics.end(); iter++){
        color_sort[vj].mindiff = 0x2FFF;
        color_sort[vj].minidx = ~0x0;
        color_sort[vj].color = iter->first;
        color_sort[vj].count = iter->second;
        vj ++;
    }

    qsort(color_sort, ccount, sizeof(stdcolor), compar);

    size_t jmax = 0;
    for (i=0; i<ccount; i++){
        stdcolor *ci, *cj;
        ci = &color_sort[i];
        size_t cdel =~0x0, mindiff=0x2FFF;
        for (j=0; j<jmax; j++){
            assert(i!=j);
            cj = &color_sort[j];
            size_t cdiff = colordiff(cj->color, ci->color);
            if (cdiff < cj->mindiff){
                cj->mindiff = cdiff;
                cj->minidx = i;
            }
            if (cdiff < ci->mindiff){
                ci->mindiff = cdiff;
                ci->minidx = j;
            }
            if ((cj->mindiff<<FI)+cj->count < mindiff){
                mindiff = (cj->mindiff<<FI)+cj->count;
                cdel = j;
            }
        }
        if ((ci->mindiff<<FI)+ci->count < mindiff){
            mindiff = (ci->mindiff<<FI)+ci->count;
            cdel = i;
        }
        jmax++;
        stdcolor *sci = &color_sort[i];
        if (jmax > 16){
            /* do color merge */
            ci = &color_sort[cdel];
            cj = &color_sort[ci->minidx];
            assert(ci != cj);
            cj->count += ci->count;

            size_t mergeto = ci->minidx;
            if (cdel >= 16){
                /* merge new color to old colors */
                if (cj->minidx == i){
                    cj->minidx = ~0x0;
                    cj->mindiff = 0xFFF;
                    for (j=0; j<16; j++){
                        if (j == mergeto)
                            continue;
                        ci = &color_sort[j];
                        size_t cdiff = colordiff(cj->color, ci->color);
                        if (cdiff < cj->mindiff){
                            cj->mindiff = cdiff;
                            cj->minidx = j;
                        }
                    }
                }
                /* corrent minidx refto cdel */
                for (j=0, jmax--; j<jmax; j++){
                    ci = &color_sort[j];
                    if (ci->minidx == cdel){
                        ci->mindiff = colordiff(ci->color, cj->color);
                        ci->minidx = mergeto;
                    }
                }
            }else if (mergeto >= 16){
                /* merge one old color to new color */
                memmove(ci, sci, sizeof(stdcolor));
                cj = &color_sort[cdel];
                /* correct minidx refto cdel */
                for (j=0, jmax--; j<jmax; j++){
                    if (j == cdel)
                        continue;
                    ci = &color_sort[j];
                    if (ci->minidx == cdel)
                        ci->mindiff = colordiff(ci->color, cj->color);
                    if (ci->minidx == mergeto)
                        ci->minidx = cdel;
                }
                if (cj->minidx == cdel){
                    cj->minidx = ~0x0;
                    cj->mindiff = 0xFFF;
                    for (j=0; j<jmax; j++){
                        if (j==cdel)
                            continue;
                        ci = &color_sort[j];
                        size_t cdiff = colordiff(cj->color, ci->color);
                        if (cdiff < cj->mindiff){
                            cj->mindiff = cdiff;
                            cj->minidx = j;
                        }
                    }
                }
            }else{
                /* merge one old color to another old color */
                memmove(ci, sci, sizeof(stdcolor));
                for (j=0, jmax--; j<jmax; j++){
                    ci = &color_sort[j];
                    /* corrent minidx refto cdel */
                    if (ci->minidx == cdel){
                        ci->mindiff = colordiff(ci->color, cj->color);
                        ci->minidx = mergeto;
                    }
                    /* corrent minidx refto i */
                    if (ci->minidx == i){
                        ci->minidx = cdel;
                        assert(j!=cdel);
                    }
                }
                cj = &color_sort[mergeto];
                if (cj->minidx == mergeto){
                    cj->minidx = ~0x0;
                    cj->mindiff = 0xFFF;
                    for (j=0; j<jmax; j++){
                        if (j == mergeto)
                            continue;
                        ci = &color_sort[j];
                        size_t cdiff = colordiff(cj->color, ci->color);
                        if (cdiff < cj->mindiff){
                            cj->mindiff = cdiff;
                            cj->minidx = j;
                        }
                    }
                }
            }
        }
    }

    printf("dump to bitmap file!\n");

    size_t planes[256];
    for (i=0; i<256; i++){
        planes[i] = color_sort[i].color;
    }
  
    std::map<size_t, size_t> *list = &colorstatics;
    for (iter=list->begin(); iter!=list->end(); iter++){
        size_t minidx = ~0x0;
        size_t mindiff  = 0x3FF;

        size_t color = iter->first;
        for (i=0; i<16; i++){
            size_t cdiff = colordiff(color, color_sort[i].color);
            if (cdiff < mindiff){
                mindiff = cdiff;
                minidx = i;
            }
        }
        iter->second = minidx;
    }

    size_t cc = bmp_init(pbihdr->biWidth, height, pbihdr->biHeight, plane_buffer);

    FILE *fpo = fopen(path, "wb");
    fwrite(plane_buffer, cc, 1, fpo);
    fwrite(planes, 256, 4, fpo);

    size_t line1 = (pbihdr->biWidth+3)&~0x3;
    doff = (char*)(buffer+pbfhdr->bfOffBits);
    for (i=0; i<height; i++){
        pd = doff;
        uint8_t code = 0;
        for (j=0; j<pbihdr->biWidth; j++){
            size_t temp = 0;
            memcpy(&temp, pd, 3);
            code = colorstatics[temp];
            fwrite(&code, 1, 1, fpo);
            pd += 3;
        }
        fwrite(pd, line1-j, 1, fpo);
        doff += line;
    }
    fclose(fpo);
    return 0;
}

int main(int argc, char *argv[])
{
    for (int i=1; i<argc; i++)
        doconvert(argv[1]);
    return 0;
}
