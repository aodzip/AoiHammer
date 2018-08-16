#include "AoiHammer.h"
#include "StorageStruct.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    srand(500);
    for (uint8_t i = 0; i < 128; i++)
    {
        ImageInfo *im = calloc(1, sizeof(ImageInfo));
        im->id = i;
        im->phash.hash = rand();
        insertIndex(&indexA[0], im);
    }
}