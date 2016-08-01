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
    size_t width3 = (width+3)&~0x3;
    BITMAPFILEHEADER fhdr;
    memcpy(&fhdr.bfType, "BM", 2);
    fhdr.bfReserved1 = 0;
    fhdr.bfReserved2 = 0;
    fhdr.bfOffBits = sizeof(BITMAPFILEHEADER);
    fhdr.bfOffBits += sizeof(BITMAPINFOHEADER);
    fhdr.bfOffBits += 256*4;

    fhdr.bfSize = width3*height+fhdr.bfOffBits;
    BITMAPINFOHEADER ihdr;
    ihdr.biSize = sizeof(ihdr);
    ihdr.biWidth = width;
    ihdr.biHeight = biHeight;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 8;
    ihdr.biCompression = 0;
    ihdr.biSizeImage = 0; //width3*height;
    ihdr.biXPelsPerMeter = 0x1075;
    ihdr.biYPelsPerMeter = 0x1075;
    ihdr.biClrUsed = 256;
    ihdr.biClrImportant = 256;
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
    int l;
    size_t rbit, gbit, bbit, tidx=0;

    octree *p = root;

    for (l=7; !p->leaf; l--){

        rbit = (r>>l)&1;
        gbit = (g>>l)&1;
        bbit = (b>>l)&1;
        tidx = (rbit<<2)|(gbit<<1)|bbit;

        p = p->children[tidx];

        assert (p!=NULL);

        p->count++;
        p->colorR += r;
        p->colorG += g;
        p->colorB += b;
    }
    return p->index;
}

int addcolor(octree *root, size_t r, size_t g, size_t b)
{
    int l;
    size_t rbit, gbit, bbit, tidx=0;

    octree *p = root;

    for (l=7; !p->leaf; l--){

        rbit = (r>>l)&1;
        gbit = (g>>l)&1;
        bbit = (b>>l)&1;
        tidx = (rbit<<2)|(gbit<<1)|bbit;

        if (p->children[tidx] == NULL){
            octree *tree = new octree();
            memset(tree, 0, sizeof(octree));
            tree->leaf = (l==0);
            tree->level = l;
            if (tree->leaf){
                g_leafnodes[g_count] = tree;
                g_count++;
            }
            tree->parent = p;
            p->children[tidx] = tree;
        }

        p = p->children[tidx];
        p->count++;
        p->colorR += r;
        p->colorG += g;
        p->colorB += b;
    }

    while (g_count > 16){
        octree *tree = g_leafnodes[0]->parent;
        assert(tree);
        for (l=1; l<g_count; l++){
            octree *node = g_leafnodes[l]->parent;
            if (node == NULL)
                printf("%p %d %p\n", g_leafnodes[l], l, &g_root);
            assert(node);

            if (node->level > tree->level)
                continue;
            if (tree->count > node->count)
                tree = node;
        }
        assert(tree!=&g_root);
        size_t adjcnt = 0;
        for (l=0; l<g_count; l++){
            octree *node = g_leafnodes[l];
            if (node->parent == tree){
                delete node;
                continue;
            }
            g_leafnodes[adjcnt++] = g_leafnodes[l];
        }
        tree->leaf = 1;
        g_leafnodes[adjcnt++] = tree;
        for (l=0; l<8; l++)
            tree->children[l] = NULL;
        g_count = adjcnt;
    }

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

    g_root.level = 8;

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
    int line8 = (pbihdr->biWidth+3)&~0x3;
    doff = (char*)(buffer+pbfhdr->bfOffBits);
    line = (pbihdr->biWidth*3+3)&~0x3;

    for (i=0; i<height; i++){
        pd = doff;
        for (j=0; j<pbihdr->biWidth; j++){
            uint8_t *rgb = (uint8_t*)pd;
            opd[j] = mapcolor(&g_root, rgb[0], rgb[1], rgb[2]);
            pd += 3;
        }
        doff += line;
        opd += line8;
    }

    char bmpbuff[8192];
    count = bmp_init(pbihdr->biWidth, height, pbihdr->biHeight, bmpbuff);

    FILE *fpo = fopen(path, "wb");
    if (fpo != NULL){
        fwrite(bmpbuff, 1, count, fpo);
        fwrite(plane_buffer, 4, 256, fpo);
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
