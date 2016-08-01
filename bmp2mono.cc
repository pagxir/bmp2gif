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

int bmp_init(size_t width, size_t height, int biHeight, char *bmpbuf)
{
    size_t width3 = ((width+7)/8+3)&~0x3;
    BITMAPFILEHEADER fhdr;
    memcpy(&fhdr.bfType, "BM", 2);
    fhdr.bfReserved1 = 0;
    fhdr.bfReserved2 = 0;
    fhdr.bfOffBits = sizeof(BITMAPFILEHEADER);
    fhdr.bfOffBits += sizeof(BITMAPINFOHEADER);
    fhdr.bfOffBits += 2*4;

    fhdr.bfSize = width3*height+fhdr.bfOffBits;
    BITMAPINFOHEADER ihdr;
    ihdr.biSize = sizeof(ihdr);
    ihdr.biWidth = width;
    ihdr.biHeight = biHeight;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 1;
    ihdr.biCompression = 0;
    ihdr.biSizeImage = width3*height;
    ihdr.biXPelsPerMeter = 0x1075;
    ihdr.biYPelsPerMeter = 0x1075;
    ihdr.biClrUsed = 2;
    ihdr.biClrImportant = 2;
    memcpy(bmpbuf, &fhdr, sizeof(fhdr));
    memcpy(bmpbuf+sizeof(fhdr), &ihdr, sizeof(ihdr));
    return sizeof(fhdr)+sizeof(ihdr);
}


typedef struct _octree{
    size_t index;

    size_t leaf;
    size_t level;

    size_t count;
    size_t colorR, colorG, colorB;

    struct _octree *parent;
    struct _octree *children[8];
}octree;

static octree g_root;
static size_t g_count = 0;
static octree *g_leafnodes[257];

int clrcolor(octree *root)
{
    for (int i=0; i<8; i++){
        octree *node = root->children[i];
        if (node == NULL)
            continue;
        clrcolor(node);
        delete node;
    }
    memset(root, 0, sizeof(octree));
    g_count = 0;
    return 0;
}

int mapcolor(octree *root, size_t r, size_t g, size_t b)
{
    octree *p = root;
    static size_t rr = (p->colorR/p->count);
    static size_t rg = (p->colorG/p->count);
    static size_t rb = (p->colorB/p->count);
    static float ry = 0.257*rr+0.504*rg+0.098*rb;
    float cy = 0.257*r+0.504*g+0.098*b;
    return (1.23*cy)>ry;
}

int addcolor(octree *root, size_t r, size_t g, size_t b)
{
    octree *p = root;
    p->colorR += r;
    p->colorG += g;
    p->colorB += b;
    p->count++;
    return 0;
}

char buffer[1024*1024*3*3];
char plane_buffer[1024*1024];

char outbuff[1024*1024*3];

int doconvert(const char *path)
{
    FILE *fp = fopen(path, "rb");
    int count = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    BITMAPFILEHEADER *pbfhdr = NULL;
    pbfhdr = (BITMAPFILEHEADER*)buffer;
    BITMAPINFOHEADER *pbihdr = NULL;
    pbihdr = (BITMAPINFOHEADER*)(pbfhdr+1);

    assert(pbihdr->biBitCount == 24);
    assert(pbihdr->biCompression == 0);

    int i, j, line, height;
    char *pd, *doff = (char*)(buffer+pbfhdr->bfOffBits);
    line = (pbihdr->biWidth*3+3)&~0x3;

    height = -pbihdr->biHeight;
    if (pbihdr->biHeight > 0){
        height = pbihdr->biHeight;
        doff += (line*height-line);
        line = -line;
    }

    for (i=0; i<height; i++){
        pd = doff;
        for (j=0; j<pbihdr->biWidth; j++){
            uint8_t *rgb = (uint8_t*)pd;
            addcolor(&g_root, rgb[0], rgb[1], rgb[2]);
            pd += 3;
        }
        doff += line;
    }

    char *plane = plane_buffer;
    for (i=0; i<g_count; i++){
        octree *p = g_leafnodes[i];
        *plane++ = p->colorR/p->count;
        *plane++ = p->colorG/p->count;
        *plane++ = p->colorB/p->count;
        assert(p->leaf);
        p->index = i;
        plane++;
    }

    char *opd = outbuff;
    int line8 = ((pbihdr->biWidth+7)/8+3)&~0x3;
    doff = (char*)(buffer+pbfhdr->bfOffBits);
    line = (pbihdr->biWidth*3+3)&~0x3;

    size_t _cnts[2], _rgbs[3][2];

    memset(_cnts, 0, sizeof(_cnts));
    memset(_rgbs, 0, sizeof(_rgbs));

    size_t bitcnt = 0;
    size_t bitbuf = 0;

    char *opdbase = NULL;
    for (i=0; i<height; i++){
        pd = doff;
        opdbase = opd;
        for (j=0; j<pbihdr->biWidth; j++){
            uint8_t *rgb = (uint8_t*)pd;
            int ij = mapcolor(&g_root, rgb[0], rgb[1], rgb[2]);
            _cnts[ij] ++;
            for (int u=0; u<3; u++)
                _rgbs[u][ij] += rgb[u];
            pd += 3;

            bitbuf <<= 1;
            bitbuf |= (ij&1);
            bitcnt++;

            while (bitcnt >= 8){
                *opdbase++ = bitbuf&0xFF;
                bitbuf >>= 8;
                bitcnt -= 8;
            }
        }
        if (bitcnt > 0){
            *opdbase++ = bitbuf<<(8-bitcnt);
            bitcnt = 0;
        }
        doff += line;
        opd += line8;
    }

    for (i=0; i<3; i++)
        plane_buffer[i] = _rgbs[i][0]/_cnts[0];

    for (i=0; i<3; i++)
        plane_buffer[4+i] = _rgbs[i][1]/_cnts[1];


    char bmpbuff[8192];
    count = bmp_init(pbihdr->biWidth, height, pbihdr->biHeight, bmpbuff);

    FILE *fpo = fopen(path, "wb");
    if (fpo != NULL){
        fwrite(bmpbuff, 1, count, fpo);
        fwrite(plane_buffer, 4, 2, fpo);
        fwrite(outbuff, line8, height, fpo);
        fclose(fpo);
    }

    clrcolor(&g_root);
    return 0;
}

int main(int argc, char *argv[])
{
    for (int i=1; i<argc; i++)
        doconvert(argv[i]);
    return 0;
}
